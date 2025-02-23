// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_mouse_lock_proxy.h"

#include "ppapi/c/ppp_mouse_lock.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace ppapi {
namespace proxy {

namespace {

void MouseLockLost(PP_Instance instance) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    // The dispatcher should always be valid.
    NOTREACHED();
    return;
  }

  dispatcher->Send(new PpapiMsg_PPPMouseLock_MouseLockLost(
      INTERFACE_ID_PPP_MOUSE_LOCK, instance));
}

static const PPP_MouseLock mouse_lock_interface = {
  &MouseLockLost
};

InterfaceProxy* CreateMouseLockProxy(Dispatcher* dispatcher) {
  return new PPP_MouseLock_Proxy(dispatcher);
}

}  // namespace

PPP_MouseLock_Proxy::PPP_MouseLock_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
  if (dispatcher->IsPlugin()) {
    ppp_mouse_lock_impl_ = static_cast<const PPP_MouseLock*>(
        dispatcher->local_get_interface()(PPP_MOUSELOCK_INTERFACE));
  }
}

PPP_MouseLock_Proxy::~PPP_MouseLock_Proxy() {
}

// static
const InterfaceProxy::Info* PPP_MouseLock_Proxy::GetInfo() {
  static const Info info = {
    &mouse_lock_interface,
    PPP_MOUSELOCK_INTERFACE,
    INTERFACE_ID_PPP_MOUSE_LOCK,
    false,
    &CreateMouseLockProxy,
  };
  return &info;
}

bool PPP_MouseLock_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_MouseLock_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPMouseLock_MouseLockLost,
                        OnMsgMouseLockLost)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPP_MouseLock_Proxy::OnMsgMouseLockLost(PP_Instance instance) {
  if (ppp_mouse_lock_impl_)
    ppp_mouse_lock_impl_->MouseLockLost(instance);
}

}  // namespace proxy
}  // namespace ppapi
