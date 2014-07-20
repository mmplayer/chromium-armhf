// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webrequest_api.h"

#include <algorithm>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_id_map.h"
#include "chrome/browser/extensions/extension_webrequest_api_constants.h"
#include "chrome/browser/extensions/extension_webrequest_time_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_message_filter.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "net/base/auth.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"

namespace keys = extension_webrequest_api_constants;

namespace {

// List of all the webRequest events.
static const char* const kWebRequestEvents[] = {
  keys::kOnBeforeRedirect,
  keys::kOnBeforeRequest,
  keys::kOnBeforeSendHeaders,
  keys::kOnCompleted,
  keys::kOnErrorOccurred,
  keys::kOnSendHeaders,
  keys::kOnAuthRequired,
  keys::kOnResponseStarted,
  keys::kOnHeadersReceived,
};

static const char* kResourceTypeStrings[] = {
  "main_frame",
  "sub_frame",
  "stylesheet",
  "script",
  "image",
  "object",
  "xmlhttprequest",
  "other",
  "other",
};

static ResourceType::Type kResourceTypeValues[] = {
  ResourceType::MAIN_FRAME,
  ResourceType::SUB_FRAME,
  ResourceType::STYLESHEET,
  ResourceType::SCRIPT,
  ResourceType::IMAGE,
  ResourceType::OBJECT,
  ResourceType::XHR,
  ResourceType::LAST_TYPE,  // represents "other"
  // TODO(jochen): We duplicate the last entry, so the array's size is not a
  // power of two. If it is, this triggers a bug in gcc 4.4 in Release builds
  // (http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43949). Once we use a version
  // of gcc with this bug fixed, or the array is changed so this duplicate
  // entry is no longer required, this should be removed.
  ResourceType::LAST_TYPE,
};

COMPILE_ASSERT(
    arraysize(kResourceTypeStrings) == arraysize(kResourceTypeValues),
    keep_resource_types_in_sync);

#define ARRAYEND(array) (array + arraysize(array))

// NetLog parameter to indicate the ID of the extension that caused an event.
class NetLogExtensionIdParameter : public net::NetLog::EventParameters {
 public:
  explicit NetLogExtensionIdParameter(const std::string& extension_id)
      : extension_id_(extension_id) {}
  virtual ~NetLogExtensionIdParameter() {}

  virtual base::Value* ToValue() const OVERRIDE {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetString("extension_id", extension_id_);
    return dict;
  }

 private:
  const std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(NetLogExtensionIdParameter);
};

// NetLog parameter to indicate that an extension modified a request.
class NetLogModificationParameter : public NetLogExtensionIdParameter {
 public:
  explicit NetLogModificationParameter(const std::string& extension_id)
      : NetLogExtensionIdParameter(extension_id) {}
  virtual ~NetLogModificationParameter() {}

  virtual base::Value* ToValue() const OVERRIDE {
    Value* parent = NetLogExtensionIdParameter::ToValue();
    DCHECK(parent->IsType(Value::TYPE_DICTIONARY));
    DictionaryValue* dict = static_cast<DictionaryValue*>(parent);
    dict->Set("modified_headers", modified_headers_.DeepCopy());
    dict->Set("deleted_headers", deleted_headers_.DeepCopy());
    return dict;
  }

  void DeletedHeader(const std::string& key) {
    deleted_headers_.Append(Value::CreateStringValue(key));
  }

  void ModifiedHeader(const std::string& key, const std::string& value) {
    modified_headers_.Append(Value::CreateStringValue(key + ": " + value));
  }

 private:
  ListValue modified_headers_;
  ListValue deleted_headers_;

  DISALLOW_COPY_AND_ASSIGN(NetLogModificationParameter);
};

// Returns the frame ID as it will be passed to the extension:
// 0 if the navigation happens in the main frame, or the frame ID
// modulo 32 bits otherwise.
// Keep this in sync with the GetFrameId() function in
// extension_webnavigation_api.cc.
int GetFrameId(bool is_main_frame, int64 frame_id) {
  return is_main_frame ? 0 : static_cast<int>(frame_id);
}

bool IsWebRequestEvent(const std::string& event_name) {
  return std::find(kWebRequestEvents, ARRAYEND(kWebRequestEvents),
                   event_name) != ARRAYEND(kWebRequestEvents);
}

// Returns true if the scheme is one we want to allow extensions to have access
// to. Extensions still need specific permissions for a given URL, which is
// covered by CanExtensionAccessURL.
bool HasWebRequestScheme(const GURL& url) {
  return (url.SchemeIs(chrome::kAboutScheme) ||
          url.SchemeIs(chrome::kFileScheme) ||
          url.SchemeIs(chrome::kFtpScheme) ||
          url.SchemeIs(chrome::kHttpScheme) ||
          url.SchemeIs(chrome::kHttpsScheme) ||
          url.SchemeIs(chrome::kExtensionScheme));
}

bool CanExtensionAccessURL(const Extension* extension, const GURL& url) {
  // about: URLs are not covered in host permissions, but are allowed anyway.
  return (url.SchemeIs(chrome::kAboutScheme) ||
          extension->HasHostPermission(url) ||
          url.GetOrigin() == extension->url());
}

const char* ResourceTypeToString(ResourceType::Type type) {
  ResourceType::Type* iter =
      std::find(kResourceTypeValues, ARRAYEND(kResourceTypeValues), type);
  if (iter == ARRAYEND(kResourceTypeValues))
    return "other";

  return kResourceTypeStrings[iter - kResourceTypeValues];
}

bool ParseResourceType(const std::string& type_str,
                       ResourceType::Type* type) {
  const char** iter =
      std::find(kResourceTypeStrings, ARRAYEND(kResourceTypeStrings), type_str);
  if (iter == ARRAYEND(kResourceTypeStrings))
    return false;
  *type = kResourceTypeValues[iter - kResourceTypeStrings];
  return true;
}

void ExtractRequestInfoDetails(net::URLRequest* request,
                               bool* is_main_frame,
                               int64* frame_id,
                               int* tab_id,
                               int* window_id,
                               ResourceType::Type* resource_type) {
  if (!request->GetUserData(NULL))
    return;

  ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request);
  ExtensionTabIdMap::GetInstance()->GetTabAndWindowId(
      info->child_id(), info->route_id(), tab_id, window_id);
  *frame_id = info->frame_id();
  *is_main_frame = info->is_main_frame();

  // Restrict the resource type to the values we care about.
  ResourceType::Type* iter =
      std::find(kResourceTypeValues, ARRAYEND(kResourceTypeValues),
                info->resource_type());
  *resource_type = (iter != ARRAYEND(kResourceTypeValues)) ?
      *iter : ResourceType::LAST_TYPE;
}

// Extracts from |request| information for the keys requestId, url, method,
// frameId, tabId, type, and timeStamp and writes these into |out| to be passed
// on to extensions.
void ExtractRequestInfo(net::URLRequest* request, DictionaryValue* out) {
  bool is_main_frame = false;
  int64 frame_id = -1;
  int frame_id_for_extension = -1;
  int tab_id = -1;
  int window_id = -1;
  ResourceType::Type resource_type = ResourceType::LAST_TYPE;
  ExtractRequestInfoDetails(request, &is_main_frame, &frame_id, &tab_id,
                            &window_id, &resource_type);
  frame_id_for_extension = GetFrameId(is_main_frame, frame_id);

  out->SetString(keys::kRequestIdKey,
                 base::Uint64ToString(request->identifier()));
  out->SetString(keys::kUrlKey, request->url().spec());
  out->SetString(keys::kMethodKey, request->method());
  out->SetInteger(keys::kFrameIdKey, frame_id_for_extension);
  out->SetInteger(keys::kTabIdKey, tab_id);
  out->SetString(keys::kTypeKey, ResourceTypeToString(resource_type));
  out->SetDouble(keys::kTimeStampKey, base::Time::Now().ToDoubleT() * 1000);
}

