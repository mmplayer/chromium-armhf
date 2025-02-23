// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_TESTING_PROXY_H_
#define PPAPI_PROXY_PPB_TESTING_PROXY_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/host_resource.h"

struct PP_Point;
struct PPB_Testing_Dev;

namespace ppapi {
namespace proxy {

class PPB_Testing_Proxy : public InterfaceProxy {
 public:
  PPB_Testing_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Testing_Proxy();

  static const Info* GetInfo();

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgReadImageData(const ppapi::HostResource& device_context_2d,
                          const ppapi::HostResource& image,
                          const PP_Point& top_left,
                          PP_Bool* result);
  void OnMsgRunMessageLoop(PP_Instance instance);
  void OnMsgQuitMessageLoop(PP_Instance instance);
  void OnMsgGetLiveObjectsForInstance(PP_Instance instance, uint32_t* result);

  // When this proxy is in the host side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the plugin, this value is always NULL.
  const PPB_Testing_Dev* ppb_testing_impl_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Testing_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_TESTING_PROXY_H_
