// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_API_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/singleton.h"
#include "base/time.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/url_pattern_set.h"
#include "ipc/ipc_message.h"
#include "net/base/completion_callback.h"
#include "net/base/network_delegate.h"
#include "net/http/http_request_headers.h"
#include "webkit/glue/resource_type.h"

class ExtensionInfoMap;
class ExtensionWebRequestTimeTracker;
class GURL;
class RenderProcessHost;

namespace base {
class DictionaryValue;
class ListValue;
class StringValue;
}

namespace net {
class AuthCredentials;
class AuthChallengeInfo;
class HostPortPair;
class HttpRequestHeaders;
class HttpResponseHeaders;
class URLRequest;
}

// This class observes network events and routes them to the appropriate
// extensions listening to those events. All methods must be called on the IO
// thread unless otherwise specified.
class ExtensionWebRequestEventRouter {
 public:
  enum EventTypes {
    kInvalidEvent = 0,
    kOnBeforeRequest = 1 << 0,
    kOnBeforeSendHeaders = 1 << 1,
    kOnSendHeaders = 1 << 2,
    kOnHeadersReceived = 1 << 3,
    kOnBeforeRedirect = 1 << 4,
    kOnAuthRequired = 1 << 5,
    kOnResponseStarted = 1 << 6,
    kOnErrorOccurred = 1 << 7,
    kOnCompleted = 1 << 8,
  };

  // Internal representation of the webRequest.RequestFilter type, used to
  // filter what network events an extension cares about.
  struct RequestFilter {
    URLPatternSet urls;
    std::vector<ResourceType::Type> types;
    int tab_id;
    int window_id;

    RequestFilter();
    ~RequestFilter();

    // Returns false if there was an error initializing. If it is a user error,
    // an error message is provided, otherwise the error is internal (and
    // unexpected).
    bool InitFromValue(const base::DictionaryValue& value, std::string* error);
  };

  // Internal representation of the extraInfoSpec parameter on webRequest
  // events, used to specify extra information to be included with network
  // events.
  struct ExtraInfoSpec {
    enum Flags {
      REQUEST_HEADERS = 1<<0,
      RESPONSE_HEADERS = 1<<1,
      BLOCKING = 1<<2,
    };

    static bool InitFromValue(const base::ListValue& value,
                              int* extra_info_spec);
  };

  // Contains an extension's response to a blocking event.
  struct EventResponse {
    // ID of the extension that sent this response.
    std::string extension_id;

    // The time that the extension was installed. Used for deciding order of
    // precedence in case multiple extensions respond with conflicting
    // decisions.
    base::Time extension_install_time;

    // Response values. These are mutually exclusive.
    bool cancel;
    GURL new_url;
    scoped_ptr<net::HttpRequestHeaders> request_headers;
    // Contains all header lines after the status line, lines are \n separated.
    std::string response_headers_string;
    scoped_ptr<net::AuthCredentials> auth_credentials;

    EventResponse(const std::string& extension_id,
                  const base::Time& extension_install_time);
    ~EventResponse();

    DISALLOW_COPY_AND_ASSIGN(EventResponse);
  };

  // Contains the modification an extension wants to perform on an event.
  struct EventResponseDelta {
    // ID of the extension that sent this response.
    std::string extension_id;

    // The time that the extension was installed. Used for deciding order of
    // precedence in case multiple extensions respond with conflicting
    // decisions.
    base::Time extension_install_time;

    // Response values. These are mutually exclusive.
    bool cancel;
    GURL new_url;

    // Newly introduced or overridden request headers.
    net::HttpRequestHeaders modified_request_headers;

    // Keys of request headers to be deleted.
    std::vector<std::string> deleted_request_headers;

    // Complete set of response headers that will replace the original ones.
    scoped_refptr<net::HttpResponseHeaders> new_response_headers;

    // Authentication Credentials to use.
    scoped_ptr<net::AuthCredentials> auth_credentials;

    EventResponseDelta(const std::string& extension_id,
                       const base::Time& extension_install_time);
    ~EventResponseDelta();

    DISALLOW_COPY_AND_ASSIGN(EventResponseDelta);
  };

  typedef std::list<linked_ptr<EventResponseDelta> > EventResponseDeltas;

  static ExtensionWebRequestEventRouter* GetInstance();

  // Dispatches the OnBeforeRequest event to any extensions whose filters match
  // the given request. Returns net::ERR_IO_PENDING if an extension is
  // intercepting the request, OK otherwise.
  int OnBeforeRequest(void* profile,
                      ExtensionInfoMap* extension_info_map,
                      net::URLRequest* request,
                      net::OldCompletionCallback* callback,
                      GURL* new_url);

