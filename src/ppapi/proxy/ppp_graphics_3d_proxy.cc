// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_graphics_3d_proxy.h"

#include "ppapi/c/ppp_graphics_3d.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace ppapi {
namespace proxy {

namespace {

void ContextLost(PP_Instance instance) {
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPGraphics3D_ContextLost(INTERFACE_ID_PPP_GRAPHICS_3D,
                                             instance));
}

static const PPP_Graphics3D graphics_3d_interface = {
  &ContextLost
};

InterfaceProxy* CreateGraphics3DProxy(Dispatcher* dispatcher) {
  return new PPP_Graphics3D_Proxy(dispatcher);
}

}  // namespace

PPP_Graphics3D_Proxy::PPP_Graphics3D_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppp_graphics_3d_impl_(NULL) {
  if (dispatcher->IsPlugin()) {
    ppp_graphics_3d_impl_ = static_cast<const PPP_Graphics3D*>(
        dispatcher->local_get_interface()(PPP_GRAPHICS_3D_INTERFACE));
  }
}

PPP_Graphics3D_Proxy::~PPP_Graphics3D_Proxy() {
}

// static
const InterfaceProxy::Info* PPP_Graphics3D_Proxy::GetInfo() {
  static const Info info = {
    &graphics_3d_interface,
    PPP_GRAPHICS_3D_INTERFACE,
    INTERFACE_ID_PPP_GRAPHICS_3D,
    false,
    &CreateGraphics3DProxy,
  };
  return &info;
}

bool PPP_Graphics3D_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_Graphics3D_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPGraphics3D_ContextLost,
                        OnMsgContextLost)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPP_Graphics3D_Proxy::OnMsgContextLost(PP_Instance instance) {
  if (ppp_graphics_3d_impl_)
    ppp_graphics_3d_impl_->Graphics3DContextLost(instance);
}

}  // namespace proxy
}  // namespace ppapi
