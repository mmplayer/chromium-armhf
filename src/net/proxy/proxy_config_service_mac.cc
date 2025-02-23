// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_config_service_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/message_loop.h"
#include "base/sys_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_server.h"

namespace net {

namespace {

const int kPollIntervalSec = 5;

// Utility function to pull out a boolean value from a dictionary and return it,
// returning a default value if the key is not present.
bool GetBoolFromDictionary(CFDictionaryRef dict,
                           CFStringRef key,
                           bool default_value) {
  CFNumberRef number = (CFNumberRef)base::mac::GetValueFromDictionary(
      dict, key, CFNumberGetTypeID());
  if (!number)
    return default_value;

  int int_value;
  if (CFNumberGetValue(number, kCFNumberIntType, &int_value))
    return int_value;
  else
    return default_value;
}

void GetCurrentProxyConfig(ProxyConfig* config) {
  base::mac::ScopedCFTypeRef<CFDictionaryRef> config_dict(
      SCDynamicStoreCopyProxies(NULL));
  DCHECK(config_dict);

  // auto-detect

  // There appears to be no UI for this configuration option, and we're not sure
  // if Apple's proxy code even takes it into account. But the constant is in
  // the header file so we'll use it.
  config->set_auto_detect(
      GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesProxyAutoDiscoveryEnable,
                            false));

  // PAC file

  if (GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesProxyAutoConfigEnable,
                            false)) {
    CFStringRef pac_url_ref = (CFStringRef)base::mac::GetValueFromDictionary(
        config_dict.get(),
        kSCPropNetProxiesProxyAutoConfigURLString,
        CFStringGetTypeID());
    if (pac_url_ref)
      config->set_pac_url(GURL(base::SysCFStringRefToUTF8(pac_url_ref)));
  }

  // proxies (for now ftp, http, https, and SOCKS)

  if (GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesFTPEnable,
                            false)) {
    ProxyServer proxy_server =
        ProxyServer::FromDictionary(ProxyServer::SCHEME_HTTP,
                                    config_dict.get(),
                                    kSCPropNetProxiesFTPProxy,
                                    kSCPropNetProxiesFTPPort);
    if (proxy_server.is_valid()) {
      config->proxy_rules().type =
          ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;
      config->proxy_rules().proxy_for_ftp = proxy_server;
    }
  }
  if (GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesHTTPEnable,
                            false)) {
    ProxyServer proxy_server =
        ProxyServer::FromDictionary(ProxyServer::SCHEME_HTTP,
                                    config_dict.get(),
                                    kSCPropNetProxiesHTTPProxy,
                                    kSCPropNetProxiesHTTPPort);
    if (proxy_server.is_valid()) {
      config->proxy_rules().type =
          ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;
      config->proxy_rules().proxy_for_http = proxy_server;
    }
  }
  if (GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesHTTPSEnable,
                            false)) {
    ProxyServer proxy_server =
        ProxyServer::FromDictionary(ProxyServer::SCHEME_HTTP,
                                    config_dict.get(),
                                    kSCPropNetProxiesHTTPSProxy,
                                    kSCPropNetProxiesHTTPSPort);
    if (proxy_server.is_valid()) {
      config->proxy_rules().type =
          ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;
      config->proxy_rules().proxy_for_https = proxy_server;
    }
  }
  if (GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesSOCKSEnable,
                            false)) {
    ProxyServer proxy_server =
        ProxyServer::FromDictionary(ProxyServer::SCHEME_SOCKS5,
                                    config_dict.get(),
                                    kSCPropNetProxiesSOCKSProxy,
                                    kSCPropNetProxiesSOCKSPort);
    if (proxy_server.is_valid()) {
      config->proxy_rules().type =
          ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;
      config->proxy_rules().fallback_proxy = proxy_server;
    }
  }

  // proxy bypass list

  CFArrayRef bypass_array_ref =
      (CFArrayRef)base::mac::GetValueFromDictionary(
          config_dict.get(),
          kSCPropNetProxiesExceptionsList,
          CFArrayGetTypeID());
  if (bypass_array_ref) {
    CFIndex bypass_array_count = CFArrayGetCount(bypass_array_ref);
    for (CFIndex i = 0; i < bypass_array_count; ++i) {
      CFStringRef bypass_item_ref =
          (CFStringRef)CFArrayGetValueAtIndex(bypass_array_ref, i);
      if (CFGetTypeID(bypass_item_ref) != CFStringGetTypeID()) {
        LOG(WARNING) << "Expected value for item " << i
                     << " in the kSCPropNetProxiesExceptionsList"
                        " to be a CFStringRef but it was not";

      } else {
        config->proxy_rules().bypass_rules.AddRuleFromString(
            base::SysCFStringRefToUTF8(bypass_item_ref));
      }
    }
  }

  // proxy bypass boolean

  if (GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesExcludeSimpleHostnames,
                            false)) {
    config->proxy_rules().bypass_rules.AddRuleToBypassLocal();
  }
}

}  // namespace