// Creates a list of HttpHeaders (see extension_api.json). If |headers| is
// NULL, the list is empty. Ownership is passed to the caller.
ListValue* GetResponseHeadersList(const net::HttpResponseHeaders* headers) {
  ListValue* headers_value = new ListValue();
  if (headers) {
    void* iter = NULL;
    std::string name;
    std::string value;
    while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
      DictionaryValue* header = new DictionaryValue();
      header->SetString(keys::kHeaderNameKey, name);
      header->SetString(keys::kHeaderValueKey, value);
      headers_value->Append(header);
    }
  }
  return headers_value;
}

ListValue* GetRequestHeadersList(const net::HttpRequestHeaders& headers) {
  ListValue* headers_value = new ListValue();
  for (net::HttpRequestHeaders::Iterator it(headers); it.GetNext(); ) {
    DictionaryValue* header = new DictionaryValue();
    header->SetString(keys::kHeaderNameKey, it.name());
    header->SetString(keys::kHeaderValueKey, it.value());
    headers_value->Append(header);
  }
  return headers_value;
}

// Creates a StringValue with the status line of |headers|. If |headers| is
// NULL, an empty string is returned.  Ownership is passed to the caller.
StringValue* GetStatusLine(net::HttpResponseHeaders* headers) {
  return new StringValue(headers ? headers->GetStatusLine() : "");
}

// Comparison operator that returns true if the extension that caused
// |a| was installed after the extension that caused |b|.
bool InDecreasingExtensionInstallationTimeOrder(
    const linked_ptr<ExtensionWebRequestEventRouter::EventResponseDelta>& a,
    const linked_ptr<ExtensionWebRequestEventRouter::EventResponseDelta>& b) {
  return a->extension_install_time > b->extension_install_time;
}

void NotifyWebRequestAPIUsed(void* profile_id, const Extension* extension) {
  Profile* profile = reinterpret_cast<Profile*>(profile_id);
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return;

  if (profile->GetExtensionService()->HasUsedWebRequest(extension))
    return;
  profile->GetExtensionService()->SetHasUsedWebRequest(extension, true);

  content::BrowserContext* browser_context = profile;
  for (RenderProcessHost::iterator it = RenderProcessHost::AllHostsIterator();
       !it.IsAtEnd(); it.Advance()) {
    RenderProcessHost* host = it.GetCurrentValue();
    if (host->browser_context() == browser_context)
      SendExtensionWebRequestStatusToHost(host);
  }
}

}  // namespace

// Represents a single unique listener to an event, along with whatever filter
// parameters and extra_info_spec were specified at the time the listener was
// added.
struct ExtensionWebRequestEventRouter::EventListener {
  std::string extension_id;
  std::string extension_name;
  std::string sub_event_name;
  RequestFilter filter;
  int extra_info_spec;
  base::WeakPtr<IPC::Message::Sender> ipc_sender;
  mutable std::set<uint64> blocked_requests;

  // Comparator to work with std::set.
  bool operator<(const EventListener& that) const {
    if (extension_id < that.extension_id)
      return true;
    if (extension_id == that.extension_id &&
        sub_event_name < that.sub_event_name)
      return true;
    return false;
  }

  EventListener() : extra_info_spec(0) {}
};

// Contains info about requests that are blocked waiting for a response from
// an extension.
struct ExtensionWebRequestEventRouter::BlockedRequest {
  // The request that is being blocked.
  net::URLRequest* request;

  // The event that we're currently blocked on.
  EventTypes event;

  // The number of event handlers that we are awaiting a response from.
  int num_handlers_blocking;

  // Pointer to NetLog to report significant changes to the request for
  // debugging.
  const net::BoundNetLog* net_log;

  // The callback to call when we get a response from all event handlers.
  net::OldCompletionCallback* callback;

  // If non-empty, this contains the new URL that the request will redirect to.
  // Only valid for OnBeforeRequest.
  GURL* new_url;

  // The request headers that will be issued along with this request. Only valid
  // for OnBeforeSendHeaders.
  net::HttpRequestHeaders* request_headers;

  // The response headers that were received from the server. Only valid for
  // OnHeadersReceived.
  scoped_refptr<net::HttpResponseHeaders> original_response_headers;

  // Location where to override response headers. Only valid for
  // OnHeadersReceived.
  scoped_refptr<net::HttpResponseHeaders>* override_response_headers;

  // If non-empty, this contains the auth credentials that may be filled in.
  // Only valid for OnAuthRequired.
  net::AuthCredentials* auth_credentials;

  // The callback to invoke for auth. If |auth_callback.is_null()| is false,
  // |callback| must be NULL.
  // Only valid for OnAuthRequired.
  net::NetworkDelegate::AuthCallback auth_callback;

  // Time the request was paused. Used for logging purposes.
  base::Time blocking_time;

  // Changes requested by extensions.
  EventResponseDeltas response_deltas;

  BlockedRequest()
      : request(NULL),
        event(kInvalidEvent),
        num_handlers_blocking(0),
        net_log(NULL),
        callback(NULL),
        new_url(NULL),
        request_headers(NULL),
        override_response_headers(NULL),
        auth_credentials(NULL) {}
};

bool ExtensionWebRequestEventRouter::RequestFilter::InitFromValue(
    const DictionaryValue& value, std::string* error) {
  for (DictionaryValue::key_iterator key = value.begin_keys();
       key != value.end_keys(); ++key) {
    if (*key == "urls") {
      ListValue* urls_value = NULL;
      if (!value.GetList("urls", &urls_value))
        return false;
      for (size_t i = 0; i < urls_value->GetSize(); ++i) {
        std::string url;
        URLPattern pattern(URLPattern::SCHEME_ALL);
        if (!urls_value->GetString(i, &url) ||
            pattern.Parse(url, URLPattern::ERROR_ON_PORTS) !=
                URLPattern::PARSE_SUCCESS) {
          *error = ExtensionErrorUtils::FormatErrorMessage(
              keys::kInvalidRequestFilterUrl, url);
          return false;
        }
        urls.AddPattern(pattern);
      }
    } else if (*key == "types") {
      ListValue* types_value = NULL;
      if (!value.GetList("types", &types_value))
        return false;
      for (size_t i = 0; i < types_value->GetSize(); ++i) {
        std::string type_str;
        ResourceType::Type type;
        if (!types_value->GetString(i, &type_str) ||
            !ParseResourceType(type_str, &type))
          return false;
        types.push_back(type);
      }
    } else if (*key == "tabId") {
      if (!value.GetInteger("tabId", &tab_id))
        return false;
    } else if (*key == "windowId") {
      if (!value.GetInteger("windowId", &window_id))
        return false;
    } else {
      return false;
    }
  }
  return true;
}

// static
bool ExtensionWebRequestEventRouter::ExtraInfoSpec::InitFromValue(
    const ListValue& value, int* extra_info_spec) {
  *extra_info_spec = 0;
  for (size_t i = 0; i < value.GetSize(); ++i) {
    std::string str;
    if (!value.GetString(i, &str))
      return false;

    if (str == "requestHeaders")
      *extra_info_spec |= REQUEST_HEADERS;
    else if (str == "responseHeaders")
      *extra_info_spec |= RESPONSE_HEADERS;
    else if (str == "blocking")
      *extra_info_spec |= BLOCKING;
    else
      return false;
  }
  return true;
}


