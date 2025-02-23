// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_DISPATCHER_H_
#define PPAPI_PROXY_DISPATCHER_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/tracked_objects.h"
#include "ipc/ipc_channel_proxy.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/proxy/callback_tracker.h"
#include "ppapi/proxy/proxy_channel.h"
#include "ppapi/proxy/interface_id.h"
#include "ppapi/proxy/interface_list.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/plugin_var_tracker.h"

namespace ppapi {

class WebKitForwarding;

namespace proxy {

class VarSerializationRules;

// An interface proxy can represent either end of a cross-process interface
// call. The "source" side is where the call is invoked, and the "target" side
// is where the call ends up being executed.
//
// Plugin side                          | Browser side
// -------------------------------------|--------------------------------------
//                                      |
//    "Source"                          |    "Target"
//    InterfaceProxy ----------------------> InterfaceProxy
//                                      |
//                                      |
//    "Target"                          |    "Source"
//    InterfaceProxy <---------------------- InterfaceProxy
//                                      |
class PPAPI_PROXY_EXPORT Dispatcher : public ProxyChannel {
 public:
  typedef const void* (*GetInterfaceFunc)(const char*);
  typedef int32_t (*InitModuleFunc)(PP_Module, GetInterfaceFunc);

  virtual ~Dispatcher();

  // Returns true if the dispatcher is on the plugin side, or false if it's the
  // browser side.
  virtual bool IsPlugin() const = 0;

  VarSerializationRules* serialization_rules() const {
    return serialization_rules_.get();
  }

  // Returns a non-owning pointer to the interface proxy for the given ID, or
  // NULL if the ID isn't found. This will create the proxy if it hasn't been
  // created so far.
  InterfaceProxy* GetInterfaceProxy(InterfaceID id);

  // Returns the pointer to the IO thread for processing IPC messages.
  // TODO(brettw) remove this. It's a hack to support the Flash
  // ModuleLocalThreadAdapter. When the thread stuff is sorted out, this
  // implementation detail should be hidden.
  base::MessageLoopProxy* GetIPCMessageLoop();

  // Adds the given filter to the IO thread. Takes ownership of the pointer.
  // TODO(brettw) remove this. It's a hack to support the Flash
  // ModuleLocalThreadAdapter. When the thread stuff is sorted out, this
  // implementation detail should be hidden.
  void AddIOThreadMessageFilter(IPC::ChannelProxy::MessageFilter* filter);

  // TODO(brettw): What is this comment referring to?
  // Called if the remote side is declaring to us which interfaces it supports
  // so we don't have to query for each one. We'll pre-create proxies for
  // each of the given interfaces.

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  CallbackTracker& callback_tracker() {
    return callback_tracker_;
  }

  GetInterfaceFunc local_get_interface() const { return local_get_interface_; }

 protected:
  Dispatcher(base::ProcessHandle remote_process_handle,
             GetInterfaceFunc local_get_interface);

  // Setter for the derived classes to set the appropriate var serialization.
  // Takes ownership of the given pointer, which must be on the heap.
  void SetSerializationRules(VarSerializationRules* var_serialization_rules);

  // Called when an invalid message is received from the remote site. The
  // default implementation does nothing, derived classes can override.
  virtual void OnInvalidMessageReceived();

  bool disallow_trusted_interfaces() const {
    return disallow_trusted_interfaces_;
  }

 private:
  friend class HostDispatcherTest;
  friend class PluginDispatcherTest;

  // Lists all lazily-created interface proxies.
  scoped_ptr<InterfaceProxy> proxies_[INTERFACE_ID_COUNT];

  bool disallow_trusted_interfaces_;

  GetInterfaceFunc local_get_interface_;

  CallbackTracker callback_tracker_;

  scoped_ptr<VarSerializationRules> serialization_rules_;

  DISALLOW_COPY_AND_ASSIGN(Dispatcher);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_DISPATCHER_H_
