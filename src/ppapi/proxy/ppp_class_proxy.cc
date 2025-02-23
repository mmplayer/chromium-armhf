// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_class_proxy.h"

#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppp_class_deprecated.h"
#include "ppapi/proxy/dispatcher.h"
#include "ppapi/proxy/interface_id.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"

namespace ppapi {
namespace proxy {

namespace {

// PPP_Class in the browser implementation -------------------------------------

// Represents a plugin-implemented class in the browser process. This just
// stores the data necessary to call back the plugin.
struct ObjectProxy {
  ObjectProxy(Dispatcher* d, int64 p, int64 ud)
      : dispatcher(d),
        ppp_class(p),
        user_data(ud) {
  }

  Dispatcher* dispatcher;
  int64 ppp_class;
  int64 user_data;
};

ObjectProxy* ToObjectProxy(void* data) {
  return reinterpret_cast<ObjectProxy*>(data);
}

bool HasProperty(void* object, PP_Var name, PP_Var* exception) {
  ObjectProxy* obj = ToObjectProxy(object);
  bool result = false;
  ReceiveSerializedException se(obj->dispatcher, exception);
  obj->dispatcher->Send(new PpapiMsg_PPPClass_HasProperty(
      INTERFACE_ID_PPP_CLASS, obj->ppp_class, obj->user_data,
      SerializedVarSendInput(obj->dispatcher, name), &se, &result));
  return result;
}

bool HasMethod(void* object, PP_Var name, PP_Var* exception) {
  ObjectProxy* obj = ToObjectProxy(object);
  bool result = false;
  ReceiveSerializedException se(obj->dispatcher, exception);
  obj->dispatcher->Send(new PpapiMsg_PPPClass_HasMethod(
      INTERFACE_ID_PPP_CLASS, obj->ppp_class, obj->user_data,
      SerializedVarSendInput(obj->dispatcher, name), &se, &result));
  return result;
}

PP_Var GetProperty(void* object,
                   PP_Var name,
                   PP_Var* exception) {
  ObjectProxy* obj = ToObjectProxy(object);
  ReceiveSerializedException se(obj->dispatcher, exception);
  ReceiveSerializedVarReturnValue result;
  obj->dispatcher->Send(new PpapiMsg_PPPClass_GetProperty(
      INTERFACE_ID_PPP_CLASS, obj->ppp_class, obj->user_data,
      SerializedVarSendInput(obj->dispatcher, name), &se, &result));
  return result.Return(obj->dispatcher);
}

void GetAllPropertyNames(void* object,
                         uint32_t* property_count,
                         PP_Var** properties,
                         PP_Var* exception) {
  NOTIMPLEMENTED();
  // TODO(brettw) implement this.
}

void SetProperty(void* object,
                 PP_Var name,
                 PP_Var value,
                 PP_Var* exception) {
  ObjectProxy* obj = ToObjectProxy(object);
  ReceiveSerializedException se(obj->dispatcher, exception);
  obj->dispatcher->Send(new PpapiMsg_PPPClass_SetProperty(
      INTERFACE_ID_PPP_CLASS, obj->ppp_class, obj->user_data,
      SerializedVarSendInput(obj->dispatcher, name),
      SerializedVarSendInput(obj->dispatcher, value), &se));
}

void RemoveProperty(void* object,
                    PP_Var name,
                    PP_Var* exception) {
  ObjectProxy* obj = ToObjectProxy(object);
  ReceiveSerializedException se(obj->dispatcher, exception);
  obj->dispatcher->Send(new PpapiMsg_PPPClass_RemoveProperty(
      INTERFACE_ID_PPP_CLASS, obj->ppp_class, obj->user_data,
      SerializedVarSendInput(obj->dispatcher, name), &se));
}

PP_Var Call(void* object,
            PP_Var method_name,
            uint32_t argc,
            PP_Var* argv,
            PP_Var* exception) {
  ObjectProxy* obj = ToObjectProxy(object);

  ReceiveSerializedVarReturnValue result;
  ReceiveSerializedException se(obj->dispatcher, exception);
  std::vector<SerializedVar> argv_vect;
  SerializedVarSendInput::ConvertVector(obj->dispatcher, argv, argc,
                                        &argv_vect);

  obj->dispatcher->Send(new PpapiMsg_PPPClass_Call(
      INTERFACE_ID_PPP_CLASS, obj->ppp_class, obj->user_data,
      SerializedVarSendInput(obj->dispatcher, method_name), argv_vect,
      &se, &result));
  return result.Return(obj->dispatcher);
}

PP_Var Construct(void* object,
                 uint32_t argc,
                 PP_Var* argv,
                 PP_Var* exception) {
  ObjectProxy* obj = ToObjectProxy(object);

  ReceiveSerializedVarReturnValue result;
  ReceiveSerializedException se(obj->dispatcher, exception);
  std::vector<SerializedVar> argv_vect;
  SerializedVarSendInput::ConvertVector(obj->dispatcher, argv, argc,
                                        &argv_vect);

  obj->dispatcher->Send(new PpapiMsg_PPPClass_Construct(
      INTERFACE_ID_PPP_CLASS,
      obj->ppp_class, obj->user_data, argv_vect, &se, &result));
  return result.Return(obj->dispatcher);
}

void Deallocate(void* object) {
  ObjectProxy* obj = ToObjectProxy(object);
  obj->dispatcher->Send(new PpapiMsg_PPPClass_Deallocate(
      INTERFACE_ID_PPP_CLASS, obj->ppp_class, obj->user_data));
  delete obj;
}

const PPP_Class_Deprecated class_interface = {
  &HasProperty,
  &HasMethod,
  &GetProperty,
  &GetAllPropertyNames,
  &SetProperty,
  &RemoveProperty,
  &Call,
  &Construct,
  &Deallocate
};

// Plugin helper functions -----------------------------------------------------

// Converts an int64 object from IPC to a PPP_Class* for calling into the
// plugin's implementation.
const PPP_Class_Deprecated* ToPPPClass(int64 value) {
  return reinterpret_cast<const PPP_Class_Deprecated*>(
      static_cast<intptr_t>(value));
}

// Converts an int64 object from IPC to a void* for calling into the plugin's
// implementation as the user data.
void* ToUserData(int64 value) {
  return reinterpret_cast<void*>(static_cast<intptr_t>(value));
}

}  // namespace

// PPP_Class_Proxy -------------------------------------------------------------

PPP_Class_Proxy::PPP_Class_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPP_Class_Proxy::~PPP_Class_Proxy() {
}

// static
InterfaceProxy* PPP_Class_Proxy::Create(Dispatcher* dispatcher) {
  return new PPP_Class_Proxy(dispatcher);
}

// static
PP_Var PPP_Class_Proxy::CreateProxiedObject(const PPB_Var_Deprecated* var,
                                            Dispatcher* dispatcher,
                                            PP_Module module_id,
                                            int64 ppp_class,
                                            int64 class_data) {
  ObjectProxy* object_proxy = new ObjectProxy(dispatcher,
                                              ppp_class, class_data);
  return var->CreateObject(module_id, &class_interface, object_proxy);
}

bool PPP_Class_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_Class_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPClass_HasProperty,
                        OnMsgHasProperty)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPClass_HasMethod,
                        OnMsgHasMethod)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPClass_GetProperty,
                        OnMsgGetProperty)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPClass_EnumerateProperties,
                        OnMsgEnumerateProperties)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPClass_SetProperty,
                        OnMsgSetProperty)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPClass_Call,
                        OnMsgCall)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPClass_Construct,
                        OnMsgConstruct)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPClass_Deallocate,
                        OnMsgDeallocate)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPP_Class_Proxy::OnMsgHasProperty(int64 ppp_class, int64 object,
                                       SerializedVarReceiveInput property,
                                       SerializedVarOutParam exception,
                                       bool* result) {
  *result = ToPPPClass(ppp_class)->HasProperty(ToUserData(object),
      property.Get(dispatcher()), exception.OutParam(dispatcher()));
}