ExtensionWebRequestEventRouter::EventResponse::EventResponse(
    const std::string& extension_id, const base::Time& extension_install_time)
    : extension_id(extension_id),
      extension_install_time(extension_install_time),
      cancel(false) {
}

ExtensionWebRequestEventRouter::EventResponse::~EventResponse() {
}

ExtensionWebRequestEventRouter::EventResponseDelta::EventResponseDelta(
    const std::string& extension_id, const base::Time& extension_install_time)
    : extension_id(extension_id),
      extension_install_time(extension_install_time),
      cancel(false) {
}

ExtensionWebRequestEventRouter::EventResponseDelta::~EventResponseDelta() {
}


ExtensionWebRequestEventRouter::RequestFilter::RequestFilter()
    : tab_id(-1), window_id(-1) {
}

ExtensionWebRequestEventRouter::RequestFilter::~RequestFilter() {
}

//
// ExtensionWebRequestEventRouter
//

// static
ExtensionWebRequestEventRouter* ExtensionWebRequestEventRouter::GetInstance() {
  return Singleton<ExtensionWebRequestEventRouter>::get();
}

ExtensionWebRequestEventRouter::ExtensionWebRequestEventRouter()
    : request_time_tracker_(new ExtensionWebRequestTimeTracker) {
}

ExtensionWebRequestEventRouter::~ExtensionWebRequestEventRouter() {
}

int ExtensionWebRequestEventRouter::OnBeforeRequest(
    void* profile,
    ExtensionInfoMap* extension_info_map,
    net::URLRequest* request,
    net::OldCompletionCallback* callback,
    GURL* new_url) {
  // TODO(jochen): Figure out what to do with events from the system context.
  if (!profile)
    return net::OK;

  if (!HasWebRequestScheme(request->url()))
    return net::OK;

  request_time_tracker_->LogRequestStartTime(request->identifier(),
                                             base::Time::Now(),
                                             request->url(),
                                             profile);

  int extra_info_spec = 0;
  std::vector<const EventListener*> listeners =
      GetMatchingListeners(profile, extension_info_map, keys::kOnBeforeRequest,
                           request, &extra_info_spec);
  if (listeners.empty())
    return net::OK;

  if (GetAndSetSignaled(request->identifier(), kOnBeforeRequest))
    return net::OK;

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  ExtractRequestInfo(request, dict);
  args.Append(dict);

  if (DispatchEvent(profile, request, listeners, args)) {
    blocked_requests_[request->identifier()].event = kOnBeforeRequest;
    blocked_requests_[request->identifier()].callback = callback;
    blocked_requests_[request->identifier()].new_url = new_url;
    blocked_requests_[request->identifier()].net_log = &request->net_log();
    return net::ERR_IO_PENDING;
  }
  return net::OK;
}

int ExtensionWebRequestEventRouter::OnBeforeSendHeaders(
    void* profile,
    ExtensionInfoMap* extension_info_map,
    net::URLRequest* request,
    net::OldCompletionCallback* callback,
    net::HttpRequestHeaders* headers) {
  // TODO(jochen): Figure out what to do with events from the system context.
  if (!profile)
    return net::OK;

  if (!HasWebRequestScheme(request->url()))
    return net::OK;

  if (GetAndSetSignaled(request->identifier(), kOnBeforeSendHeaders))
    return net::OK;

  int extra_info_spec = 0;
  std::vector<const EventListener*> listeners =
      GetMatchingListeners(profile, extension_info_map,
                           keys::kOnBeforeSendHeaders, request,
                           &extra_info_spec);
  if (listeners.empty())
    return net::OK;

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  ExtractRequestInfo(request, dict);
  if (extra_info_spec & ExtraInfoSpec::REQUEST_HEADERS)
    dict->Set(keys::kRequestHeadersKey, GetRequestHeadersList(*headers));
  args.Append(dict);

  if (DispatchEvent(profile, request, listeners, args)) {
    blocked_requests_[request->identifier()].event = kOnBeforeSendHeaders;
    blocked_requests_[request->identifier()].callback = callback;
    blocked_requests_[request->identifier()].request_headers = headers;
    blocked_requests_[request->identifier()].net_log = &request->net_log();
    return net::ERR_IO_PENDING;
  }
  return net::OK;
}

void ExtensionWebRequestEventRouter::OnSendHeaders(
    void* profile,
    ExtensionInfoMap* extension_info_map,
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers) {
  if (!profile)
    return;

  if (!HasWebRequestScheme(request->url()))
    return;

  if (GetAndSetSignaled(request->identifier(), kOnSendHeaders))
    return;

  ClearSignaled(request->identifier(), kOnBeforeRedirect);

  int extra_info_spec = 0;
  std::vector<const EventListener*> listeners =
      GetMatchingListeners(profile, extension_info_map,
                           keys::kOnSendHeaders, request, &extra_info_spec);
  if (listeners.empty())
    return;

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  ExtractRequestInfo(request, dict);
  if (extra_info_spec & ExtraInfoSpec::REQUEST_HEADERS)
    dict->Set(keys::kRequestHeadersKey, GetRequestHeadersList(headers));
  args.Append(dict);

  DispatchEvent(profile, request, listeners, args);
}

int ExtensionWebRequestEventRouter::OnHeadersReceived(
    void* profile,
    ExtensionInfoMap* extension_info_map,
    net::URLRequest* request,
    net::OldCompletionCallback* callback,
    net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers) {
  if (!profile)
    return net::OK;

  if (!HasWebRequestScheme(request->url()))
    return net::OK;

  int extra_info_spec = 0;
  std::vector<const EventListener*> listeners =
      GetMatchingListeners(profile, extension_info_map,
                           keys::kOnHeadersReceived, request,
                           &extra_info_spec);

  if (listeners.empty())
    return net::OK;

  if (GetAndSetSignaled(request->identifier(), kOnHeadersReceived))
    return net::OK;

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  ExtractRequestInfo(request, dict);
  dict->SetString(keys::kStatusLineKey,
      original_response_headers->GetStatusLine());
  if (extra_info_spec & ExtraInfoSpec::RESPONSE_HEADERS) {
    dict->Set(keys::kResponseHeadersKey,
        GetResponseHeadersList(original_response_headers));
  }
  args.Append(dict);

  if (DispatchEvent(profile, request, listeners, args)) {
    blocked_requests_[request->identifier()].event = kOnHeadersReceived;
    blocked_requests_[request->identifier()].callback = callback;
    blocked_requests_[request->identifier()].net_log = &request->net_log();
    blocked_requests_[request->identifier()].override_response_headers =
        override_response_headers;
    blocked_requests_[request->identifier()].original_response_headers =
        original_response_headers;
    return net::ERR_IO_PENDING;
  }
  return net::OK;
}

