// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_MESSAGE_H_
#define DBUS_MESSAGE_H_
#pragma once

#include <string>
#include <vector>
#include <dbus/dbus.h>

#include "base/basictypes.h"

namespace dbus {

class MessageWriter;
class MessageReader;

// Message is the base class of D-Bus message types. Client code must use
// sub classes such as MethodCall and Response instead.
//
// The class name Message is very generic, but there should be no problem
// as the class is inside 'dbus' namespace. We chose to name this way, as
// libdbus defines lots of types starting with DBus, such as
// DBusMessage. We should avoid confusion and conflict with these types.
class Message {
 public:
  // The message type used in D-Bus.  Redefined here so client code
  // doesn't need to use raw D-Bus macros. DBUS_MESSAGE_TYPE_INVALID
  // etc. are #define macros. Having an enum type here makes code a bit
  // more type-safe.
  enum MessageType {
    MESSAGE_INVALID = DBUS_MESSAGE_TYPE_INVALID,
    MESSAGE_METHOD_CALL = DBUS_MESSAGE_TYPE_METHOD_CALL,
    MESSAGE_METHOD_RETURN = DBUS_MESSAGE_TYPE_METHOD_RETURN,
    MESSAGE_SIGNAL = DBUS_MESSAGE_TYPE_SIGNAL,
    MESSAGE_ERROR = DBUS_MESSAGE_TYPE_ERROR,
 };

  // The data type used in the D-Bus type system.  See the comment at
  // MessageType for why we are redefining data types here.
  enum DataType {
    INVALID_DATA = DBUS_TYPE_INVALID,
    BYTE = DBUS_TYPE_BYTE,
    BOOL = DBUS_TYPE_BOOLEAN,
    INT16 = DBUS_TYPE_INT16,
    UINT16 = DBUS_TYPE_UINT16,
    INT32 = DBUS_TYPE_INT32,
    UINT32 = DBUS_TYPE_UINT32,
    INT64 = DBUS_TYPE_INT64,
    UINT64 = DBUS_TYPE_UINT64,
    DOUBLE = DBUS_TYPE_DOUBLE,
    STRING = DBUS_TYPE_STRING,
    OBJECT_PATH = DBUS_TYPE_OBJECT_PATH,
    ARRAY = DBUS_TYPE_ARRAY,
    STRUCT = DBUS_TYPE_STRUCT,
    DICT_ENTRY = DBUS_TYPE_DICT_ENTRY,
    VARIANT = DBUS_TYPE_VARIANT,
  };

  // Returns the type of the message. Returns MESSAGE_INVALID if
  // raw_message_ is NULL.
  MessageType GetMessageType();

  // Returns the type of the message as string like "MESSAGE_METHOD_CALL"
  // for instance.
  std::string GetMessageTypeAsString();

  DBusMessage* raw_message() { return raw_message_; }

  // Sets the destination, the path, the interface, the member, etc.
  void SetDestination(const std::string& destination);
  void SetPath(const std::string& path);
  void SetInterface(const std::string& interface);
  void SetMember(const std::string& member);
  void SetErrorName(const std::string& error_name);
  void SetSender(const std::string& sender);
  void SetSerial(uint32 serial);
  void SetReplySerial(uint32 reply_serial);
  // SetSignature() does not exist as we cannot do it.

  // Gets the destination, the path, the interface, the member, etc.
  // If not set, an empty string is returned.
  std::string GetDestination();
  std::string GetPath();
  std::string GetInterface();
  std::string GetMember();
  std::string GetErrorName();
  std::string GetSender();
  std::string GetSignature();
  // Gets the serial and reply serial numbers. Returns 0 if not set.
  uint32 GetSerial();
  uint32 GetReplySerial();

  // Returns the string representation of this message. Useful for
  // debugging.
  std::string ToString();

 protected:
  // This class cannot be instantiated. Use sub classes instead.
  Message();
  virtual ~Message();

  // Initializes the message with the given raw message.
  void Init(DBusMessage* raw_message);