void PPP_Class_Proxy::OnMsgHasMethod(int64 ppp_class, int64 object,
                                     SerializedVarReceiveInput property,
                                     SerializedVarOutParam exception,
                                     bool* result) {
  *result = ToPPPClass(ppp_class)->HasMethod(ToUserData(object),
      property.Get(dispatcher()), exception.OutParam(dispatcher()));
}

void PPP_Class_Proxy::OnMsgGetProperty(int64 ppp_class, int64 object,
                                       SerializedVarReceiveInput property,
                                       SerializedVarOutParam exception,
                                       SerializedVarReturnValue result) {
  result.Return(dispatcher(), ToPPPClass(ppp_class)->GetProperty(
      ToUserData(object), property.Get(dispatcher()),
      exception.OutParam(dispatcher())));
}

void PPP_Class_Proxy::OnMsgEnumerateProperties(
    int64 ppp_class, int64 object,
    std::vector<SerializedVar>* props,
    SerializedVarOutParam exception) {
  NOTIMPLEMENTED();
  // TODO(brettw) implement this.
}

void PPP_Class_Proxy::OnMsgSetProperty(int64 ppp_class, int64 object,
                                       SerializedVarReceiveInput property,
                                       SerializedVarReceiveInput value,
                                       SerializedVarOutParam exception) {
  ToPPPClass(ppp_class)->SetProperty(
      ToUserData(object), property.Get(dispatcher()), value.Get(dispatcher()),
      exception.OutParam(dispatcher()));
}

void PPP_Class_Proxy::OnMsgRemoveProperty(int64 ppp_class, int64 object,
                                          SerializedVarReceiveInput property,
                                          SerializedVarOutParam exception) {
  ToPPPClass(ppp_class)->RemoveProperty(
      ToUserData(object), property.Get(dispatcher()),
      exception.OutParam(dispatcher()));
}

void PPP_Class_Proxy::OnMsgCall(
    int64 ppp_class, int64 object,
    SerializedVarReceiveInput method_name,
    SerializedVarVectorReceiveInput arg_vector,
    SerializedVarOutParam exception,
    SerializedVarReturnValue result) {
  uint32_t arg_count = 0;
  PP_Var* args = arg_vector.Get(dispatcher(), &arg_count);
  result.Return(dispatcher(), ToPPPClass(ppp_class)->Call(
      ToUserData(object), method_name.Get(dispatcher()),
      arg_count, args, exception.OutParam(dispatcher())));
}

void PPP_Class_Proxy::OnMsgConstruct(
    int64 ppp_class, int64 object,
    SerializedVarVectorReceiveInput arg_vector,
    SerializedVarOutParam exception,
    SerializedVarReturnValue result) {
  uint32_t arg_count = 0;
  PP_Var* args = arg_vector.Get(dispatcher(), &arg_count);
  result.Return(dispatcher(), ToPPPClass(ppp_class)->Construct(
      ToUserData(object), arg_count, args, exception.OutParam(dispatcher())));
}

void PPP_Class_Proxy::OnMsgDeallocate(int64 ppp_class, int64 object) {
  ToPPPClass(ppp_class)->Deallocate(ToUserData(object));
}

}  // namespace proxy
}  // namespace ppapi
