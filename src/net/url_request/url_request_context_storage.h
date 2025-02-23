// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_CONTEXT_STORAGE_H_
#define NET_URL_REQUEST_URL_REQUEST_CONTEXT_STORAGE_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"

namespace net {

class CertVerifier;
class CookieStore;
class DnsCertProvenanceChecker;
class DnsRRResolver;
class FraudulentCertificateReporter;
class FtpTransactionFactory;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpServerProperties;
class HttpTransactionFactory;
class NetLog;
class NetworkDelegate;
class OriginBoundCertService;
class ProxyService;
class SSLConfigService;
class TransportSecurityState;
class URLRequestContext;
class URLRequestJobFactory;

// URLRequestContextStorage is a helper class that provides storage for unowned
// member variables of URLRequestContext.
class NET_EXPORT URLRequestContextStorage {
 public:
  // Note that URLRequestContextStorage does not acquire a reference to
  // URLRequestContext, since it is often designed to be embedded in a
  // URLRequestContext subclass.
  explicit URLRequestContextStorage(URLRequestContext* context);
  ~URLRequestContextStorage();

  // These setters will set both the member variables and call the setter on the
  // URLRequestContext object.

  void set_net_log(NetLog* net_log);
  void set_host_resolver(HostResolver* host_resolver);
  void set_cert_verifier(CertVerifier* cert_verifier);
  void set_origin_bound_cert_service(
      OriginBoundCertService* origin_bound_cert_service);
  void set_dnsrr_resolver(DnsRRResolver* dnsrr_resolver);
  void set_dns_cert_checker(DnsCertProvenanceChecker* dns_cert_checker);
  void set_fraudulent_certificate_reporter(
      FraudulentCertificateReporter* fraudulent_certificate_reporter);
  void set_http_auth_handler_factory(
      HttpAuthHandlerFactory* http_auth_handler_factory);
  void set_proxy_service(ProxyService* proxy_service);
  void set_ssl_config_service(SSLConfigService* ssl_config_service);
  void set_network_delegate(NetworkDelegate* network_delegate);
  void set_http_server_properties(HttpServerProperties* http_server_properties);
  void set_cookie_store(CookieStore* cookie_store);
  void set_transport_security_state(
      TransportSecurityState* transport_security_state);
  void set_http_transaction_factory(
      HttpTransactionFactory* http_transaction_factory);
  void set_ftp_transaction_factory(
      FtpTransactionFactory* ftp_transaction_factory);
  void set_job_factory(URLRequestJobFactory* job_factory);

 private:
  // We use a raw pointer to prevent reference cycles, since
  // URLRequestContextStorage can often be contained within a URLRequestContext
  // subclass.
  URLRequestContext* const context_;

  // Owned members.
  scoped_ptr<NetLog> net_log_;
  scoped_ptr<HostResolver> host_resolver_;
  scoped_ptr<CertVerifier> cert_verifier_;
  scoped_ptr<OriginBoundCertService> origin_bound_cert_service_;
  scoped_ptr<DnsRRResolver> dnsrr_resolver_;
  scoped_ptr<DnsCertProvenanceChecker> dns_cert_checker_;
  scoped_ptr<FraudulentCertificateReporter> fraudulent_certificate_reporter_;
  scoped_ptr<HttpAuthHandlerFactory> http_auth_handler_factory_;
  scoped_ptr<ProxyService> proxy_service_;
  // TODO(willchan): Remove refcounting on these members.
  scoped_refptr<SSLConfigService> ssl_config_service_;
  scoped_ptr<NetworkDelegate> network_delegate_;
  scoped_ptr<HttpServerProperties> http_server_properties_;
  scoped_refptr<CookieStore> cookie_store_;
  scoped_ptr<TransportSecurityState> transport_security_state_;

  scoped_ptr<HttpTransactionFactory> http_transaction_factory_;
  scoped_ptr<FtpTransactionFactory> ftp_transaction_factory_;
  scoped_ptr<URLRequestJobFactory> job_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestContextStorage);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_CONTEXT_STORAGE_H_
