// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_url_request_context_getter.h"

#include "base/logging.h"
#include "base/string_split.h"
#include "content/browser/browser_thread.h"
#include "net/base/cert_verifier.h"
#include "net/base/cookie_monster.h"
#include "net/base/default_origin_bound_cert_store.h"
#include "net/base/dnsrr_resolver.h"
#include "net/base/host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_cache.h"
#include "net/base/origin_bound_cert_service.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_job_factory.h"

namespace content {

ShellURLRequestContextGetter::ShellURLRequestContextGetter(
    const FilePath& base_path_,
    MessageLoop* io_loop,
    MessageLoop* file_loop)
    : io_loop_(io_loop),
      file_loop_(file_loop) {
}

ShellURLRequestContextGetter::~ShellURLRequestContextGetter() {
}

net::URLRequestContext* ShellURLRequestContextGetter::GetURLRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!url_request_context_) {
    url_request_context_ = new net::URLRequestContext();
    storage_.reset(new net::URLRequestContextStorage(url_request_context_));

    storage_->set_cookie_store(new net::CookieMonster(NULL, NULL));
    storage_->set_origin_bound_cert_service(new net::OriginBoundCertService(
        new net::DefaultOriginBoundCertStore(NULL)));
    url_request_context_->set_accept_language("en-us,en");
    url_request_context_->set_accept_charset("iso-8859-1,*,utf-8");

    scoped_ptr<net::ProxyConfigService> proxy_config_service(
        net::ProxyService::CreateSystemProxyConfigService(
            io_loop_, file_loop_));
    storage_->set_host_resolver(
        net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                      net::HostResolver::kDefaultRetryAttempts,
                                      NULL));
    storage_->set_cert_verifier(new net::CertVerifier);
    // TODO(jam): use v8 if possible, look at chrome code.
    storage_->set_proxy_service(
        net::ProxyService::CreateUsingSystemProxyResolver(
        proxy_config_service.release(),
        0,
        NULL));
    storage_->set_ssl_config_service(new net::SSLConfigServiceDefaults);
    storage_->set_http_auth_handler_factory(
        net::HttpAuthHandlerFactory::CreateDefault(
            url_request_context_->host_resolver()));
    storage_->set_http_server_properties(new net::HttpServerPropertiesImpl);

    FilePath cache_path = base_path_.Append(FILE_PATH_LITERAL("Cache"));
    net::HttpCache::DefaultBackend* main_backend =
        new net::HttpCache::DefaultBackend(
            net::DISK_CACHE,
            cache_path,
            0,
            BrowserThread::GetMessageLoopProxyForThread(
                BrowserThread::CACHE));

    storage_->set_dnsrr_resolver(new net::DnsRRResolver());

    net::HttpCache* main_cache = new net::HttpCache(
        url_request_context_->host_resolver(),
        url_request_context_->cert_verifier(),
        url_request_context_->origin_bound_cert_service(),
        url_request_context_->dnsrr_resolver(),
        NULL, //dns_cert_checker
        url_request_context_->proxy_service(),
        url_request_context_->ssl_config_service(),
        url_request_context_->http_auth_handler_factory(),
        NULL,  // network_delegate
        url_request_context_->http_server_properties(),
        NULL,
        main_backend);
    storage_->set_http_transaction_factory(main_cache);

    storage_->set_job_factory(new net::URLRequestJobFactory);
  }

  return url_request_context_;
}

net::CookieStore* ShellURLRequestContextGetter::DONTUSEME_GetCookieStore() {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO))
    return GetURLRequestContext()->cookie_store();
  NOTIMPLEMENTED();
  return NULL;
}

scoped_refptr<base::MessageLoopProxy>
    ShellURLRequestContextGetter::GetIOMessageLoopProxy() const {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
}

net::HostResolver* ShellURLRequestContextGetter::host_resolver() {
  return url_request_context_->host_resolver();
}

}  // namespace content
