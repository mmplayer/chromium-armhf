// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ClientSocketPoolManager manages access to all ClientSocketPools.  It's a
// simple container for all of them.  Most importantly, it handles the lifetime
// and destruction order properly.

#include "net/socket/client_socket_pool_manager.h"

#include <string>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_network_session.h"
#include "net/http/http_proxy_client_socket_pool.h"
#include "net/http/http_request_info.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/ssl_client_socket_pool.h"
#include "net/socket/transport_client_socket_pool.h"

namespace net {

namespace {

// Limit of sockets of each socket pool.
int g_max_sockets_per_pool = 256;

// Default to allow up to 6 connections per host. Experiment and tuning may
// try other values (greater than 0).  Too large may cause many problems, such
// as home routers blocking the connections!?!?  See http://crbug.com/12066.
int g_max_sockets_per_group = 6;

// The max number of sockets to allow per proxy server.  This applies both to
// http and SOCKS proxies.  See http://crbug.com/12066 and
// http://crbug.com/44501 for details about proxy server connection limits.
int g_max_sockets_per_proxy_server = kDefaultMaxSocketsPerProxyServer;

// Appends information about all |socket_pools| to the end of |list|.
template <class MapType>
static void AddSocketPoolsToList(ListValue* list,
                                 const MapType& socket_pools,
                                 const std::string& type,
                                 bool include_nested_pools) {
  for (typename MapType::const_iterator it = socket_pools.begin();
       it != socket_pools.end(); it++) {
    list->Append(it->second->GetInfoAsValue(it->first.ToString(),
                                            type,
                                            include_nested_pools));
  }
}

// The meat of the implementation for the InitSocketHandleForHttpRequest,
// InitSocketHandleForRawConnect and PreconnectSocketsForHttpRequest methods.
int InitSocketPoolHelper(const GURL& request_url,
                         const HttpRequestHeaders& request_extra_headers,
                         int request_load_flags,
                         RequestPriority request_priority,
                         HttpNetworkSession* session,
                         const ProxyInfo& proxy_info,
                         bool force_spdy_over_ssl,
                         bool want_spdy_over_npn,
                         const SSLConfig& ssl_config_for_origin,
                         const SSLConfig& ssl_config_for_proxy,
                         bool force_tunnel,
                         const BoundNetLog& net_log,
                         int num_preconnect_streams,
                         ClientSocketHandle* socket_handle,
                         OldCompletionCallback* callback) {
  scoped_refptr<TransportSocketParams> tcp_params;
  scoped_refptr<HttpProxySocketParams> http_proxy_params;
  scoped_refptr<SOCKSSocketParams> socks_params;
  scoped_ptr<HostPortPair> proxy_host_port;

  GURL request_referrer;
  std::string request_referrer_str;
  if (request_extra_headers.GetHeader(HttpRequestHeaders::kReferer,
                                      &request_referrer_str)) {
    request_referrer = GURL(request_referrer_str);
  }

  bool using_ssl = request_url.SchemeIs("https") || force_spdy_over_ssl;

  HostPortPair origin_host_port =
      HostPortPair(request_url.HostNoBrackets(),
                   request_url.EffectiveIntPort());

  bool disable_resolver_cache =
      request_load_flags & LOAD_BYPASS_CACHE ||
      request_load_flags & LOAD_VALIDATE_CACHE ||
      request_load_flags & LOAD_DISABLE_CACHE;

  int load_flags = request_load_flags;
  if (HttpStreamFactory::ignore_certificate_errors())
    load_flags |= LOAD_IGNORE_ALL_CERT_ERRORS;

  // Build the string used to uniquely identify connections of this type.
  // Determine the host and port to connect to.
  std::string connection_group = origin_host_port.ToString();
  DCHECK(!connection_group.empty());
  if (using_ssl) {
    std::string prefix;
    if (ssl_config_for_origin.tls1_enabled) {
      prefix = "ssl/";
    } else {
      prefix = "sslv3/";
    }
    connection_group = prefix + connection_group;
  }

  bool ignore_limits = (request_load_flags & LOAD_IGNORE_LIMITS) != 0;
  if (proxy_info.is_direct()) {
    tcp_params = new TransportSocketParams(origin_host_port,
                                           request_priority,
                                           request_referrer,
                                           disable_resolver_cache,
                                           ignore_limits);
  } else {
    ProxyServer proxy_server = proxy_info.proxy_server();
    proxy_host_port.reset(new HostPortPair(proxy_server.host_port_pair()));
    scoped_refptr<TransportSocketParams> proxy_tcp_params(
        new TransportSocketParams(*proxy_host_port,
                                  request_priority,
                                  request_referrer,
                                  disable_resolver_cache,
                                  ignore_limits));

    if (proxy_info.is_http() || proxy_info.is_https()) {
      std::string user_agent;
      request_extra_headers.GetHeader(HttpRequestHeaders::kUserAgent,
                                      &user_agent);
      scoped_refptr<SSLSocketParams> ssl_params;
      if (proxy_info.is_https()) {
        // Set ssl_params, and unset proxy_tcp_params
        ssl_params = new SSLSocketParams(proxy_tcp_params,
                                         NULL,
                                         NULL,
                                         ProxyServer::SCHEME_DIRECT,
                                         *proxy_host_port.get(),
                                         ssl_config_for_proxy,
                                         load_flags,
                                         force_spdy_over_ssl,
                                         want_spdy_over_npn);
        proxy_tcp_params = NULL;
      }

      http_proxy_params =
          new HttpProxySocketParams(proxy_tcp_params,
                                    ssl_params,
                                    request_url,
                                    user_agent,
                                    origin_host_port,
                                    session->http_auth_cache(),
                                    session->http_auth_handler_factory(),
                                    session->spdy_session_pool(),
                                    force_tunnel || using_ssl);
    } else {
      DCHECK(proxy_info.is_socks());
      char socks_version;
      if (proxy_server.scheme() == ProxyServer::SCHEME_SOCKS5)
        socks_version = '5';
      else
        socks_version = '4';
      connection_group = base::StringPrintf(
          "socks%c/%s", socks_version, connection_group.c_str());

      socks_params = new SOCKSSocketParams(proxy_tcp_params,
                                           socks_version == '5',
                                           origin_host_port,
                                           request_priority,
                                           request_referrer);
    }
  }

  // Deal with SSL - which layers on top of any given proxy.
  if (using_ssl) {
    scoped_refptr<SSLSocketParams> ssl_params =
        new SSLSocketParams(tcp_params,
                            socks_params,
                            http_proxy_params,
                            proxy_info.proxy_server().scheme(),
                            origin_host_port,
                            ssl_config_for_origin,
                            load_flags,
                            force_spdy_over_ssl,
                            want_spdy_over_npn);
    SSLClientSocketPool* ssl_pool = NULL;
    if (proxy_info.is_direct())
      ssl_pool = session->ssl_socket_pool();
    else
      ssl_pool = session->GetSocketPoolForSSLWithProxy(*proxy_host_port);

    if (num_preconnect_streams) {
      RequestSocketsForPool(ssl_pool, connection_group, ssl_params,
                            num_preconnect_streams, net_log);
      return OK;
    }

    return socket_handle->Init(connection_group, ssl_params,
                               request_priority, callback, ssl_pool,
                               net_log);
  }

  // Finally, get the connection started.
  if (proxy_info.is_http() || proxy_info.is_https()) {
    HttpProxyClientSocketPool* pool =
        session->GetSocketPoolForHTTPProxy(*proxy_host_port);
    if (num_preconnect_streams) {
      RequestSocketsForPool(pool, connection_group, http_proxy_params,
                            num_preconnect_streams, net_log);
      return OK;
    }

    return socket_handle->Init(connection_group, http_proxy_params,
                               request_priority, callback,
                               pool, net_log);
  }

  if (proxy_info.is_socks()) {
    SOCKSClientSocketPool* pool =
        session->GetSocketPoolForSOCKSProxy(*proxy_host_port);
    if (num_preconnect_streams) {
      RequestSocketsForPool(pool, connection_group, socks_params,
                            num_preconnect_streams, net_log);
      return OK;
    }

    return socket_handle->Init(connection_group, socks_params,
                               request_priority, callback, pool,
                               net_log);
  }

  DCHECK(proxy_info.is_direct());

  TransportClientSocketPool* pool = session->transport_socket_pool();
  if (num_preconnect_streams) {
    RequestSocketsForPool(pool, connection_group, tcp_params,
                          num_preconnect_streams, net_log);
    return OK;
  }

  return socket_handle->Init(connection_group, tcp_params,
                             request_priority, callback,
                             pool, net_log);
}

}  // namespace

ClientSocketPoolManager::ClientSocketPoolManager(
    NetLog* net_log,
    ClientSocketFactory* socket_factory,
    HostResolver* host_resolver,
    CertVerifier* cert_verifier,
    OriginBoundCertService* origin_bound_cert_service,
    DnsRRResolver* dnsrr_resolver,
    DnsCertProvenanceChecker* dns_cert_checker,
    SSLHostInfoFactory* ssl_host_info_factory,
    ProxyService* proxy_service,
    SSLConfigService* ssl_config_service)
    : net_log_(net_log),
      socket_factory_(socket_factory),
      host_resolver_(host_resolver),
      cert_verifier_(cert_verifier),
      origin_bound_cert_service_(origin_bound_cert_service),
      dnsrr_resolver_(dnsrr_resolver),
      dns_cert_checker_(dns_cert_checker),
      ssl_host_info_factory_(ssl_host_info_factory),
      proxy_service_(proxy_service),
      ssl_config_service_(ssl_config_service),
      transport_pool_histograms_("TCP"),
      transport_socket_pool_(new TransportClientSocketPool(
          g_max_sockets_per_pool, g_max_sockets_per_group,
          &transport_pool_histograms_,
          host_resolver,
          socket_factory_,
          net_log)),
      ssl_pool_histograms_("SSL2"),
      ssl_socket_pool_(new SSLClientSocketPool(
          g_max_sockets_per_pool, g_max_sockets_per_group,
          &ssl_pool_histograms_,
          host_resolver,
          cert_verifier,
          origin_bound_cert_service,
          dnsrr_resolver,
          dns_cert_checker,
          ssl_host_info_factory,
          socket_factory,
          transport_socket_pool_.get(),
          NULL /* no socks proxy */,
          NULL /* no http proxy */,
          ssl_config_service,
          net_log)),
      transport_for_socks_pool_histograms_("TCPforSOCKS"),
      socks_pool_histograms_("SOCK"),
      transport_for_http_proxy_pool_histograms_("TCPforHTTPProxy"),
      transport_for_https_proxy_pool_histograms_("TCPforHTTPSProxy"),
      ssl_for_https_proxy_pool_histograms_("SSLforHTTPSProxy"),
      http_proxy_pool_histograms_("HTTPProxy"),
      ssl_socket_pool_for_proxies_histograms_("SSLForProxies") {
  CertDatabase::AddObserver(this);
}

ClientSocketPoolManager::~ClientSocketPoolManager() {
  CertDatabase::RemoveObserver(this);
}

void ClientSocketPoolManager::FlushSocketPools() {
  // Flush the highest level pools first, since higher level pools may release
  // stuff to the lower level pools.

  for (SSLSocketPoolMap::const_iterator it =
       ssl_socket_pools_for_proxies_.begin();
       it != ssl_socket_pools_for_proxies_.end();
       ++it)
    it->second->Flush();

  for (HTTPProxySocketPoolMap::const_iterator it =
       http_proxy_socket_pools_.begin();
       it != http_proxy_socket_pools_.end();
       ++it)
    it->second->Flush();

  for (SSLSocketPoolMap::const_iterator it =
       ssl_socket_pools_for_https_proxies_.begin();
       it != ssl_socket_pools_for_https_proxies_.end();
       ++it)
    it->second->Flush();

  for (TransportSocketPoolMap::const_iterator it =
       transport_socket_pools_for_https_proxies_.begin();
       it != transport_socket_pools_for_https_proxies_.end();
       ++it)
    it->second->Flush();

  for (TransportSocketPoolMap::const_iterator it =
       transport_socket_pools_for_http_proxies_.begin();
       it != transport_socket_pools_for_http_proxies_.end();
       ++it)
    it->second->Flush();

  for (SOCKSSocketPoolMap::const_iterator it =
       socks_socket_pools_.begin();
       it != socks_socket_pools_.end();
       ++it)
    it->second->Flush();

  for (TransportSocketPoolMap::const_iterator it =
       transport_socket_pools_for_socks_proxies_.begin();
       it != transport_socket_pools_for_socks_proxies_.end();
       ++it)
    it->second->Flush();

  ssl_socket_pool_->Flush();
  transport_socket_pool_->Flush();
}

void ClientSocketPoolManager::CloseIdleSockets() {
  // Close sockets in the highest level pools first, since higher level pools'
  // sockets may release stuff to the lower level pools.
  for (SSLSocketPoolMap::const_iterator it =
       ssl_socket_pools_for_proxies_.begin();
       it != ssl_socket_pools_for_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (HTTPProxySocketPoolMap::const_iterator it =
       http_proxy_socket_pools_.begin();
       it != http_proxy_socket_pools_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (SSLSocketPoolMap::const_iterator it =
       ssl_socket_pools_for_https_proxies_.begin();
       it != ssl_socket_pools_for_https_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (TransportSocketPoolMap::const_iterator it =
       transport_socket_pools_for_https_proxies_.begin();
       it != transport_socket_pools_for_https_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (TransportSocketPoolMap::const_iterator it =
       transport_socket_pools_for_http_proxies_.begin();
       it != transport_socket_pools_for_http_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (SOCKSSocketPoolMap::const_iterator it =
       socks_socket_pools_.begin();
       it != socks_socket_pools_.end();
       ++it)
    it->second->CloseIdleSockets();

  for (TransportSocketPoolMap::const_iterator it =
       transport_socket_pools_for_socks_proxies_.begin();
       it != transport_socket_pools_for_socks_proxies_.end();
       ++it)
    it->second->CloseIdleSockets();

  ssl_socket_pool_->CloseIdleSockets();
  transport_socket_pool_->CloseIdleSockets();
}

SOCKSClientSocketPool* ClientSocketPoolManager::GetSocketPoolForSOCKSProxy(
    const HostPortPair& socks_proxy) {
  SOCKSSocketPoolMap::const_iterator it = socks_socket_pools_.find(socks_proxy);
  if (it != socks_socket_pools_.end()) {
    DCHECK(ContainsKey(transport_socket_pools_for_socks_proxies_, socks_proxy));
    return it->second;
  }

  DCHECK(!ContainsKey(transport_socket_pools_for_socks_proxies_, socks_proxy));

  std::pair<TransportSocketPoolMap::iterator, bool> tcp_ret =
      transport_socket_pools_for_socks_proxies_.insert(
          std::make_pair(
              socks_proxy,
              new TransportClientSocketPool(
                  g_max_sockets_per_proxy_server, g_max_sockets_per_group,
                  &transport_for_socks_pool_histograms_,
                  host_resolver_,
                  socket_factory_,
                  net_log_)));
  DCHECK(tcp_ret.second);

  std::pair<SOCKSSocketPoolMap::iterator, bool> ret =
      socks_socket_pools_.insert(
          std::make_pair(socks_proxy, new SOCKSClientSocketPool(
              g_max_sockets_per_proxy_server, g_max_sockets_per_group,
              &socks_pool_histograms_,
              host_resolver_,
              tcp_ret.first->second,
              net_log_)));

  return ret.first->second;
}

HttpProxyClientSocketPool* ClientSocketPoolManager::GetSocketPoolForHTTPProxy(
    const HostPortPair& http_proxy) {
  HTTPProxySocketPoolMap::const_iterator it =
      http_proxy_socket_pools_.find(http_proxy);
  if (it != http_proxy_socket_pools_.end()) {
    DCHECK(ContainsKey(transport_socket_pools_for_http_proxies_, http_proxy));
    DCHECK(ContainsKey(transport_socket_pools_for_https_proxies_, http_proxy));
    DCHECK(ContainsKey(ssl_socket_pools_for_https_proxies_, http_proxy));
    return it->second;
  }

  DCHECK(!ContainsKey(transport_socket_pools_for_http_proxies_, http_proxy));
  DCHECK(!ContainsKey(transport_socket_pools_for_https_proxies_, http_proxy));
  DCHECK(!ContainsKey(ssl_socket_pools_for_https_proxies_, http_proxy));

  std::pair<TransportSocketPoolMap::iterator, bool> tcp_http_ret =
      transport_socket_pools_for_http_proxies_.insert(
          std::make_pair(
              http_proxy,
              new TransportClientSocketPool(
                  g_max_sockets_per_proxy_server, g_max_sockets_per_group,
                  &transport_for_http_proxy_pool_histograms_,
                  host_resolver_,
                  socket_factory_,
                  net_log_)));
  DCHECK(tcp_http_ret.second);

  std::pair<TransportSocketPoolMap::iterator, bool> tcp_https_ret =
      transport_socket_pools_for_https_proxies_.insert(
          std::make_pair(
              http_proxy,
              new TransportClientSocketPool(
                  g_max_sockets_per_proxy_server, g_max_sockets_per_group,
                  &transport_for_https_proxy_pool_histograms_,
                  host_resolver_,
                  socket_factory_,
                  net_log_)));
  DCHECK(tcp_https_ret.second);

  std::pair<SSLSocketPoolMap::iterator, bool> ssl_https_ret =
      ssl_socket_pools_for_https_proxies_.insert(
          std::make_pair(
              http_proxy,
              new SSLClientSocketPool(
                  g_max_sockets_per_proxy_server, g_max_sockets_per_group,
                  &ssl_for_https_proxy_pool_histograms_,
                  host_resolver_,
                  cert_verifier_,
                  origin_bound_cert_service_,
                  dnsrr_resolver_,
                  dns_cert_checker_,
                  ssl_host_info_factory_,
                  socket_factory_,
                  tcp_https_ret.first->second /* https proxy */,
                  NULL /* no socks proxy */,
                  NULL /* no http proxy */,
                  ssl_config_service_, net_log_)));
  DCHECK(tcp_https_ret.second);

  std::pair<HTTPProxySocketPoolMap::iterator, bool> ret =
      http_proxy_socket_pools_.insert(
          std::make_pair(
              http_proxy,
              new HttpProxyClientSocketPool(
                  g_max_sockets_per_proxy_server, g_max_sockets_per_group,
                  &http_proxy_pool_histograms_,
                  host_resolver_,
                  tcp_http_ret.first->second,
                  ssl_https_ret.first->second,
                  net_log_)));

  return ret.first->second;
}

SSLClientSocketPool* ClientSocketPoolManager::GetSocketPoolForSSLWithProxy(
    const HostPortPair& proxy_server) {
  SSLSocketPoolMap::const_iterator it =
      ssl_socket_pools_for_proxies_.find(proxy_server);
  if (it != ssl_socket_pools_for_proxies_.end())
    return it->second;

  SSLClientSocketPool* new_pool = new SSLClientSocketPool(
      g_max_sockets_per_proxy_server, g_max_sockets_per_group,
      &ssl_pool_histograms_,
      host_resolver_,
      cert_verifier_,
      origin_bound_cert_service_,
      dnsrr_resolver_,
      dns_cert_checker_,
      ssl_host_info_factory_,
      socket_factory_,
      NULL, /* no tcp pool, we always go through a proxy */
      GetSocketPoolForSOCKSProxy(proxy_server),
      GetSocketPoolForHTTPProxy(proxy_server),
      ssl_config_service_,
      net_log_);

  std::pair<SSLSocketPoolMap::iterator, bool> ret =
      ssl_socket_pools_for_proxies_.insert(std::make_pair(proxy_server,
                                                          new_pool));

  return ret.first->second;
}

// static
void ClientSocketPoolManager::set_max_sockets_per_pool(int socket_count) {
  DCHECK_LT(0, socket_count);
  DCHECK_GT(1000, socket_count);  // Sanity check.
  g_max_sockets_per_pool = socket_count;
  DCHECK_GE(g_max_sockets_per_pool, g_max_sockets_per_group);
}

// static
int ClientSocketPoolManager::max_sockets_per_group() {
  return g_max_sockets_per_group;
}

// static
void ClientSocketPoolManager::set_max_sockets_per_group(int socket_count) {
  DCHECK_LT(0, socket_count);
  // The following is a sanity check... but we should NEVER be near this value.
  DCHECK_GT(100, socket_count);
  g_max_sockets_per_group = socket_count;

  DCHECK_GE(g_max_sockets_per_pool, g_max_sockets_per_group);
  DCHECK_GE(g_max_sockets_per_proxy_server, g_max_sockets_per_group);
}

// static
void ClientSocketPoolManager::set_max_sockets_per_proxy_server(
    int socket_count) {
  DCHECK_LT(0, socket_count);
  DCHECK_GT(100, socket_count);  // Sanity check.
  // Assert this case early on. The max number of sockets per group cannot
  // exceed the max number of sockets per proxy server.
  DCHECK_LE(g_max_sockets_per_group, socket_count);
  g_max_sockets_per_proxy_server = socket_count;
}

Value* ClientSocketPoolManager::SocketPoolInfoToValue() const {
  ListValue* list = new ListValue();
  list->Append(transport_socket_pool_->GetInfoAsValue("transport_socket_pool",
                                                "transport_socket_pool",
                                                false));
  // Third parameter is false because |ssl_socket_pool_| uses
  // |transport_socket_pool_| internally, and do not want to add it a second
  // time.
  list->Append(ssl_socket_pool_->GetInfoAsValue("ssl_socket_pool",
                                                "ssl_socket_pool",
                                                false));
  AddSocketPoolsToList(list,
                       http_proxy_socket_pools_,
                       "http_proxy_socket_pool",
                       true);
  AddSocketPoolsToList(list,
                       socks_socket_pools_,
                       "socks_socket_pool",
                       true);

  // Third parameter is false because |ssl_socket_pools_for_proxies_| use
  // socket pools in |http_proxy_socket_pools_| and |socks_socket_pools_|.
  AddSocketPoolsToList(list,
                       ssl_socket_pools_for_proxies_,
                       "ssl_socket_pool_for_proxies",
                       false);
  return list;
}

void ClientSocketPoolManager::OnUserCertAdded(const X509Certificate* cert) {
  FlushSocketPools();
}

void ClientSocketPoolManager::OnCertTrustChanged(const X509Certificate* cert) {
  // We should flush the socket pools if we removed trust from a
  // cert, because a previously trusted server may have become
  // untrusted.
  //
  // We should not flush the socket pools if we added trust to a
  // cert.
  //
  // Since the OnCertTrustChanged method doesn't tell us what
  // kind of trust change it is, we have to flush the socket
  // pools to be safe.
  FlushSocketPools();
}

// static
int ClientSocketPoolManager::InitSocketHandleForHttpRequest(
    const GURL& request_url,
    const HttpRequestHeaders& request_extra_headers,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    bool force_spdy_over_ssl,
    bool want_spdy_over_npn,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    const BoundNetLog& net_log,
    ClientSocketHandle* socket_handle,
    OldCompletionCallback* callback) {
  DCHECK(socket_handle);
  return InitSocketPoolHelper(request_url,
                              request_extra_headers,
                              request_load_flags,
                              request_priority,
                              session,
                              proxy_info,
                              force_spdy_over_ssl,
                              want_spdy_over_npn,
                              ssl_config_for_origin,
                              ssl_config_for_proxy,
                              false,
                              net_log,
                              0,
                              socket_handle,
                              callback);
}

// static
int ClientSocketPoolManager::InitSocketHandleForRawConnect(
    const HostPortPair& host_port_pair,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    const BoundNetLog& net_log,
    ClientSocketHandle* socket_handle,
    OldCompletionCallback* callback) {
  DCHECK(socket_handle);
  // Synthesize an HttpRequestInfo.
  GURL request_url = GURL("http://" + host_port_pair.ToString());
  HttpRequestHeaders request_extra_headers;
  int request_load_flags = 0;
  RequestPriority request_priority = MEDIUM;

  return InitSocketPoolHelper(request_url,
                              request_extra_headers,
                              request_load_flags,
                              request_priority,
                              session,
                              proxy_info,
                              false,
                              false,
                              ssl_config_for_origin,
                              ssl_config_for_proxy,
                              true,
                              net_log,
                              0,
                              socket_handle,
                              callback);
}

// static
int ClientSocketPoolManager::PreconnectSocketsForHttpRequest(
    const GURL& request_url,
    const HttpRequestHeaders& request_extra_headers,
    int request_load_flags,
    RequestPriority request_priority,
    HttpNetworkSession* session,
    const ProxyInfo& proxy_info,
    bool force_spdy_over_ssl,
    bool want_spdy_over_npn,
    const SSLConfig& ssl_config_for_origin,
    const SSLConfig& ssl_config_for_proxy,
    const BoundNetLog& net_log,
    int num_preconnect_streams) {
  return InitSocketPoolHelper(request_url,
                              request_extra_headers,
                              request_load_flags,
                              request_priority,
                              session,
                              proxy_info,
                              force_spdy_over_ssl,
                              want_spdy_over_npn,
                              ssl_config_for_origin,
                              ssl_config_for_proxy,
                              false,
                              net_log,
                              num_preconnect_streams,
                              NULL,
                              NULL);
}


}  // namespace net
