// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_management_service.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/policy/device_management_backend.h"
#include "chrome/browser/policy/device_management_backend_impl.h"
#include "content/browser/browser_thread.h"
#include "content/common/content_client.h"
#include "net/base/cookie_monster.h"
#include "net/base/host_resolver.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace policy {

namespace {

bool IsProxyError(const net::URLRequestStatus status) {
  switch (status.error()) {
    case net::ERR_PROXY_CONNECTION_FAILED:
    case net::ERR_TUNNEL_CONNECTION_FAILED:
    case net::ERR_PROXY_AUTH_UNSUPPORTED:
    case net::ERR_HTTPS_PROXY_TUNNEL_RESPONSE:
    case net::ERR_MANDATORY_PROXY_CONFIGURATION_FAILED:
    case net::ERR_PROXY_CERTIFICATE_INVALID:
    case net::ERR_SOCKS_CONNECTION_FAILED:
    case net::ERR_SOCKS_CONNECTION_HOST_UNREACHABLE:
      return true;
  }
  return false;
}

bool IsProtobufMimeType(const URLFetcher* source) {
  return source->response_headers()->HasHeaderValue(
      "content-type", "application/x-protobuffer");
}

// Custom request context implementation that allows to override the user agent,
// amongst others. Wraps a baseline request context from which we reuse the
// networking components.
class DeviceManagementRequestContext : public net::URLRequestContext {
 public:
  explicit DeviceManagementRequestContext(net::URLRequestContext* base_context);
  virtual ~DeviceManagementRequestContext();

 private:
  // Overridden from net::URLRequestContext:
  virtual const std::string& GetUserAgent(const GURL& url) const OVERRIDE;
};

DeviceManagementRequestContext::DeviceManagementRequestContext(
    net::URLRequestContext* base_context) {
  // Share resolver, proxy service and ssl bits with the baseline context. This
  // is important so we don't make redundant requests (e.g. when resolving proxy
  // auto configuration).
  set_net_log(base_context->net_log());
  set_host_resolver(base_context->host_resolver());
  set_proxy_service(base_context->proxy_service());
  set_ssl_config_service(base_context->ssl_config_service());

  // Share the http session.
  set_http_transaction_factory(
      new net::HttpNetworkLayer(
          base_context->http_transaction_factory()->GetSession()));

  // No cookies, please.
  set_cookie_store(new net::CookieMonster(NULL, NULL));

  // Initialize these to sane values for our purposes.
  set_accept_language("*");
  set_accept_charset("*");
}

DeviceManagementRequestContext::~DeviceManagementRequestContext() {
  delete http_transaction_factory();
}

const std::string& DeviceManagementRequestContext::GetUserAgent(
    const GURL& url) const {
  return content::GetUserAgent(url);
}

// Request context holder.
class DeviceManagementRequestContextGetter
    : public net::URLRequestContextGetter {
 public:
  explicit DeviceManagementRequestContextGetter(
      net::URLRequestContextGetter* base_context_getter)
      : base_context_getter_(base_context_getter) {}

  // Overridden from net::URLRequestContextGetter:
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy()
      const OVERRIDE;

 private:
  scoped_refptr<net::URLRequestContext> context_;
  scoped_refptr<net::URLRequestContextGetter> base_context_getter_;
};


net::URLRequestContext*
DeviceManagementRequestContextGetter::GetURLRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!context_) {
    context_ = new DeviceManagementRequestContext(
        base_context_getter_->GetURLRequestContext());
  }

  return context_.get();
}

scoped_refptr<base::MessageLoopProxy>
DeviceManagementRequestContextGetter::GetIOMessageLoopProxy() const {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
}

}  // namespace

DeviceManagementService::~DeviceManagementService() {
  // All running jobs should have been cancelled by now. If not, there are
  // backend objects still around, which is an error.
  DCHECK(pending_jobs_.empty());
  DCHECK(queued_jobs_.empty());
}

DeviceManagementBackend* DeviceManagementService::CreateBackend() {
  return new DeviceManagementBackendImpl(this);
}

