// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_CORE_PROXY_H_
#define PPAPI_PPB_CORE_PROXY_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/host_resource.h"

struct PPB_Core;

namespace ppapi {
namespace proxy {

class PPB_Core_Proxy : public InterfaceProxy {
 public:
  PPB_Core_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Core_Proxy();

  static const PPB_Core* GetPPB_Core_Interface();

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const InterfaceID kInterfaceID = INTERFACE_ID_PPB_CORE;

 private:
  // Message handlers.
  void OnMsgAddRefResource(const ppapi::HostResource& resource);
  void OnMsgReleaseResource(const ppapi::HostResource& resource);

  // When this proxy is in the host side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the plugin, this value is always NULL.
  const PPB_Core* ppb_core_impl_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PPB_CORE_PROXY_H_
