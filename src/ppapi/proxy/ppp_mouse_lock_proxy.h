// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPP_MOUSE_LOCK_PROXY_H_
#define PPAPI_PROXY_PPP_MOUSE_LOCK_PROXY_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPP_MouseLock;

namespace ppapi {
namespace proxy {

class PPP_MouseLock_Proxy : public InterfaceProxy {
 public:
  PPP_MouseLock_Proxy(Dispatcher* dispatcher);
  virtual ~PPP_MouseLock_Proxy();

  static const Info* GetInfo();

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

 private:
  // Message handlers.
  void OnMsgMouseLockLost(PP_Instance instance);

  // When this proxy is in the plugin side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the host, this value is always NULL.
  const PPP_MouseLock* ppp_mouse_lock_impl_;

  DISALLOW_COPY_AND_ASSIGN(PPP_MouseLock_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPP_MOUSE_LOCK_PROXY_H_