 private:
  // Helper function used in ToString().
  std::string ToStringInternal(const std::string& indent,
                               MessageReader* reader);

  DBusMessage* raw_message_;
  DISALLOW_COPY_AND_ASSIGN(Message);
};

// MessageCall is a type of message used for calling a method via D-Bus.
class MethodCall : public Message {
 public:
  // Creates a method call message for the specified interface name and
  // the method name.
  //
  // For instance, to call "Get" method of DBUS_INTERFACE_INTROSPECTABLE
  // interface ("org.freedesktop.DBus.Introspectable"), create a method
  // call like this:
  //
  //   MethodCall method_call(DBUS_INTERFACE_INTROSPECTABLE, "Get");
  //
  // The constructor creates the internal raw message.
  MethodCall(const std::string& interface_name,
             const std::string& method_name);

  // Returns a newly created MethodCall from the given raw message of the
  // type DBUS_MESSAGE_TYPE_METHOD_CALL. The caller must delete the
  // returned object. Takes the ownership of |raw_message|.
  static MethodCall* FromRawMessage(DBusMessage* raw_message);

 private:
  // Creates a method call message. The internal raw message is NULL.
  // Only used internally.
  MethodCall();

  DISALLOW_COPY_AND_ASSIGN(MethodCall);
};

// Signal is a type of message used to send a signal.
class Signal : public Message {
 public:
  // Creates a signal message for the specified interface name and the
  // method name.
  //
  // For instance, to send "PropertiesChanged" signal of
  // DBUS_INTERFACE_INTROSPECTABLE interface
  // ("org.freedesktop.DBus.Introspectable"), create a signal like this:
  //
  //   Signal signal(DBUS_INTERFACE_INTROSPECTABLE, "PropertiesChanged");
  //
  // The constructor creates the internal raw_message_.
  Signal(const std::string& interface_name,
         const std::string& method_name);

  // Returns a newly created SIGNAL from the given raw message of the type
  // DBUS_MESSAGE_TYPE_SIGNAL. The caller must delete the returned
  // object. Takes the ownership of |raw_message|.
  static Signal* FromRawMessage(DBusMessage* raw_message);

 private:
  // Creates a signal message. The internal raw message is NULL.
  // Only used internally.
  Signal();

  DISALLOW_COPY_AND_ASSIGN(Signal);
};

// Response is a type of message used for receiving a response from a
// method via D-Bus.
class Response : public Message {
 public:
  // Returns a newly created Response from the given raw message of the
  // type DBUS_MESSAGE_TYPE_METHOD_RETURN. The caller must delete the
  // returned object. Takes the ownership of |raw_message|.
  static Response* FromRawMessage(DBusMessage* raw_message);

  // Returns a newly created Response from the given method call. The
  // caller must delete the returned object. Used for implementing
  // exported methods.
  static Response* FromMethodCall(MethodCall* method_call);

  // Returns a newly created Response with an empty payload. The caller
  // must delete the returned object. Useful for testing.
  static Response* CreateEmpty();

 private:
  // Creates a Response message. The internal raw message is NULL.
  Response();

  DISALLOW_COPY_AND_ASSIGN(Response);
};

// ErrorResponse is a type of message used to return an error to the
// caller of a method.
class ErrorResponse: public Message {
 public:
  // Returns a newly created Response from the given raw message of the
  // type DBUS_MESSAGE_TYPE_METHOD_RETURN. The caller must delete the
  // returned object. Takes the ownership of |raw_message|.
  static ErrorResponse* FromRawMessage(DBusMessage* raw_message);

  // Returns a newly created ErrorResponse from the given method call, the
  // error name, and the error message.  The error name looks like
  // "org.freedesktop.DBus.Error.Failed". Used for returning an error to a
  // failed method call.
  static ErrorResponse* FromMethodCall(MethodCall* method_call,
                                       const std::string& error_name,
                                       const std::string& error_message);

 private:
  // Creates an ErrorResponse message. The internal raw message is NULL.
  ErrorResponse();