void DeviceManagementService::ScheduleInitialization(int64 delay_milliseconds) {
  if (initialized_)
    return;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DeviceManagementService::Initialize,
                 weak_ptr_factory_.GetWeakPtr()),
      delay_milliseconds);
}

void DeviceManagementService::Initialize() {
  if (initialized_)
    return;
  DCHECK(!request_context_getter_);
  request_context_getter_ = new DeviceManagementRequestContextGetter(
      g_browser_process->system_request_context());
  initialized_ = true;

  while (!queued_jobs_.empty()) {
    StartJob(queued_jobs_.front(), false);
    queued_jobs_.pop_front();
  }
}

void DeviceManagementService::Shutdown() {
  for (JobFetcherMap::iterator job(pending_jobs_.begin());
       job != pending_jobs_.end();
       ++job) {
    delete job->first;
    queued_jobs_.push_back(job->second);
  }
  pending_jobs_.clear();
}

DeviceManagementService::DeviceManagementService(
    const std::string& server_url)
    : server_url_(server_url),
      initialized_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

void DeviceManagementService::AddJob(DeviceManagementJob* job) {
  if (initialized_)
    StartJob(job, false);
  else
    queued_jobs_.push_back(job);
}

void DeviceManagementService::RemoveJob(DeviceManagementJob* job) {
  for (JobFetcherMap::iterator entry(pending_jobs_.begin());
       entry != pending_jobs_.end();
       ++entry) {
    if (entry->second == job) {
      delete entry->first;
      pending_jobs_.erase(entry);
      return;
    }
  }

  const JobQueue::iterator elem =
      std::find(queued_jobs_.begin(), queued_jobs_.end(), job);
  if (elem != queued_jobs_.end())
    queued_jobs_.erase(elem);
}

void DeviceManagementService::StartJob(DeviceManagementJob* job,
                                       bool bypass_proxy) {
  URLFetcher* fetcher = URLFetcher::Create(0, job->GetURL(server_url_),
                                           URLFetcher::POST, this);
  fetcher->set_load_flags(net::LOAD_DO_NOT_SEND_COOKIES |
                          net::LOAD_DO_NOT_SAVE_COOKIES |
                          net::LOAD_DISABLE_CACHE |
                          (bypass_proxy ? net::LOAD_BYPASS_PROXY : 0));
  fetcher->set_request_context(request_context_getter_.get());
  job->ConfigureRequest(fetcher);
  pending_jobs_[fetcher] = job;
  fetcher->Start();
}

void DeviceManagementService::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const net::ResponseCookies& cookies,
    const std::string& data) {
  JobFetcherMap::iterator entry(pending_jobs_.find(source));
  if (entry != pending_jobs_.end()) {
    DeviceManagementJob* job = entry->second;
    pending_jobs_.erase(entry);

    // Retry the job if it failed due to a broken proxy, by bypassing the
    // proxy on the next try. Don't retry if this URLFetcher already bypassed
    // the proxy.
    bool retry = false;
    if ((source->load_flags() & net::LOAD_BYPASS_PROXY) == 0) {
      if (!status.is_success() && IsProxyError(status)) {
        LOG(WARNING) << "Proxy failed while contacting dmserver.";
        retry = true;
      } else if (status.is_success() &&
                 source->was_fetched_via_proxy() &&
                 !IsProtobufMimeType(source)) {
        // The proxy server can be misconfigured but pointing to an existing
        // server that replies to requests. Try to recover if a successful
        // request that went through a proxy returns an unexpected mime type.
        LOG(WARNING) << "Got bad mime-type in response from dmserver that was "
                     << "fetched via a proxy.";
        retry = true;
      }
    }

    if (retry) {
      LOG(WARNING) << "Retrying dmserver request without using a proxy.";
      StartJob(job, true);
    } else {
      job->HandleResponse(status, response_code, cookies, data);
    }
  } else {
    NOTREACHED() << "Callback from foreign URL fetcher";
  }
  delete source;
}

}  // namespace policy