net::NetworkDelegate::AuthRequiredResponse
ExtensionWebRequestEventRouter::OnAuthRequired(
    void* profile,
    ExtensionInfoMap* extension_info_map,
    net::URLRequest* request,
    const net::AuthChallengeInfo& auth_info,
    const net::NetworkDelegate::AuthCallback& callback,
    net::AuthCredentials* credentials) {
  // No profile means that this is for authentication challenges in the
  // system context. Skip in that case.
  if (!profile)
    return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;

  if (!HasWebRequestScheme(request->url()))
    return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;

  int extra_info_spec = 0;
  std::vector<const EventListener*> listeners =
      GetMatchingListeners(profile, extension_info_map,
                           keys::kOnAuthRequired, request, &extra_info_spec);
  if (listeners.empty())
    return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  ExtractRequestInfo(request, dict);
  dict->SetBoolean(keys::kIsProxyKey, auth_info.is_proxy);
  if (!auth_info.scheme.empty())
    dict->SetString(keys::kSchemeKey, auth_info.scheme);
  if (!auth_info.realm.empty())
    dict->SetString(keys::kRealmKey, auth_info.realm);
  DictionaryValue* challenger = new DictionaryValue();
  challenger->SetString(keys::kHostKey, auth_info.challenger.host());
  challenger->SetInteger(keys::kPortKey, auth_info.challenger.port());
  dict->Set(keys::kChallengerKey, challenger);
  dict->Set(keys::kStatusLineKey, GetStatusLine(request->response_headers()));
  if (extra_info_spec & ExtraInfoSpec::RESPONSE_HEADERS) {
    dict->Set(keys::kResponseHeadersKey,
              GetResponseHeadersList(request->response_headers()));
  }
  args.Append(dict);

  if (DispatchEvent(profile, request, listeners, args)) {
    blocked_requests_[request->identifier()].event = kOnAuthRequired;
    blocked_requests_[request->identifier()].auth_callback = callback;
    blocked_requests_[request->identifier()].auth_credentials = credentials;
    blocked_requests_[request->identifier()].net_log = &request->net_log();
    return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_IO_PENDING;
  }
  return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

void ExtensionWebRequestEventRouter::OnBeforeRedirect(
    void* profile,
    ExtensionInfoMap* extension_info_map,
    net::URLRequest* request,
    const GURL& new_location) {
  if (!profile)
    return;

  if (!HasWebRequestScheme(request->url()))
    return;

  if (GetAndSetSignaled(request->identifier(), kOnBeforeRedirect))
    return;

  ClearSignaled(request->identifier(), kOnBeforeRequest);
  ClearSignaled(request->identifier(), kOnBeforeSendHeaders);
  ClearSignaled(request->identifier(), kOnSendHeaders);
  ClearSignaled(request->identifier(), kOnHeadersReceived);

  int extra_info_spec = 0;
  std::vector<const EventListener*> listeners =
      GetMatchingListeners(profile, extension_info_map,
                           keys::kOnBeforeRedirect, request, &extra_info_spec);
  if (listeners.empty())
    return;

  int http_status_code = request->GetResponseCode();

  std::string response_ip = request->GetSocketAddress().host();

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  ExtractRequestInfo(request, dict);
  dict->SetString(keys::kRedirectUrlKey, new_location.spec());
  dict->SetInteger(keys::kStatusCodeKey, http_status_code);
  if (!response_ip.empty())
    dict->SetString(keys::kIpKey, response_ip);
  dict->SetBoolean(keys::kFromCache, request->was_cached());
  dict->Set(keys::kStatusLineKey, GetStatusLine(request->response_headers()));
  if (extra_info_spec & ExtraInfoSpec::RESPONSE_HEADERS) {
    dict->Set(keys::kResponseHeadersKey,
              GetResponseHeadersList(request->response_headers()));
  }
  args.Append(dict);

  DispatchEvent(profile, request, listeners, args);
}

void ExtensionWebRequestEventRouter::OnResponseStarted(
    void* profile,
    ExtensionInfoMap* extension_info_map,
    net::URLRequest* request) {
  if (!profile)
    return;

  if (!HasWebRequestScheme(request->url()))
    return;

  // OnResponseStarted is even triggered, when the request was cancelled.
  if (request->status().status() != net::URLRequestStatus::SUCCESS)
    return;

  int extra_info_spec = 0;
  std::vector<const EventListener*> listeners =
      GetMatchingListeners(profile, extension_info_map,
                           keys::kOnResponseStarted, request, &extra_info_spec);
  if (listeners.empty())
    return;

  // UrlRequestFileJobs do not send headers, so we simulate their behavior.
  int response_code = 200;
  if (request->response_headers())
    response_code = request->response_headers()->response_code();

  std::string response_ip = request->GetSocketAddress().host();

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  ExtractRequestInfo(request, dict);
  if (!response_ip.empty())
    dict->SetString(keys::kIpKey, response_ip);
  dict->SetBoolean(keys::kFromCache, request->was_cached());
  dict->SetInteger(keys::kStatusCodeKey, response_code);
  dict->Set(keys::kStatusLineKey, GetStatusLine(request->response_headers()));
  if (extra_info_spec & ExtraInfoSpec::RESPONSE_HEADERS) {
    dict->Set(keys::kResponseHeadersKey,
              GetResponseHeadersList(request->response_headers()));
  }
  args.Append(dict);

  DispatchEvent(profile, request, listeners, args);
}

void ExtensionWebRequestEventRouter::OnCompleted(
    void* profile,
    ExtensionInfoMap* extension_info_map,
    net::URLRequest* request) {
  if (!profile)
    return;

  if (!HasWebRequestScheme(request->url()))
    return;

  request_time_tracker_->LogRequestEndTime(request->identifier(),
                                           base::Time::Now());

  DCHECK(request->status().status() == net::URLRequestStatus::SUCCESS);

  DCHECK(!GetAndSetSignaled(request->identifier(), kOnCompleted));

  int extra_info_spec = 0;
  std::vector<const EventListener*> listeners =
      GetMatchingListeners(profile, extension_info_map,
                           keys::kOnCompleted, request, &extra_info_spec);
  if (listeners.empty())
    return;

  // UrlRequestFileJobs do not send headers, so we simulate their behavior.
  int response_code = 200;
  if (request->response_headers())
    response_code = request->response_headers()->response_code();

  std::string response_ip = request->GetSocketAddress().host();

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  ExtractRequestInfo(request, dict);
  dict->SetInteger(keys::kStatusCodeKey, response_code);
  if (!response_ip.empty())
    dict->SetString(keys::kIpKey, response_ip);
  dict->SetBoolean(keys::kFromCache, request->was_cached());
  dict->Set(keys::kStatusLineKey, GetStatusLine(request->response_headers()));
  if (extra_info_spec & ExtraInfoSpec::RESPONSE_HEADERS) {
    dict->Set(keys::kResponseHeadersKey,
              GetResponseHeadersList(request->response_headers()));
  }
  args.Append(dict);

  DispatchEvent(profile, request, listeners, args);
}

void ExtensionWebRequestEventRouter::OnErrorOccurred(
    void* profile,
    ExtensionInfoMap* extension_info_map,
    net::URLRequest* request) {
  if (!profile)
      return;

  if (!HasWebRequestScheme(request->url()))
    return;

  request_time_tracker_->LogRequestEndTime(request->identifier(),
                                           base::Time::Now());

  DCHECK(request->status().status() == net::URLRequestStatus::FAILED ||
         request->status().status() == net::URLRequestStatus::CANCELED);

  DCHECK(!GetAndSetSignaled(request->identifier(), kOnErrorOccurred));

  int extra_info_spec = 0;
  std::vector<const EventListener*> listeners =
      GetMatchingListeners(profile, extension_info_map,
                           keys::kOnErrorOccurred, request, &extra_info_spec);
  if (listeners.empty())
    return;

  std::string response_ip = request->GetSocketAddress().host();

  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  ExtractRequestInfo(request, dict);
  if (!response_ip.empty())
    dict->SetString(keys::kIpKey, response_ip);
  dict->SetBoolean(keys::kFromCache, request->was_cached());
  dict->SetString(keys::kErrorKey,
                  net::ErrorToString(request->status().error()));
  args.Append(dict);

  DispatchEvent(profile, request, listeners, args);
}

void ExtensionWebRequestEventRouter::OnURLRequestDestroyed(
    void* profile, net::URLRequest* request) {
  blocked_requests_.erase(request->identifier());
  signaled_requests_.erase(request->identifier());

  request_time_tracker_->LogRequestEndTime(request->identifier(),
                                           base::Time::Now());
}

bool ExtensionWebRequestEventRouter::DispatchEvent(
    void* profile,
    net::URLRequest* request,
    const std::vector<const EventListener*>& listeners,
    const ListValue& args) {
  std::string json_args;

  // TODO(mpcomplete): Consider consolidating common (extension_id,json_args)
  // pairs into a single message sent to a list of sub_event_names.
  int num_handlers_blocking = 0;
  for (std::vector<const EventListener*>::const_iterator it = listeners.begin();
       it != listeners.end(); ++it) {
    // Filter out the optional keys that this listener didn't request.
    scoped_ptr<ListValue> args_filtered(args.DeepCopy());
    DictionaryValue* dict = NULL;
    CHECK(args_filtered->GetDictionary(0, &dict) && dict);
    if (!((*it)->extra_info_spec & ExtraInfoSpec::REQUEST_HEADERS))
      dict->Remove(keys::kRequestHeadersKey, NULL);
    if (!((*it)->extra_info_spec & ExtraInfoSpec::RESPONSE_HEADERS))
      dict->Remove(keys::kResponseHeadersKey, NULL);

    base::JSONWriter::Write(args_filtered.get(), false, &json_args);

    ExtensionEventRouter::DispatchEvent(
        (*it)->ipc_sender.get(), (*it)->extension_id, (*it)->sub_event_name,
        json_args, GURL());
    if ((*it)->extra_info_spec & ExtraInfoSpec::BLOCKING) {
      (*it)->blocked_requests.insert(request->identifier());
      ++num_handlers_blocking;

      request->SetLoadStateParam(
          l10n_util::GetStringFUTF16(IDS_LOAD_STATE_PARAMETER_EXTENSION,
                                     UTF8ToUTF16((*it)->extension_name)));
    }
  }

  if (num_handlers_blocking > 0) {
    CHECK(blocked_requests_.find(request->identifier()) ==
          blocked_requests_.end());
    blocked_requests_[request->identifier()].request = request;
    blocked_requests_[request->identifier()].num_handlers_blocking =
        num_handlers_blocking;
    blocked_requests_[request->identifier()].blocking_time = base::Time::Now();

    return true;
  }

  return false;
}

void ExtensionWebRequestEventRouter::OnEventHandled(
    void* profile,
    const std::string& extension_id,
    const std::string& event_name,
    const std::string& sub_event_name,
    uint64 request_id,
    EventResponse* response) {
  EventListener listener;
  listener.extension_id = extension_id;
  listener.sub_event_name = sub_event_name;

  // The listener may have been removed (e.g. due to the process going away)
  // before we got here.
  std::set<EventListener>::iterator found =
      listeners_[profile][event_name].find(listener);
  if (found != listeners_[profile][event_name].end())
    found->blocked_requests.erase(request_id);

  DecrementBlockCount(profile, extension_id, event_name, request_id, response);
}

void ExtensionWebRequestEventRouter::AddEventListener(
    void* profile,
    const std::string& extension_id,
    const std::string& extension_name,
    const std::string& event_name,
    const std::string& sub_event_name,
    const RequestFilter& filter,
    int extra_info_spec,
    base::WeakPtr<IPC::Message::Sender> ipc_sender) {
  if (!IsWebRequestEvent(event_name))
    return;

  EventListener listener;
  listener.extension_id = extension_id;
  listener.extension_name = extension_name;
  listener.sub_event_name = sub_event_name;
  listener.filter = filter;
  listener.extra_info_spec = extra_info_spec;
  listener.ipc_sender = ipc_sender;

  CHECK_EQ(listeners_[profile][event_name].count(listener), 0u) <<
      "extension=" << extension_id << " event=" << event_name;
  listeners_[profile][event_name].insert(listener);
}

void ExtensionWebRequestEventRouter::RemoveEventListener(
    void* profile,
    const std::string& extension_id,
    const std::string& sub_event_name) {
  size_t slash_sep = sub_event_name.find('/');
  std::string event_name = sub_event_name.substr(0, slash_sep);

  if (!IsWebRequestEvent(event_name))
    return;

  EventListener listener;
  listener.extension_id = extension_id;
  listener.sub_event_name = sub_event_name;

  // It's possible for AddEventListener to fail asynchronously. In that case,
  // the renderer believes the listener exists, while the browser does not.
  // Ignore a RemoveEventListener in that case.
  std::set<EventListener>::iterator found =
      listeners_[profile][event_name].find(listener);
  if (found == listeners_[profile][event_name].end())
    return;

  CHECK_EQ(listeners_[profile][event_name].count(listener), 1u) <<
      "extension=" << extension_id << " event=" << event_name;

  // Unblock any request that this event listener may have been blocking.
  for (std::set<uint64>::iterator it = found->blocked_requests.begin();
       it != found->blocked_requests.end(); ++it) {
    DecrementBlockCount(profile, extension_id, event_name, *it, NULL);
  }

  listeners_[profile][event_name].erase(listener);
}

void ExtensionWebRequestEventRouter::OnOTRProfileCreated(
    void* original_profile, void* otr_profile) {
  cross_profile_map_[original_profile] = otr_profile;
  cross_profile_map_[otr_profile] = original_profile;
}

void ExtensionWebRequestEventRouter::OnOTRProfileDestroyed(
    void* original_profile, void* otr_profile) {
  cross_profile_map_.erase(otr_profile);
  cross_profile_map_.erase(original_profile);
}

void ExtensionWebRequestEventRouter::GetMatchingListenersImpl(
    void* profile,
    ExtensionInfoMap* extension_info_map,
    bool crosses_incognito,
    const std::string& event_name,
    const GURL& url,
    int tab_id,
    int window_id,
    ResourceType::Type resource_type,
    int* extra_info_spec,
    std::vector<const ExtensionWebRequestEventRouter::EventListener*>*
        matching_listeners) {
  std::set<EventListener>& listeners = listeners_[profile][event_name];
  for (std::set<EventListener>::iterator it = listeners.begin();
       it != listeners.end(); ++it) {
    if (!it->ipc_sender.get()) {
      // The IPC sender has been deleted. This listener will be removed soon
      // via a call to RemoveEventListener. For now, just skip it.
      continue;
    }

    if (!it->filter.urls.is_empty() && !it->filter.urls.MatchesURL(url))
      continue;
    if (it->filter.tab_id != -1 && tab_id != it->filter.tab_id)
      continue;
    if (it->filter.window_id != -1 && window_id != it->filter.window_id)
      continue;
    if (!it->filter.types.empty() &&
        std::find(it->filter.types.begin(), it->filter.types.end(),
                  resource_type) == it->filter.types.end())
      continue;

    // extension_info_map can be NULL if this is a system-level request.
    if (extension_info_map) {
      const Extension* extension =
          extension_info_map->extensions().GetByID(it->extension_id);

      // Check if this event crosses incognito boundaries when it shouldn't.
      if (!extension ||
          (crosses_incognito &&
           !extension_info_map->CanCrossIncognito(extension)))
        continue;

      // Only send webRequest events for URLs the extension has access to.
      if (!CanExtensionAccessURL(extension, url))
        continue;
    }

    matching_listeners->push_back(&(*it));
    *extra_info_spec |= it->extra_info_spec;
  }
}

std::vector<const ExtensionWebRequestEventRouter::EventListener*>
ExtensionWebRequestEventRouter::GetMatchingListeners(
    void* profile,
    ExtensionInfoMap* extension_info_map,
    const std::string& event_name,
    net::URLRequest* request,
    int* extra_info_spec) {
  // TODO(mpcomplete): handle profile == NULL (should collect all listeners).
  *extra_info_spec = 0;

  bool is_main_frame = false;
  int64 frame_id = -1;
  int tab_id = -1;
  int window_id = -1;
  ResourceType::Type resource_type = ResourceType::LAST_TYPE;
  const GURL& url = request->url();

  ExtractRequestInfoDetails(request, &is_main_frame, &frame_id, &tab_id,
                            &window_id, &resource_type);

  std::vector<const ExtensionWebRequestEventRouter::EventListener*>
      matching_listeners;

  GetMatchingListenersImpl(
      profile, extension_info_map, false, event_name, url,
      tab_id, window_id, resource_type, extra_info_spec, &matching_listeners);
  CrossProfileMap::const_iterator cross_profile =
      cross_profile_map_.find(profile);
  if (cross_profile != cross_profile_map_.end()) {
    GetMatchingListenersImpl(
        cross_profile->second, extension_info_map, true, event_name, url,
        tab_id, window_id, resource_type, extra_info_spec, &matching_listeners);
  }

  return matching_listeners;
}

linked_ptr<ExtensionWebRequestEventRouter::EventResponseDelta>
    ExtensionWebRequestEventRouter::CalculateDelta(
        BlockedRequest* blocked_request,
        EventResponse* response) const {
  linked_ptr<EventResponseDelta> result(
      new EventResponseDelta(response->extension_id,
                             response->extension_install_time));

  result->cancel = response->cancel;

  if (blocked_request->event == kOnBeforeRequest)
    result->new_url = response->new_url;

  if (blocked_request->event == kOnBeforeSendHeaders) {
    net::HttpRequestHeaders* old_headers = blocked_request->request_headers;
    net::HttpRequestHeaders* new_headers = response->request_headers.get();

    // Find deleted headers.
    {
      net::HttpRequestHeaders::Iterator i(*old_headers);
      while (i.GetNext()) {
        if (!new_headers->HasHeader(i.name())) {
          result->deleted_request_headers.push_back(i.name());
        }
      }
    }

    // Find modified headers.
    {
      net::HttpRequestHeaders::Iterator i(*new_headers);
      while (i.GetNext()) {
        std::string value;
        if (!old_headers->GetHeader(i.name(), &value) || i.value() != value) {
          result->modified_request_headers.SetHeader(i.name(), i.value());
        }
      }
    }
  }

  if (blocked_request->event == kOnHeadersReceived) {
    net::HttpResponseHeaders* old_headers =
        blocked_request->original_response_headers.get();
    if (!response->response_headers_string.empty()) {
      std::string new_headers_string =
          old_headers->GetStatusLine() + "\n" +
          response->response_headers_string;

      result->new_response_headers =
          new net::HttpResponseHeaders(
              net::HttpUtil::AssembleRawHeaders(new_headers_string.c_str(),
                                                new_headers_string.length()));
    }
  }

  if (blocked_request->event == kOnAuthRequired)
    result->auth_credentials.swap(response->auth_credentials);

  return result;
}

void ExtensionWebRequestEventRouter::MergeOnBeforeRequestResponses(
    BlockedRequest* request,
    std::list<std::string>* conflicting_extensions) const {
  EventResponseDeltas::iterator delta;
  EventResponseDeltas& deltas = request->response_deltas;

  bool redirected = false;
  for (delta = deltas.begin(); delta != deltas.end(); ++delta) {
    if (!(*delta)->new_url.is_empty()) {
      if (!redirected) {
        *request->new_url = (*delta)->new_url;
        redirected = true;
        request->net_log->AddEvent(
            net::NetLog::TYPE_CHROME_EXTENSION_REDIRECTED_REQUEST,
            make_scoped_refptr(
                new NetLogExtensionIdParameter((*delta)->extension_id)));
      } else {
        conflicting_extensions->push_back((*delta)->extension_id);
        request->net_log->AddEvent(
            net::NetLog::TYPE_CHROME_EXTENSION_IGNORED_DUE_TO_CONFLICT,
            make_scoped_refptr(
                new NetLogExtensionIdParameter((*delta)->extension_id)));
      }
    }
  }
}

void ExtensionWebRequestEventRouter::MergeOnBeforeSendHeadersResponses(
    BlockedRequest* request,
    std::list<std::string>* conflicting_extensions) const {
  EventResponseDeltas::iterator delta;
  EventResponseDeltas& deltas = request->response_deltas;

  // Here we collect which headers we have removed or set to new values
  // so far due to extensions of higher precedence.
  std::set<std::string> removed_headers;
  std::set<std::string> set_headers;

  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  for (delta = deltas.begin(); delta != deltas.end(); ++delta) {
    if ((*delta)->modified_request_headers.IsEmpty() &&
        (*delta)->deleted_request_headers.empty()) {
      continue;
    }

    scoped_refptr<NetLogModificationParameter> log(
        new NetLogModificationParameter((*delta)->extension_id));

    // Check whether any modification affects a request header that
    // has been modified differently before. As deltas is sorted by decreasing
    // extension installation order, this takes care of precedence.
    bool extension_conflicts = false;
    {
      net::HttpRequestHeaders::Iterator modification(
          (*delta)->modified_request_headers);
      while (modification.GetNext() && !extension_conflicts) {
        // This modification sets |key| to |value|.
        const std::string& key = modification.name();
        const std::string& value = modification.value();

        // We must not delete anything that has been modified before.
        if (removed_headers.find(key) != removed_headers.end())
          extension_conflicts = true;

        // We must not modify anything that has been set to a *different*
        // value before.
        if (set_headers.find(key) != set_headers.end()) {
          std::string current_value;
          if (!request->request_headers->GetHeader(key, &current_value) ||
              current_value != value) {
            extension_conflicts = true;
          }
        }
      }
    }

    // Check whether any deletion affects a request header that has been
    // modified before.
    {
      std::vector<std::string>::iterator key;
      for (key = (*delta)->deleted_request_headers.begin();
           key != (*delta)->deleted_request_headers.end() &&
               !extension_conflicts;
           ++key) {
        if (set_headers.find(*key) != set_headers.end())
          extension_conflicts = true;
      }
    }

    // Now execute the modifications if there were no conflicts.
    if (!extension_conflicts) {
      // Copy all modifications into the original headers.
      request->request_headers->MergeFrom((*delta)->modified_request_headers);
      {
        // Record which keys were changed.
        net::HttpRequestHeaders::Iterator modification(
            (*delta)->modified_request_headers);
        while (modification.GetNext()) {
          set_headers.insert(modification.name());
          log->ModifiedHeader(modification.name(), modification.value());
        }
      }

      // Perform all deletions and record which keys were deleted.
      {
        std::vector<std::string>::iterator key;
        for (key = (*delta)->deleted_request_headers.begin();
            key != (*delta)->deleted_request_headers.end();
             ++key) {
          request->request_headers->RemoveHeader(*key);
          removed_headers.insert(*key);
          log->DeletedHeader(*key);
        }
      }
      request->net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_MODIFIED_HEADERS, log);
    } else {
      conflicting_extensions->push_back((*delta)->extension_id);
      request->net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_IGNORED_DUE_TO_CONFLICT,
          make_scoped_refptr(
              new NetLogExtensionIdParameter((*delta)->extension_id)));
    }
  }
}