  DISALLOW_COPY_AND_ASSIGN(ErrorResponse);
};

// MessageWriter is used to write outgoing messages for calling methods
// and sending signals.
//
// The main design goal of MessageReader and MessageWriter classes is to
// provide a type safe API. In the past, there was a Chrome OS blocker
// bug, that took days to fix, that would have been prevented if the API
// was type-safe.
//
// For instance, instead of doing something like:
//
//   // We shouldn't add '&' to str here, but it compiles with '&' added.
//   dbus_g_proxy_call(..., G_TYPE_STRING, str, G_TYPE_INVALID, ...)
//
// We want to do something like:
//
//   writer.AppendString(str);
//
class MessageWriter {
 public:
  // Data added with Append* will be written to |message|.
  MessageWriter(Message* message);
  ~MessageWriter();

  // Appends a byte to the message.
  void AppendByte(uint8 value);
  void AppendBool(bool value);
  void AppendInt16(int16 value);
  void AppendUint16(uint16 value);
  void AppendInt32(int32 value);
  void AppendUint32(uint32 value);
  void AppendInt64(int64 value);
  void AppendUint64(uint64 value);
  void AppendDouble(double value);
  void AppendString(const std::string& value);
  void AppendObjectPath(const std::string& value);

  // Opens an array. The array contents can be added to the array with
  // |sub_writer|. The client code must close the array with
  // CloseContainer(), once all contents are added.
  //
  // |signature| parameter is used to supply the D-Bus type signature of
  // the array contents. For instance, if you want an array of strings,
  // then you pass "s" as the signature.
  //
  // See the spec for details about the type signatures.
  // http://dbus.freedesktop.org/doc/dbus-specification.html
  // #message-protocol-signatures
  //
  void OpenArray(const std::string& signature, MessageWriter* sub_writer);
  // Do the same for a variant.
  void OpenVariant(const std::string& signature, MessageWriter* sub_writer);
  // Do the same for Struct and dict entry. They don't need the signature.
  void OpenStruct(MessageWriter* sub_writer);
  void OpenDictEntry(MessageWriter* sub_writer);

  // Close the container for a array/variant/struct/dict entry.
  void CloseContainer(MessageWriter* sub_writer);

  // Appends the array of bytes. Arrays of bytes are often used for
  // exchanging binary blobs hence it's worth having a specialized
  // function.
  void AppendArrayOfBytes(const uint8* values, size_t length);

  // Appends the array of strings. Arrays of strings are often used for
  // exchanging lists of names hence it's worth having a specialized
  // function.
  void AppendArrayOfStrings(const std::vector<std::string>& strings);

  // Appends the array of object paths. Arrays of object paths are often
  // used when exchanging object paths, hence it's worth having a
  // specialized function.
  void AppendArrayOfObjectPaths(const std::vector<std::string>& object_paths);

  // Appends the byte wrapped in a variant data container. Variants are
  // widely used in D-Bus services so it's worth having a specialized
  // function. For instance, The third parameter of
  // "org.freedesktop.DBus.Properties.Set" is a variant.
  void AppendVariantOfByte(uint8 value);
  void AppendVariantOfBool(bool value);
  void AppendVariantOfInt16(int16 value);
  void AppendVariantOfUint16(uint16 value);
  void AppendVariantOfInt32(int32 value);
  void AppendVariantOfUint32(uint32 value);
  void AppendVariantOfInt64(int64 value);
  void AppendVariantOfUint64(uint64 value);
  void AppendVariantOfDouble(double value);
  void AppendVariantOfString(const std::string& value);
  void AppendVariantOfObjectPath(const std::string& value);

 private:
  // Helper function used to implement AppendByte etc.
  void AppendBasic(int dbus_type, const void* value);

  // Helper function used to implement AppendVariantOfByte() etc.
  void AppendVariantOfBasic(int dbus_type, const void* value);

  Message* message_;
  DBusMessageIter raw_message_iter_;
  bool container_is_open_;

  DISALLOW_COPY_AND_ASSIGN(MessageWriter);
};

// MessageReader is used to read incoming messages such as responses for
// method calls.
//
// MessageReader manages an internal iterator to read data. All functions
// starting with Pop advance the iterator on success.
class MessageReader {
 public:
  // The data will be read from the given message.
  MessageReader(Message* message);
  ~MessageReader();

