// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROXY_CROS_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_PROXY_CROS_SETTINGS_PROVIDER_H_
#pragma once

#include "base/memory/singleton.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros_settings_provider.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"

namespace chromeos {

class ProxyCrosSettingsProvider : public CrosSettingsProvider {
 public:
  ProxyCrosSettingsProvider();
  // CrosSettingsProvider implementation.
  virtual bool Get(const std::string& path, Value** out_value) const OVERRIDE;
  virtual bool HandlesSetting(const std::string& path) const OVERRIDE;

  // Set the current network whose proxy settings will be displayed and possibly
  // edited on.
  void SetCurrentNetwork(const std::string& network);

  // Make the active network the current one whose proxy settings will be
  // displayed and possibly edited on.
  void MakeActiveNetworkCurrent();

  // Returns name of current network that has been set via SetCurrentNetwork or
  // MakeActiveNetworkCurrent.
  const std::string& GetCurrentNetworkName() const;

  // Returns true if user has selected to use shared proxies.
  bool IsUsingSharedProxies() const;

 private:
  // CrosSettingsProvider implementation.
  virtual void DoSet(const std::string& path, Value* value) OVERRIDE;

  chromeos::ProxyConfigServiceImpl* GetConfigService() const;

  net::ProxyServer CreateProxyServerFromHost(
      const std::string& host,
      const ProxyConfigServiceImpl::ProxyConfig::ManualProxy& proxy,
      net::ProxyServer::Scheme scheme) const;

  net::ProxyServer CreateProxyServerFromPort(
      uint16 port,
      const ProxyConfigServiceImpl::ProxyConfig::ManualProxy& proxy,
      net::ProxyServer::Scheme scheme) const;

  Value* CreateServerHostValue(
      const ProxyConfigServiceImpl::ProxyConfig::ManualProxy& proxy) const;

  Value* CreateServerPortValue(
      const ProxyConfigServiceImpl::ProxyConfig::ManualProxy& proxy) const;

  DISALLOW_COPY_AND_ASSIGN(ProxyCrosSettingsProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PROXY_CROS_SETTINGS_PROVIDER_H_