void ExtensionWebRequestEventRouter::MergeOnHeadersReceivedResponses(
    BlockedRequest* request,
    std::list<std::string>* conflicting_extensions) const {
  EventResponseDeltas::iterator delta;
  EventResponseDeltas& deltas = request->response_deltas;

  // Whether any extension has overridden the response headers, yet.
  bool headers_overridden = false;

  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  for (delta = deltas.begin(); delta != deltas.end(); ++delta) {
    if (!(*delta)->new_response_headers.get())
      continue;

    scoped_refptr<NetLogModificationParameter> log(
        new NetLogModificationParameter((*delta)->extension_id));

    // Conflict if a second extension returned new response headers;
    bool extension_conflicts = headers_overridden;

    if (!extension_conflicts) {
      headers_overridden = true;
      *request->override_response_headers = (*delta)->new_response_headers;
      request->net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_MODIFIED_HEADERS, log);
    } else {
      conflicting_extensions->push_back((*delta)->extension_id);
      request->net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_IGNORED_DUE_TO_CONFLICT,
          make_scoped_refptr(
              new NetLogExtensionIdParameter((*delta)->extension_id)));
    }
  }
}

bool ExtensionWebRequestEventRouter::MergeOnAuthRequiredResponses(
    BlockedRequest* request,
    std::list<std::string>* conflicting_extensions) const {
  CHECK(request);
  CHECK(request->auth_credentials);
  bool credentials_set = false;

  const EventResponseDeltas& deltas = request->response_deltas;
  for (EventResponseDeltas::const_iterator delta = deltas.begin();
       delta != deltas.end();
       ++delta) {
    if (!(*delta)->auth_credentials.get())
      continue;
    if (credentials_set) {
      conflicting_extensions->push_back((*delta)->extension_id);
      request->net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_IGNORED_DUE_TO_CONFLICT,
          make_scoped_refptr(
              new NetLogExtensionIdParameter((*delta)->extension_id)));
    } else {
      request->net_log->AddEvent(
          net::NetLog::TYPE_CHROME_EXTENSION_PROVIDE_AUTH_CREDENTIALS,
          make_scoped_refptr(
              new NetLogExtensionIdParameter((*delta)->extension_id)));
      *request->auth_credentials = *(*delta)->auth_credentials;
      credentials_set = true;
    }
  }
  return credentials_set;
}

