// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_VAR_DEPRECATED_PROXY_H_
#define PPAPI_PPB_VAR_DEPRECATED_PROXY_H_

#include <vector>

#include "base/task.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_Var_Deprecated;

namespace ppapi {
namespace proxy {

class SerializedVar;
class SerializedVarArray;
class SerializedVarReceiveInput;
class SerializedVarVectorOutParam;
class SerializedVarVectorReceiveInput;
class SerializedVarOutParam;
class SerializedVarReturnValue;

class PPB_Var_Deprecated_Proxy : public InterfaceProxy {
 public:
  PPB_Var_Deprecated_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Var_Deprecated_Proxy();

  static const Info* GetInfo();

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgAddRefObject(int64 object_id, int* unused);
  void OnMsgReleaseObject(int64 object_id);
  void OnMsgHasProperty(SerializedVarReceiveInput var,
                        SerializedVarReceiveInput name,
                        SerializedVarOutParam exception,
                        PP_Bool* result);
  void OnMsgHasMethodDeprecated(SerializedVarReceiveInput var,
                                SerializedVarReceiveInput name,
                                SerializedVarOutParam exception,
                                PP_Bool* result);
  void OnMsgGetProperty(SerializedVarReceiveInput var,
                        SerializedVarReceiveInput name,
                        SerializedVarOutParam exception,
                        SerializedVarReturnValue result);
  void OnMsgEnumerateProperties(
      SerializedVarReceiveInput var,
      SerializedVarVectorOutParam props,
      SerializedVarOutParam exception);
  void OnMsgSetPropertyDeprecated(SerializedVarReceiveInput var,
                                  SerializedVarReceiveInput name,
                                  SerializedVarReceiveInput value,
                                  SerializedVarOutParam exception);
  void OnMsgDeleteProperty(SerializedVarReceiveInput var,
                           SerializedVarReceiveInput name,
                           SerializedVarOutParam exception,
                           PP_Bool* result);
  void OnMsgCall(SerializedVarReceiveInput object,
                 SerializedVarReceiveInput this_object,
                 SerializedVarReceiveInput method_name,
                 SerializedVarVectorReceiveInput arg_vector,
                 SerializedVarOutParam exception,
                 SerializedVarReturnValue result);
  void OnMsgCallDeprecated(SerializedVarReceiveInput object,
                           SerializedVarReceiveInput method_name,
                           SerializedVarVectorReceiveInput arg_vector,
                           SerializedVarOutParam exception,
                           SerializedVarReturnValue result);
  void OnMsgConstruct(SerializedVarReceiveInput var,
                      SerializedVarVectorReceiveInput arg_vector,
                      SerializedVarOutParam exception,
                      SerializedVarReturnValue result);
  void OnMsgIsInstanceOfDeprecated(SerializedVarReceiveInput var,
                                   int64 ppp_class,
                                   int64* ppp_class_data,
                                   PP_Bool* result);
  void OnMsgCreateObjectDeprecated(PP_Instance instance,
                                   int64 ppp_class,
                                   int64 ppp_class_data,
                                   SerializedVarReturnValue result);

  // Call in the host for messages that can be reentered.
  void SetAllowPluginReentrancy();

  void DoReleaseObject(int64 object_id);
  base::WeakPtrFactory<PPB_Var_Deprecated_Proxy> task_factory_;

  const PPB_Var_Deprecated* ppb_var_impl_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Var_Deprecated_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PPB_VAR_DEPRECATED_PROXY_H_
