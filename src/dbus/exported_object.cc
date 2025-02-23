// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/exported_object.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/scoped_dbus_error.h"

namespace dbus {

namespace {

// Used for success ratio histograms. 1 for success, 0 for failure.
const int kSuccessRatioHistogramMaxValue = 2;

// Gets the absolute method name by concatenating the interface name and
// the method name. Used for building keys for method_table_ in
// ExportedObject.
std::string GetAbsoluteMethodName(
    const std::string& interface_name,
    const std::string& method_name) {
  return interface_name + "." + method_name;
}

}  // namespace

ExportedObject::ExportedObject(Bus* bus,
                               const std::string& service_name,
                               const std::string& object_path)
    : bus_(bus),
      service_name_(service_name),
      object_path_(object_path),
      object_is_registered_(false) {
}

ExportedObject::~ExportedObject() {
  DCHECK(!object_is_registered_);
}

bool ExportedObject::ExportMethodAndBlock(
    const std::string& interface_name,
    const std::string& method_name,
    MethodCallCallback method_call_callback) {
  bus_->AssertOnDBusThread();

  // Check if the method is already exported.
  const std::string absolute_method_name =
      GetAbsoluteMethodName(interface_name, method_name);
  if (method_table_.find(absolute_method_name) != method_table_.end()) {
    LOG(ERROR) << absolute_method_name << " is already exported";
    return false;
  }

  if (!bus_->Connect())
    return false;
  if (!bus_->SetUpAsyncOperations())
    return false;
  if (!bus_->RequestOwnership(service_name_))
    return false;
  if (!Register())
    return false;

  // Add the method callback to the method table.
  method_table_[absolute_method_name] = method_call_callback;

  return true;
}

void ExportedObject::ExportMethod(const std::string& interface_name,
                                  const std::string& method_name,
                                  MethodCallCallback method_call_callback,
                                  OnExportedCallback on_exported_calback) {
  bus_->AssertOnOriginThread();

  base::Closure task = base::Bind(&ExportedObject::ExportMethodInternal,
                                  this,
                                  interface_name,
                                  method_name,
                                  method_call_callback,
                                  on_exported_calback);
  bus_->PostTaskToDBusThread(FROM_HERE, task);
}

void ExportedObject::SendSignal(Signal* signal) {
  // For signals, the object path should be set to the path to the sender
  // object, which is this exported object here.
  signal->SetPath(object_path_);

  // Increment the reference count so we can safely reference the
  // underlying signal message until the signal sending is complete. This
  // will be unref'ed in SendSignalInternal().
  DBusMessage* signal_message = signal->raw_message();
  dbus_message_ref(signal_message);

  const base::TimeTicks start_time = base::TimeTicks::Now();
  bus_->PostTaskToDBusThread(FROM_HERE,
                             base::Bind(&ExportedObject::SendSignalInternal,
                                        this,
                                        start_time,
                                        signal_message));
}

void ExportedObject::Unregister() {
  bus_->AssertOnDBusThread();

  if (!object_is_registered_)
    return;

  bus_->UnregisterObjectPath(object_path_);
  object_is_registered_ = false;
}

void ExportedObject::ExportMethodInternal(
    const std::string& interface_name,
    const std::string& method_name,
    MethodCallCallback method_call_callback,
    OnExportedCallback on_exported_calback) {
  bus_->AssertOnDBusThread();

  const bool success = ExportMethodAndBlock(interface_name,
                                            method_name,
                                            method_call_callback);
  bus_->PostTaskToOriginThread(FROM_HERE,
                               base::Bind(&ExportedObject::OnExported,
                                          this,
                                          on_exported_calback,
                                          interface_name,
                                          method_name,
                                          success));
}

void ExportedObject::OnExported(OnExportedCallback on_exported_callback,
                                const std::string& interface_name,
                                const std::string& method_name,
                                bool success) {
  bus_->AssertOnOriginThread();

  on_exported_callback.Run(interface_name, method_name, success);
}

void ExportedObject::SendSignalInternal(base::TimeTicks start_time,
                                        DBusMessage* signal_message) {
  uint32 serial = 0;
  bus_->Send(signal_message, &serial);
  dbus_message_unref(signal_message);
  // Record time spent to send the the signal. This is not accurate as the
  // signal will actually be sent from the next run of the message loop,
  // but we can at least tell the number of signals sent.
  UMA_HISTOGRAM_TIMES("DBus.SignalSendTime",
                      base::TimeTicks::Now() - start_time);
}

bool ExportedObject::Register() {
  bus_->AssertOnDBusThread();

  if (object_is_registered_)
    return true;

  ScopedDBusError error;

  DBusObjectPathVTable vtable = {};
  vtable.message_function = &ExportedObject::HandleMessageThunk;
  vtable.unregister_function = &ExportedObject::OnUnregisteredThunk;
  const bool success = bus_->TryRegisterObjectPath(object_path_,
                                                   &vtable,
                                                   this,
                                                   error.get());
  if (!success) {
    LOG(ERROR) << "Failed to register the object: " << object_path_ << ": "
               << (error.is_set() ? error.message() : "");
    return false;
  }

  object_is_registered_ = true;
  return true;
}

DBusHandlerResult ExportedObject::HandleMessage(
    DBusConnection* connection,
    DBusMessage* raw_message) {
  bus_->AssertOnDBusThread();
  DCHECK_EQ(DBUS_MESSAGE_TYPE_METHOD_CALL, dbus_message_get_type(raw_message));

  // raw_message will be unrefed on exit of the function. Increment the
  // reference so we can use it in MethodCall.
  dbus_message_ref(raw_message);
  scoped_ptr<MethodCall> method_call(
      MethodCall::FromRawMessage(raw_message));
  const std::string interface = method_call->GetInterface();
  const std::string member = method_call->GetMember();

  if (interface.empty()) {
    // We don't support method calls without interface.
    LOG(WARNING) << "Interface is missing: " << method_call->ToString();
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  // Check if we know about the method.
  const std::string absolute_method_name = GetAbsoluteMethodName(
      interface, member);
  MethodTable::const_iterator iter = method_table_.find(absolute_method_name);
  if (iter == method_table_.end()) {
    // Don't know about the method.
    LOG(WARNING) << "Unknown method: " << method_call->ToString();
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  const base::TimeTicks start_time = base::TimeTicks::Now();
  if (bus_->HasDBusThread()) {
    // Post a task to run the method in the origin thread.
    bus_->PostTaskToOriginThread(FROM_HERE,
                                 base::Bind(&ExportedObject::RunMethod,
                                            this,
                                            iter->second,
                                            method_call.release(),
                                            start_time));
  } else {
    // If the D-Bus thread is not used, just call the method directly. We
    // don't need the complicated logic to wait for the method call to be
    // complete.
    // |response| will be deleted in OnMethodCompleted().
    Response* response = iter->second.Run(method_call.get());
    OnMethodCompleted(method_call.release(), response, start_time);
  }

  // It's valid to say HANDLED here, and send a method response at a later
  // time from OnMethodCompleted() asynchronously.
  return DBUS_HANDLER_RESULT_HANDLED;
}

void ExportedObject::RunMethod(MethodCallCallback method_call_callback,
                               MethodCall* method_call,
                               base::TimeTicks start_time) {
  bus_->AssertOnOriginThread();

  Response* response = method_call_callback.Run(method_call);
  bus_->PostTaskToDBusThread(FROM_HERE,
                             base::Bind(&ExportedObject::OnMethodCompleted,
                                        this,
                                        method_call,
                                        response,
                                        start_time));
}

void ExportedObject::OnMethodCompleted(MethodCall* method_call,
                                       Response* response,
                                       base::TimeTicks start_time) {
  bus_->AssertOnDBusThread();
  scoped_ptr<MethodCall> method_call_deleter(method_call);
  scoped_ptr<Response> response_deleter(response);

  // Record if the method call is successful, or not. 1 if successful.
  UMA_HISTOGRAM_ENUMERATION("DBus.ExportedMethodHandleSuccess",
                            response ? 1 : 0,
                            kSuccessRatioHistogramMaxValue);

  // Check if the bus is still connected. If the method takes long to
  // complete, the bus may be shut down meanwhile.
  if (!bus_->is_connected())
    return;

  if (!response) {
    // Something bad happened in the method call.
    scoped_ptr<dbus::ErrorResponse> error_response(
        ErrorResponse::FromMethodCall(
            method_call,
            DBUS_ERROR_FAILED,
            "error occurred in " + method_call->GetMember()));
    bus_->Send(error_response->raw_message(), NULL);
    return;
  }

  // The method call was successful.
  bus_->Send(response->raw_message(), NULL);

  // Record time spent to handle the the method call. Don't include failures.
  UMA_HISTOGRAM_TIMES("DBus.ExportedMethodHandleTime",
                      base::TimeTicks::Now() - start_time);
}

void ExportedObject::OnUnregistered(DBusConnection* connection) {
}

DBusHandlerResult ExportedObject::HandleMessageThunk(
    DBusConnection* connection,
    DBusMessage* raw_message,
    void* user_data) {
  ExportedObject* self = reinterpret_cast<ExportedObject*>(user_data);
  return self->HandleMessage(connection, raw_message);
}

void ExportedObject::OnUnregisteredThunk(DBusConnection *connection,
                                         void* user_data) {
  ExportedObject* self = reinterpret_cast<ExportedObject*>(user_data);
  return self->OnUnregistered(connection);
}

}  // namespace dbus