void ExtensionWebRequestEventRouter::DecrementBlockCount(
    void* profile,
    const std::string& extension_id,
    const std::string& event_name,
    uint64 request_id,
    EventResponse* response) {
  scoped_ptr<EventResponse> response_scoped(response);

  // It's possible that this request was deleted, or cancelled by a previous
  // event handler. If so, ignore this response.
  if (blocked_requests_.find(request_id) == blocked_requests_.end())
    return;

  BlockedRequest& blocked_request = blocked_requests_[request_id];
  int num_handlers_blocking = --blocked_request.num_handlers_blocking;
  CHECK_GE(num_handlers_blocking, 0);

  if (response) {
    blocked_request.response_deltas.push_back(
        CalculateDelta(&blocked_request, response));
  }

  base::TimeDelta block_time =
      base::Time::Now() - blocked_request.blocking_time;
  request_time_tracker_->IncrementExtensionBlockTime(
      extension_id, request_id, block_time);

  if (num_handlers_blocking == 0) {
    request_time_tracker_->IncrementTotalBlockTime(request_id, block_time);

    EventResponseDeltas& deltas = blocked_request.response_deltas;
    bool canceled = false;
    bool credentials_set = false;
    for (EventResponseDeltas::const_iterator i = deltas.begin();
         i != deltas.end(); ++i) {
      if ((*i)->cancel) {
       canceled = true;
       blocked_request.net_log->AddEvent(
            net::NetLog::TYPE_CHROME_EXTENSION_ABORTED_REQUEST,
            make_scoped_refptr(
                new NetLogExtensionIdParameter((*i)->extension_id)));
        break;
      }
    }

    deltas.sort(&InDecreasingExtensionInstallationTimeOrder);
    std::list<std::string> conflicting_extensions;

    if (blocked_request.event == kOnBeforeRequest) {
      CHECK(blocked_request.callback);
      MergeOnBeforeRequestResponses(&blocked_request,
                                    &conflicting_extensions);
    } else if (blocked_request.event == kOnBeforeSendHeaders) {
      CHECK(blocked_request.callback);
      MergeOnBeforeSendHeadersResponses(&blocked_request,
                                        &conflicting_extensions);
    } else if (blocked_request.event == kOnHeadersReceived) {
      CHECK(blocked_request.callback);
      MergeOnHeadersReceivedResponses(&blocked_request,
                                      &conflicting_extensions);
    } else if (blocked_request.event == kOnAuthRequired) {
      CHECK(!blocked_request.callback);
      CHECK(!blocked_request.auth_callback.is_null());
      credentials_set = MergeOnAuthRequiredResponses(
         &blocked_request,
         &conflicting_extensions);
    } else {
      NOTREACHED();
    }
    std::list<std::string>::iterator conflict_iter;
    for (conflict_iter = conflicting_extensions.begin();
         conflict_iter != conflicting_extensions.end();
         ++conflict_iter) {
      // TODO(mpcomplete): Do some fancy notification in the UI.
      LOG(ERROR) << "Extension " << *conflict_iter << " was ignored in "
                    "webRequest API because of conflicting request "
                    "modifications.";
    }

    if (canceled) {
      request_time_tracker_->SetRequestCanceled(request_id);
    } else if (blocked_request.new_url &&
               !blocked_request.new_url->is_empty()) {
      request_time_tracker_->SetRequestRedirected(request_id);
    }

    // This signals a failed request to subscribers of onErrorOccurred in case
    // a request is cancelled because net::ERR_EMPTY_RESPONSE cannot be
    // distinguished from a regular failure.
    if (blocked_request.callback) {
      int rv = canceled ? net::ERR_EMPTY_RESPONSE : net::OK;
      net::OldCompletionCallback* callback = blocked_request.callback;
      // Ensure that request is removed before callback because the callback
      // might trigger the next event.
      blocked_requests_.erase(request_id);
      callback->Run(rv);
    } else if (!blocked_request.auth_callback.is_null()) {
      net::NetworkDelegate::AuthRequiredResponse response =
          net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
      if (canceled) {
        response = net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_CANCEL_AUTH;
      } else if (credentials_set) {
        response = net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH;
      }
      net::NetworkDelegate::AuthCallback callback =
          blocked_request.auth_callback;
      blocked_requests_.erase(request_id);
      callback.Run(response);
    } else {
      blocked_requests_.erase(request_id);
    }
  } else {
    // Update the URLRequest to indicate it is now blocked on a different
    // extension.
    std::set<EventListener>& listeners = listeners_[profile][event_name];

    for (std::set<EventListener>::iterator it = listeners.begin();
         it != listeners.end(); ++it) {
      if (it->blocked_requests.count(request_id)) {
        blocked_request.request->SetLoadStateParam(
            l10n_util::GetStringFUTF16(IDS_LOAD_STATE_PARAMETER_EXTENSION,
                                       UTF8ToUTF16(it->extension_name)));
        break;
      }
    }
  }
}