  // Dispatches the onBeforeSendHeaders event. This is fired for HTTP(s)
  // requests only, and allows modification of the outgoing request headers.
  // Returns net::ERR_IO_PENDING if an extension is intercepting the request, OK
  // otherwise.
  int OnBeforeSendHeaders(void* profile,
                          ExtensionInfoMap* extension_info_map,
                          net::URLRequest* request,
                          net::OldCompletionCallback* callback,
                          net::HttpRequestHeaders* headers);

  // Dispatches the onSendHeaders event. This is fired for HTTP(s) requests
  // only.
  void OnSendHeaders(void* profile,
                     ExtensionInfoMap* extension_info_map,
                     net::URLRequest* request,
                     const net::HttpRequestHeaders& headers);

  // Dispatches the onHeadersReceived event. This is fired for HTTP(s)
  // requests only, and allows modification of incoming response headers.
  // Returns net::ERR_IO_PENDING if an extension is intercepting the request,
  // OK otherwise. |original_response_headers| is reference counted. |callback|
  // and |override_response_headers| are owned by a URLRequestJob. They are
  // guaranteed to be valid until |callback| is called or OnURLRequestDestroyed
  // is called (whatever comes first).
  // Do not modify |original_response_headers| directly but write new ones
  // into |override_response_headers|.
  int OnHeadersReceived(
      void* profile,
      ExtensionInfoMap* extension_info_map,
      net::URLRequest* request,
      net::OldCompletionCallback* callback,
      net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers);

  // Dispatches the OnAuthRequired event to any extensions whose filters match
  // the given request. If the listener is not registered as "blocking", then
  // AUTH_REQUIRED_RESPONSE_OK is returned. Otherwise,
  // AUTH_REQUIRED_RESPONSE_IO_PENDING is returned and |callback| will be
  // invoked later.
  net::NetworkDelegate::AuthRequiredResponse OnAuthRequired(
                     void* profile,
                     ExtensionInfoMap* extension_info_map,
                     net::URLRequest* request,
                     const net::AuthChallengeInfo& auth_info,
                     const net::NetworkDelegate::AuthCallback& callback,
                     net::AuthCredentials* credentials);

  // Dispatches the onBeforeRedirect event. This is fired for HTTP(s) requests
  // only.
  void OnBeforeRedirect(void* profile,
                        ExtensionInfoMap* extension_info_map,
                        net::URLRequest* request,
                        const GURL& new_location);

  // Dispatches the onResponseStarted event indicating that the first bytes of
  // the response have arrived.
  void OnResponseStarted(void* profile,
                         ExtensionInfoMap* extension_info_map,
                         net::URLRequest* request);

  // Dispatches the onComplete event.
  void OnCompleted(void* profile,
                   ExtensionInfoMap* extension_info_map,
                   net::URLRequest* request);

  // Dispatches an onErrorOccurred event.
  void OnErrorOccurred(void* profile,
                      ExtensionInfoMap* extension_info_map,
                      net::URLRequest* request);

  // Notifications when objects are going away.
  void OnURLRequestDestroyed(void* profile, net::URLRequest* request);

  // Called when an event listener handles a blocking event and responds.
  void OnEventHandled(
      void* profile,
      const std::string& extension_id,
      const std::string& event_name,
      const std::string& sub_event_name,
      uint64 request_id,
      EventResponse* response);

  // Adds a listener to the given event. |event_name| specifies the event being
  // listened to. |sub_event_name| is an internal event uniquely generated in
  // the extension process to correspond to the given filter and
  // extra_info_spec.
  void AddEventListener(
      void* profile,
      const std::string& extension_id,
      const std::string& extension_name,
      const std::string& event_name,
      const std::string& sub_event_name,
      const RequestFilter& filter,
      int extra_info_spec,
      base::WeakPtr<IPC::Message::Sender> ipc_sender);

  // Removes the listener for the given sub-event.
  void RemoveEventListener(
      void* profile,
      const std::string& extension_id,
      const std::string& sub_event_name);

  // Called when an incognito profile is created or destroyed.
  void OnOTRProfileCreated(void* original_profile,
                           void* otr_profile);
  void OnOTRProfileDestroyed(void* original_profile,
                             void* otr_profile);

 private:
  friend struct DefaultSingletonTraits<ExtensionWebRequestEventRouter>;
  struct EventListener;
  struct BlockedRequest;
  typedef std::map<std::string, std::set<EventListener> > ListenerMapForProfile;
  typedef std::map<void*, ListenerMapForProfile> ListenerMap;
  typedef std::map<uint64, BlockedRequest> BlockedRequestMap;
  // Map of request_id -> bit vector of EventTypes already signaled
  typedef std::map<uint64, int> SignaledRequestMap;
  typedef std::map<void*, void*> CrossProfileMap;