  // Returns true if the reader has more data to read. The function is
  // used to iterate contents in a container like:
  //
  //   while (reader.HasMoreData())
  //     reader.PopString(&value);
  bool HasMoreData();

  // Gets the byte at the current iterator position.
  // Returns true and advances the iterator on success.
  // Returns false if the data type is not a byte.
  bool PopByte(uint8* value);
  bool PopBool(bool* value);
  bool PopInt16(int16* value);
  bool PopUint16(uint16* value);
  bool PopInt32(int32* value);
  bool PopUint32(uint32* value);
  bool PopInt64(int64* value);
  bool PopUint64(uint64* value);
  bool PopDouble(double* value);
  bool PopString(std::string* value);
  bool PopObjectPath(std::string* value);

  // Sets up the given message reader to read an array at the current
  // iterator position.
  // Returns true and advances the iterator on success.
  // Returns false if the data type is not an array
  bool PopArray(MessageReader* sub_reader);
  bool PopStruct(MessageReader* sub_reader);
  bool PopDictEntry(MessageReader* sub_reader);
  bool PopVariant(MessageReader* sub_reader);

  // Gets the array of bytes at the current iterator position.
  // Returns true and advances the iterator on success.
  //
  // Arrays of bytes are often used for exchanging binary blobs hence it's
  // worth having a specialized function.
  //
  // |bytes| must be copied if the contents will be referenced after the
  // MessageReader is destroyed.
  bool PopArrayOfBytes(uint8** bytes, size_t* length);

  // Gets the array of strings at the current iterator position.
  // Returns true and advances the iterator on success.
  //
  // Arrays of strings are often used to communicate with D-Bus
  // services like KWallet, hence it's worth having a specialized
  // function.
  bool PopArrayOfStrings(std::vector<std::string>* strings);

  // Gets the array of object paths at the current iterator position.
  // Returns true and advances the iterator on success.
  //
  // Arrays of object paths are often used to communicate with D-Bus
  // services like NetworkManager, hence it's worth having a specialized
  // function.
  bool PopArrayOfObjectPaths(std::vector<std::string>* object_paths);

  // Gets the byte from the variant data container at the current iterator
  // position.
  // Returns true and advances the iterator on success.
  //
  // Variants are widely used in D-Bus services so it's worth having a
  // specialized function. For instance, The return value type of
  // "org.freedesktop.DBus.Properties.Get" is a variant.
  bool PopVariantOfByte(uint8* value);
  bool PopVariantOfBool(bool* value);
  bool PopVariantOfInt16(int16* value);
  bool PopVariantOfUint16(uint16* value);
  bool PopVariantOfInt32(int32* value);
  bool PopVariantOfUint32(uint32* value);
  bool PopVariantOfInt64(int64* value);
  bool PopVariantOfUint64(uint64* value);
  bool PopVariantOfDouble(double* value);
  bool PopVariantOfString(std::string* value);
  bool PopVariantOfObjectPath(std::string* value);

  // Get the data type of the value at the current iterator
  // position. INVALID_DATA will be returned if the iterator points to the
  // end of the message.
  Message::DataType GetDataType();

 private:
  // Returns true if the data type at the current iterator position
  // matches the given D-Bus type, such as DBUS_TYPE_BYTE.
  bool CheckDataType(int dbus_type);

  // Helper function used to implement PopByte() etc.
  bool PopBasic(int dbus_type, void *value);

  // Helper function used to implement PopArray() etc.
  bool PopContainer(int dbus_type, MessageReader* sub_reader);

  // Helper function used to implement PopVariantOfByte() etc.
  bool PopVariantOfBasic(int dbus_type, void* value);

  Message* message_;
  DBusMessageIter raw_message_iter_;

  DISALLOW_COPY_AND_ASSIGN(MessageReader);
};

}  // namespace dbus

#endif  // DBUS_MESSAGE_H_