bool ExtensionWebRequestEventRouter::GetAndSetSignaled(uint64 request_id,
                                                       EventTypes event_type) {
  SignaledRequestMap::iterator iter = signaled_requests_.find(request_id);
  if (iter == signaled_requests_.end()) {
    signaled_requests_[request_id] = event_type;
    return false;
  }
  bool was_signaled_before = (iter->second & event_type) != 0;
  iter->second |= event_type;
  return was_signaled_before;
}

void ExtensionWebRequestEventRouter::ClearSignaled(uint64 request_id,
                                                   EventTypes event_type) {
  SignaledRequestMap::iterator iter = signaled_requests_.find(request_id);
  if (iter == signaled_requests_.end())
    return;
  iter->second &= ~event_type;
}

bool WebRequestAddEventListener::RunImpl() {
  // Argument 0 is the callback, which we don't use here.

  ExtensionWebRequestEventRouter::RequestFilter filter;
  if (HasOptionalArgument(1)) {
    DictionaryValue* value = NULL;
    error_.clear();
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &value));
    // Failure + an empty error string means a fatal error.
    EXTENSION_FUNCTION_VALIDATE(filter.InitFromValue(*value, &error_) ||
                                !error_.empty());
    if (!error_.empty())
      return false;
  }

  int extra_info_spec = 0;
  if (HasOptionalArgument(2)) {
    ListValue* value = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->GetList(2, &value));
    EXTENSION_FUNCTION_VALIDATE(
        ExtensionWebRequestEventRouter::ExtraInfoSpec::InitFromValue(
            *value, &extra_info_spec));
  }

  std::string event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(3, &event_name));

  std::string sub_event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(4, &sub_event_name));

  const Extension* extension =
      extension_info_map()->extensions().GetByID(extension_id());
  std::string extension_name = extension ? extension->name() : extension_id();

  ExtensionWebRequestEventRouter::GetInstance()->AddEventListener(
      profile_id(), extension_id(), extension_name,
      event_name, sub_event_name, filter,
      extra_info_spec, ipc_sender_weak());

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &NotifyWebRequestAPIUsed,
      profile_id(), make_scoped_refptr(GetExtension())));

  return true;
}