  ExtensionWebRequestEventRouter();
  ~ExtensionWebRequestEventRouter();

  bool DispatchEvent(
      void* profile,
      net::URLRequest* request,
      const std::vector<const EventListener*>& listeners,
      const base::ListValue& args);

  // Returns a list of event listeners that care about the given event, based
  // on their filter parameters. |extra_info_spec| will contain the combined
  // set of extra_info_spec flags that every matching listener asked for.
  std::vector<const EventListener*> GetMatchingListeners(
      void* profile,
      ExtensionInfoMap* extension_info_map,
      const std::string& event_name,
      net::URLRequest* request,
      int* extra_info_spec);

  // Helper for the above functions. This is called twice: once for the profile
  // of the event, the next time for the "cross" profile (i.e. the incognito
  // profile if the event is originally for the normal profile, or vice versa).
  void GetMatchingListenersImpl(
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
          matching_listeners);

  // Decrements the count of event handlers blocking the given request. When the
  // count reaches 0, we stop blocking the request and proceed it using the
  // method requested by the extension with the highest precedence. Precedence
  // is decided by extension install time. If |response| is non-NULL, this
  // method assumes ownership.
  void DecrementBlockCount(
      void* profile,
      const std::string& extension_id,
      const std::string& event_name,
      uint64 request_id,
      EventResponse* response);

  // Sets the flag that |event_type| has been signaled for |request_id|.
  // Returns the value of the flag before setting it.
  bool GetAndSetSignaled(uint64 request_id, EventTypes event_type);

  // Clears the flag that |event_type| has been signaled for |request_id|.
  void ClearSignaled(uint64 request_id, EventTypes event_type);

  // Returns the difference between the original request properties stored
  // in |blocked_request| and the EventResponse in |response|.
  linked_ptr<EventResponseDelta> CalculateDelta(
      BlockedRequest* blocked_request,
      EventResponse* response) const;

  // These functions merge the responses of blocked request handlers,
  // assuming that |request| contains valid |respone_deltas| representing the
  // changes done by the respective handlers. The |response_deltas| need to be
  // sorted in decreasing order of precedence of extensions.
  // The functions update the target url or the headers in |request|
  // and reports extension IDs of extensions whose wishes could not be honored
  // in |conflicting_extensions|.
  void MergeOnBeforeRequestResponses(
      BlockedRequest* request,
      std::list<std::string>* conflicting_extensions) const;
  void MergeOnBeforeSendHeadersResponses(
      BlockedRequest* request,
      std::list<std::string>* conflicting_extensions) const;
  void MergeOnHeadersReceivedResponses(
      BlockedRequest* request,
      std::list<std::string>* conflicting_extensions) const;

  // Merge the responses of blocked onAuthRequired handlers. The first
  // registered listener that supplies authentication credentials in a response,
  // if any, will have its authentication credentials used. |request| must be
  // non-NULL, and contain |deltas| that are sorted in decreasing order of
  // precedence.
  // Returns whether authentication credentials are set.
  bool MergeOnAuthRequiredResponses(
      BlockedRequest* request,
      std::list<std::string>* conflicting_extensions) const;

  // A map for each profile that maps an event name to a set of extensions that
  // are listening to that event.
  ListenerMap listeners_;

  // A map of network requests that are waiting for at least one event handler
  // to respond.
  BlockedRequestMap blocked_requests_;

  // A map of request ids to a bitvector indicating which events have been
  // signaled and should not be sent again.
  SignaledRequestMap signaled_requests_;

  // A map of original profile -> corresponding incognito profile (and vice
  // versa).
  CrossProfileMap cross_profile_map_;

  // Keeps track of time spent waiting on extensions using the blocking
  // webRequest API.
  scoped_ptr<ExtensionWebRequestTimeTracker> request_time_tracker_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebRequestEventRouter);
};

class WebRequestAddEventListener : public SyncIOThreadExtensionFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.webRequest.addEventListener");
};

class WebRequestEventHandled : public SyncIOThreadExtensionFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.webRequest.eventHandled");
};

class WebRequestHandlerBehaviorChanged : public AsyncExtensionFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.webRequest.handlerBehaviorChanged");
};

// Send updates to |host| with information about what webRequest-related
// extensions are installed.
// TODO(mpcomplete): remove. http://crbug.com/100411
void SendExtensionWebRequestStatusToHost(RenderProcessHost* host);

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_API_H_