// Reference-counted helper for posting a task to
// ProxyConfigServiceMac::OnProxyConfigChanged between the notifier and IO
// thread. This helper object may outlive the ProxyConfigServiceMac.
class ProxyConfigServiceMac::Helper
    : public base::RefCountedThreadSafe<ProxyConfigServiceMac::Helper> {
 public:
  explicit Helper(ProxyConfigServiceMac* parent) : parent_(parent) {
    DCHECK(parent);
  }

  // Called when the parent is destroyed.
  void Orphan() {
    parent_ = NULL;
  }

  void OnProxyConfigChanged(const ProxyConfig& new_config) {
    if (parent_)
      parent_->OnProxyConfigChanged(new_config);
  }

 private:
  ProxyConfigServiceMac* parent_;
};

ProxyConfigServiceMac::ProxyConfigServiceMac(MessageLoop* io_loop)
    : forwarder_(this),
      has_fetched_config_(false),
      helper_(new Helper(this)),
      io_loop_(io_loop) {
  DCHECK(io_loop);
  config_watcher_.reset(new NetworkConfigWatcherMac(&forwarder_));
}

ProxyConfigServiceMac::~ProxyConfigServiceMac() {
  DCHECK_EQ(io_loop_, MessageLoop::current());
  // Delete the config_watcher_ to ensure the notifier thread finishes before
  // this object is destroyed.
  config_watcher_.reset();
  helper_->Orphan();
}

void ProxyConfigServiceMac::AddObserver(Observer* observer) {
  DCHECK_EQ(io_loop_, MessageLoop::current());
  observers_.AddObserver(observer);
}

void ProxyConfigServiceMac::RemoveObserver(Observer* observer) {
  DCHECK_EQ(io_loop_, MessageLoop::current());
  observers_.RemoveObserver(observer);
}

net::ProxyConfigService::ConfigAvailability
    ProxyConfigServiceMac::GetLatestProxyConfig(ProxyConfig* config) {
  DCHECK_EQ(io_loop_, MessageLoop::current());

  // Lazy-initialize by fetching the proxy setting from this thread.
  if (!has_fetched_config_) {
    GetCurrentProxyConfig(&last_config_fetched_);
    has_fetched_config_ = true;
  }

  *config = last_config_fetched_;
  return has_fetched_config_ ? CONFIG_VALID : CONFIG_PENDING;
}

void ProxyConfigServiceMac::SetDynamicStoreNotificationKeys(
    SCDynamicStoreRef store) {
  // Called on notifier thread.

  CFStringRef proxies_key = SCDynamicStoreKeyCreateProxies(NULL);
  CFArrayRef key_array = CFArrayCreate(
      NULL, (const void **)(&proxies_key), 1, &kCFTypeArrayCallBacks);

  bool ret = SCDynamicStoreSetNotificationKeys(store, key_array, NULL);
  // TODO(willchan): Figure out a proper way to handle this rather than crash.
  CHECK(ret);

  CFRelease(key_array);
  CFRelease(proxies_key);
}

void ProxyConfigServiceMac::OnNetworkConfigChange(CFArrayRef changed_keys) {
  // Called on notifier thread.

  // Fetch the new system proxy configuration.
  ProxyConfig new_config;
  GetCurrentProxyConfig(&new_config);

  // Call OnProxyConfigChanged() on the IO thread to notify our observers.
  io_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          helper_.get(), &Helper::OnProxyConfigChanged, new_config));
}

void ProxyConfigServiceMac::OnProxyConfigChanged(
    const ProxyConfig& new_config) {
  DCHECK_EQ(io_loop_, MessageLoop::current());

  // Keep track of the last value we have seen.
  has_fetched_config_ = true;
  last_config_fetched_ = new_config;

  // Notify all the observers.
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnProxyConfigChanged(new_config, CONFIG_VALID));
}

}  // namespace net