bool WebRequestEventHandled::RunImpl() {
  std::string event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &event_name));

  std::string sub_event_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &sub_event_name));

  std::string request_id_str;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(2, &request_id_str));
  // TODO(mpcomplete): string-to-uint64?
  int64 request_id;
  EXTENSION_FUNCTION_VALIDATE(base::StringToInt64(request_id_str, &request_id));

  scoped_ptr<ExtensionWebRequestEventRouter::EventResponse> response;
  if (HasOptionalArgument(3)) {
    DictionaryValue* value = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(3, &value));

    if (!value->empty()) {
      base::Time install_time =
          extension_info_map()->GetInstallTime(extension_id());
      response.reset(new ExtensionWebRequestEventRouter::EventResponse(
          extension_id(), install_time));
    }

    if (value->HasKey("cancel")) {
      // Don't allow cancel mixed with other keys.
      if (value->HasKey("redirectUrl") || value->HasKey("requestHeaders")) {
        error_ = keys::kInvalidBlockingResponse;
        return false;
      }

      bool cancel = false;
      EXTENSION_FUNCTION_VALIDATE(value->GetBoolean("cancel", &cancel));
      response->cancel = cancel;
    }

    if (value->HasKey("redirectUrl")) {
      std::string new_url_str;
      EXTENSION_FUNCTION_VALIDATE(value->GetString("redirectUrl",
                                                   &new_url_str));
      response->new_url = GURL(new_url_str);
      if (!response->new_url.is_valid()) {
        error_ = ExtensionErrorUtils::FormatErrorMessage(
            keys::kInvalidRedirectUrl, new_url_str);
        return false;
      }
    }

    if (value->HasKey("requestHeaders")) {
      ListValue* request_headers_value = NULL;
      response->request_headers.reset(new net::HttpRequestHeaders());
      EXTENSION_FUNCTION_VALIDATE(value->GetList(keys::kRequestHeadersKey,
                                                 &request_headers_value));
      for (size_t i = 0; i < request_headers_value->GetSize(); ++i) {
        DictionaryValue* header_value = NULL;
        std::string name;
        std::string value;
        EXTENSION_FUNCTION_VALIDATE(
            request_headers_value->GetDictionary(i, &header_value));
        EXTENSION_FUNCTION_VALIDATE(
            header_value->GetString(keys::kHeaderNameKey, &name));
        EXTENSION_FUNCTION_VALIDATE(
            header_value->GetString(keys::kHeaderValueKey, &value));

        response->request_headers->SetHeader(name, value);
      }
    }

    if (value->HasKey("responseHeaders")) {
      std::string response_headers_string;
      ListValue* response_headers_value = NULL;
      EXTENSION_FUNCTION_VALIDATE(value->GetList(keys::kResponseHeadersKey,
                                                 &response_headers_value));
      for (size_t i = 0; i < response_headers_value->GetSize(); ++i) {
        DictionaryValue* header_value = NULL;
        std::string name;
        std::string value;
        EXTENSION_FUNCTION_VALIDATE(
            response_headers_value->GetDictionary(i, &header_value));
        EXTENSION_FUNCTION_VALIDATE(
            header_value->GetString(keys::kHeaderNameKey, &name));
        EXTENSION_FUNCTION_VALIDATE(
            header_value->GetString(keys::kHeaderValueKey, &value));

        response_headers_string += name + ": " + value + '\n';
      }
      response_headers_string += '\n';
      response->response_headers_string.swap(response_headers_string);
    }

    if (value->HasKey(keys::kAuthCredentialsKey)) {
      DictionaryValue* credentials_value = NULL;
      EXTENSION_FUNCTION_VALIDATE(value->GetDictionary(
          keys::kAuthCredentialsKey,
          &credentials_value));
      response->auth_credentials.reset(new net::AuthCredentials());
      EXTENSION_FUNCTION_VALIDATE(
          credentials_value->GetString(keys::kUsernameKey,
                                       &response->auth_credentials->username));
      EXTENSION_FUNCTION_VALIDATE(
          credentials_value->GetString(keys::kPasswordKey,
                                       &response->auth_credentials->password));
    }
  }

  ExtensionWebRequestEventRouter::GetInstance()->OnEventHandled(
      profile_id(), extension_id(), event_name, sub_event_name, request_id,
      response.release());

  return true;
}

bool WebRequestHandlerBehaviorChanged::RunImpl() {
  WebCacheManager::GetInstance()->ClearCacheOnNavigation();
  return true;
}

void SendExtensionWebRequestStatusToHost(RenderProcessHost* host) {
  Profile* profile = Profile::FromBrowserContext(host->browser_context());
  if (!profile || !profile->GetExtensionService())
    return;

  bool adblock = false;
  bool adblock_plus = false;
  bool other = false;
  const ExtensionList* extensions =
      profile->GetExtensionService()->extensions();
  for (ExtensionList::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    if (profile->GetExtensionService()->HasUsedWebRequest(*it)) {
      if ((*it)->name().find("Adblock Plus") != std::string::npos) {
        adblock_plus = true;
      } else if ((*it)->name().find("AdBlock") != std::string::npos) {
        adblock = true;
      } else {
        other = true;
      }
    }
  }

  host->Send(new ExtensionMsg_UsingWebRequestAPI(adblock, adblock_plus, other));
}
