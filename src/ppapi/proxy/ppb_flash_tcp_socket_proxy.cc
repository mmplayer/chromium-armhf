// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_flash_tcp_socket_proxy.h"

#include <algorithm>
#include <cstring>
#include <map>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_flash_tcp_socket_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::PPB_Flash_TCPSocket_API;

namespace ppapi {
namespace proxy {

const int32_t kFlashTCPSocketMaxReadSize = 1024 * 1024;
const int32_t kFlashTCPSocketMaxWriteSize = 1024 * 1024;

class FlashTCPSocket;

namespace {

typedef std::map<uint32, FlashTCPSocket*> IDToSocketMap;
IDToSocketMap* g_id_to_socket = NULL;

class AbortCallbackTask : public Task {
 public:
  explicit AbortCallbackTask(PP_CompletionCallback callback)
      : callback_(callback) {}
  virtual ~AbortCallbackTask() {}
  virtual void Run() {
    if (callback_.func)
      PP_RunCompletionCallback(&callback_, PP_ERROR_ABORTED);
  }

 private:
  PP_CompletionCallback callback_;
};

InterfaceProxy* CreateFlashTCPSocketProxy(Dispatcher* dispatcher) {
  return new PPB_Flash_TCPSocket_Proxy(dispatcher);
}

}  // namespace

class FlashTCPSocket : public PPB_Flash_TCPSocket_API,
                       public Resource {
 public:
  FlashTCPSocket(const HostResource& resource, uint32 socket_id);
  virtual ~FlashTCPSocket();

  // Resource overrides.
  virtual PPB_Flash_TCPSocket_API* AsPPB_Flash_TCPSocket_API() OVERRIDE;

  // PPB_Flash_TCPSocket_API implementation.
  virtual int32_t Connect(const char* host,
                          uint16_t port,
                          PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t ConnectWithNetAddress(
      const PP_Flash_NetAddress* addr,
      PP_CompletionCallback callback) OVERRIDE;
  virtual PP_Bool GetLocalAddress(PP_Flash_NetAddress* local_addr) OVERRIDE;
  virtual PP_Bool GetRemoteAddress(PP_Flash_NetAddress* remote_addr) OVERRIDE;
  virtual int32_t SSLHandshake(const char* server_name,
                               uint16_t server_port,
                               PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Read(char* buffer,
                       int32_t bytes_to_read,
                       PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Write(const char* buffer,
                        int32_t bytes_to_write,
                        PP_CompletionCallback callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;

  // Notifications from the proxy.
  void OnConnectCompleted(bool succeeded,
                          const PP_Flash_NetAddress& local_addr,
                          const PP_Flash_NetAddress& remote_addr);
  void OnSSLHandshakeCompleted(bool succeeded);
  void OnReadCompleted(bool succeeded, const std::string& data);
  void OnWriteCompleted(bool succeeded, int32_t bytes_written);

 private:
  enum ConnectionState {
    // Before a connection is successfully established (including a connect
    // request is pending or a previous connect request failed).
    BEFORE_CONNECT,
    // A connection has been successfully established (including a request of
    // initiating SSL is pending).
    CONNECTED,
    // An SSL connection has been successfully established.
    SSL_CONNECTED,
    // The connection has been ended.
    DISCONNECTED
  };

  bool IsConnected() const;

  PluginDispatcher* GetDispatcher() const {
    return PluginDispatcher::GetForResource(this);
  }

  // Backend for both Connect() and ConnectWithNetAddress(). To keep things
  // generic, the message is passed in (on error, it's deleted).
  int32_t ConnectWithMessage(IPC::Message* msg,
                             PP_CompletionCallback callback);

  void PostAbortAndClearIfNecessary(PP_CompletionCallback* callback);

  uint32 socket_id_;
  ConnectionState connection_state_;

  PP_CompletionCallback connect_callback_;
  PP_CompletionCallback ssl_handshake_callback_;
  PP_CompletionCallback read_callback_;
  PP_CompletionCallback write_callback_;

  char* read_buffer_;
  int32_t bytes_to_read_;

  PP_Flash_NetAddress local_addr_;
  PP_Flash_NetAddress remote_addr_;

  DISALLOW_COPY_AND_ASSIGN(FlashTCPSocket);
};

FlashTCPSocket::FlashTCPSocket(const HostResource& resource, uint32 socket_id)
    : Resource(resource),
      socket_id_(socket_id),
      connection_state_(BEFORE_CONNECT),
      connect_callback_(PP_BlockUntilComplete()),
      ssl_handshake_callback_(PP_BlockUntilComplete()),
      read_callback_(PP_BlockUntilComplete()),
      write_callback_(PP_BlockUntilComplete()),
      read_buffer_(NULL),
      bytes_to_read_(-1) {
  DCHECK(socket_id != 0);

  local_addr_.size = 0;
  memset(local_addr_.data, 0, sizeof(local_addr_.data));
  remote_addr_.size = 0;
  memset(remote_addr_.data, 0, sizeof(remote_addr_.data));

  if (!g_id_to_socket)
    g_id_to_socket = new IDToSocketMap();
  DCHECK(g_id_to_socket->find(socket_id) == g_id_to_socket->end());
  (*g_id_to_socket)[socket_id] = this;
}

FlashTCPSocket::~FlashTCPSocket() {
  Disconnect();
}

PPB_Flash_TCPSocket_API* FlashTCPSocket::AsPPB_Flash_TCPSocket_API() {
  return this;
}

int32_t FlashTCPSocket::Connect(const char* host,
                                uint16_t port,
                                PP_CompletionCallback callback) {
  if (!host)
    return PP_ERROR_BADARGUMENT;

  return ConnectWithMessage(
      new PpapiHostMsg_PPBFlashTCPSocket_Connect(socket_id_, host, port),
      callback);
}

int32_t FlashTCPSocket::ConnectWithNetAddress(
    const PP_Flash_NetAddress* addr,
    PP_CompletionCallback callback) {
  if (!addr)
    return PP_ERROR_BADARGUMENT;

  return ConnectWithMessage(
      new PpapiHostMsg_PPBFlashTCPSocket_ConnectWithNetAddress(
          socket_id_, *addr),
      callback);
}

PP_Bool FlashTCPSocket::GetLocalAddress(PP_Flash_NetAddress* local_addr) {
  if (!IsConnected() || !local_addr)
    return PP_FALSE;

  *local_addr = local_addr_;
  return PP_TRUE;
}

PP_Bool FlashTCPSocket::GetRemoteAddress(PP_Flash_NetAddress* remote_addr) {
  if (!IsConnected() || !remote_addr)
    return PP_FALSE;

  *remote_addr = remote_addr_;
  return PP_TRUE;
}

int32_t FlashTCPSocket::SSLHandshake(const char* server_name,
                                     uint16_t server_port,
                                     PP_CompletionCallback callback) {
  if (!server_name)
    return PP_ERROR_BADARGUMENT;
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  if (connection_state_ != CONNECTED)
    return PP_ERROR_FAILED;
  if (ssl_handshake_callback_.func || read_callback_.func ||
      write_callback_.func)
    return PP_ERROR_INPROGRESS;

  ssl_handshake_callback_ = callback;

  // Send the request, the browser will call us back via SSLHandshakeACK.
  GetDispatcher()->SendToBrowser(
      new PpapiHostMsg_PPBFlashTCPSocket_SSLHandshake(
          socket_id_, std::string(server_name), server_port));
  return PP_OK_COMPLETIONPENDING;
}

int32_t FlashTCPSocket::Read(char* buffer,
                             int32_t bytes_to_read,
                             PP_CompletionCallback callback) {
  if (!buffer || bytes_to_read <= 0)
    return PP_ERROR_BADARGUMENT;
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  if (!IsConnected())
    return PP_ERROR_FAILED;
  if (read_callback_.func || ssl_handshake_callback_.func)
    return PP_ERROR_INPROGRESS;

  read_buffer_ = buffer;
  bytes_to_read_ = std::min(bytes_to_read, kFlashTCPSocketMaxReadSize);
  read_callback_ = callback;

  // Send the request, the browser will call us back via ReadACK.
  GetDispatcher()->SendToBrowser(
      new PpapiHostMsg_PPBFlashTCPSocket_Read(socket_id_, bytes_to_read_));
  return PP_OK_COMPLETIONPENDING;
}

int32_t FlashTCPSocket::Write(const char* buffer,
                              int32_t bytes_to_write,
                              PP_CompletionCallback callback) {
  if (!buffer || bytes_to_write <= 0)
    return PP_ERROR_BADARGUMENT;
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  if (!IsConnected())
    return PP_ERROR_FAILED;
  if (write_callback_.func || ssl_handshake_callback_.func)
    return PP_ERROR_INPROGRESS;

  if (bytes_to_write > kFlashTCPSocketMaxWriteSize)
    bytes_to_write = kFlashTCPSocketMaxWriteSize;

  write_callback_ = callback;

  // Send the request, the browser will call us back via WriteACK.
  GetDispatcher()->SendToBrowser(
      new PpapiHostMsg_PPBFlashTCPSocket_Write(
          socket_id_, std::string(buffer, bytes_to_write)));
  return PP_OK_COMPLETIONPENDING;
}

void FlashTCPSocket::Disconnect() {
  if (connection_state_ == DISCONNECTED)
    return;

  connection_state_ = DISCONNECTED;
  // After removed from the mapping, this object won't receive any notifications
  // from the proxy.
  DCHECK(g_id_to_socket->find(socket_id_) != g_id_to_socket->end());
  g_id_to_socket->erase(socket_id_);

  GetDispatcher()->SendToBrowser(
      new PpapiHostMsg_PPBFlashTCPSocket_Disconnect(socket_id_));
  socket_id_ = 0;

  PostAbortAndClearIfNecessary(&connect_callback_);
  PostAbortAndClearIfNecessary(&ssl_handshake_callback_);
  PostAbortAndClearIfNecessary(&read_callback_);
  PostAbortAndClearIfNecessary(&write_callback_);
  read_buffer_ = NULL;
  bytes_to_read_ = -1;
}

void FlashTCPSocket::OnConnectCompleted(
    bool succeeded,
    const PP_Flash_NetAddress& local_addr,
    const PP_Flash_NetAddress& remote_addr) {
  if (connection_state_ != BEFORE_CONNECT || !connect_callback_.func) {
    NOTREACHED();
    return;
  }

  if (succeeded) {
    local_addr_ = local_addr;
    remote_addr_ = remote_addr;
    connection_state_ = CONNECTED;
  }
  PP_RunAndClearCompletionCallback(&connect_callback_,
                                   succeeded ? PP_OK : PP_ERROR_FAILED);
}

void FlashTCPSocket::OnSSLHandshakeCompleted(bool succeeded) {
  if (connection_state_ != CONNECTED || !ssl_handshake_callback_.func) {
    NOTREACHED();
    return;
  }

  if (succeeded) {
    connection_state_ = SSL_CONNECTED;
    PP_RunAndClearCompletionCallback(&ssl_handshake_callback_, PP_OK);
  } else {
    PP_RunAndClearCompletionCallback(&ssl_handshake_callback_, PP_ERROR_FAILED);
    Disconnect();
  }
}

void FlashTCPSocket::OnReadCompleted(bool succeeded, const std::string& data) {
  if (!read_callback_.func || !read_buffer_) {
    NOTREACHED();
    return;
  }

  if (succeeded) {
    CHECK_LE(static_cast<int32_t>(data.size()), bytes_to_read_);
    if (!data.empty())
      memcpy(read_buffer_, data.c_str(), data.size());
  }
  read_buffer_ = NULL;
  bytes_to_read_ = -1;

  PP_RunAndClearCompletionCallback(
      &read_callback_,
      succeeded ? static_cast<int32_t>(data.size()) :
                  static_cast<int32_t>(PP_ERROR_FAILED));
}

void FlashTCPSocket::OnWriteCompleted(bool succeeded, int32_t bytes_written) {
  if (!write_callback_.func || (succeeded && bytes_written < 0)) {
    NOTREACHED();
    return;
  }

  PP_RunAndClearCompletionCallback(
      &write_callback_,
      succeeded ? bytes_written : static_cast<int32_t>(PP_ERROR_FAILED));
}

bool FlashTCPSocket::IsConnected() const {
  return connection_state_ == CONNECTED || connection_state_ == SSL_CONNECTED;
}

int32_t FlashTCPSocket::ConnectWithMessage(IPC::Message* msg,
                                           PP_CompletionCallback callback) {
  scoped_ptr<IPC::Message> msg_deletor(msg);
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;
  if (connection_state_ != BEFORE_CONNECT)
    return PP_ERROR_FAILED;
  if (connect_callback_.func)
    return PP_ERROR_INPROGRESS;  // Can only have one pending request.

  connect_callback_ = callback;
  // Send the request, the browser will call us back via ConnectACK.
  GetDispatcher()->SendToBrowser(msg_deletor.release());
  return PP_OK_COMPLETIONPENDING;
}

void FlashTCPSocket::PostAbortAndClearIfNecessary(
    PP_CompletionCallback* callback) {
  DCHECK(callback);

  if (callback->func) {
    MessageLoop::current()->PostTask(FROM_HERE,
                                     new AbortCallbackTask(*callback));
    *callback = PP_BlockUntilComplete();
  }
}

PPB_Flash_TCPSocket_Proxy::PPB_Flash_TCPSocket_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_Flash_TCPSocket_Proxy::~PPB_Flash_TCPSocket_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Flash_TCPSocket_Proxy::GetInfo() {
  static const Info info = {
    thunk::GetPPB_Flash_TCPSocket_Thunk(),
    PPB_FLASH_TCPSOCKET_INTERFACE,
    INTERFACE_ID_PPB_FLASH_TCPSOCKET,
    false,
    &CreateFlashTCPSocketProxy,
  };
  return &info;
}

// static
PP_Resource PPB_Flash_TCPSocket_Proxy::CreateProxyResource(
    PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  uint32 socket_id = 0;
  dispatcher->SendToBrowser(new PpapiHostMsg_PPBFlashTCPSocket_Create(
      INTERFACE_ID_PPB_FLASH_TCPSOCKET, dispatcher->plugin_dispatcher_id(),
      &socket_id));
  if (socket_id == 0)
    return 0;
  return (new FlashTCPSocket(HostResource::MakeInstanceOnly(instance),
                             socket_id))->GetReference();
}

bool PPB_Flash_TCPSocket_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Flash_TCPSocket_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBFlashTCPSocket_ConnectACK, OnMsgConnectACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBFlashTCPSocket_SSLHandshakeACK,
                        OnMsgSSLHandshakeACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBFlashTCPSocket_ReadACK, OnMsgReadACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBFlashTCPSocket_WriteACK, OnMsgWriteACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_Flash_TCPSocket_Proxy::OnMsgConnectACK(
    uint32 /* plugin_dispatcher_id */,
    uint32 socket_id,
    bool succeeded,
    const PP_Flash_NetAddress& local_addr,
    const PP_Flash_NetAddress& remote_addr) {
  if (!g_id_to_socket) {
    NOTREACHED();
    return;
  }
  IDToSocketMap::iterator iter = g_id_to_socket->find(socket_id);
  if (iter == g_id_to_socket->end())
    return;
  iter->second->OnConnectCompleted(succeeded, local_addr, remote_addr);
}

void PPB_Flash_TCPSocket_Proxy::OnMsgSSLHandshakeACK(
    uint32 /* plugin_dispatcher_id */,
    uint32 socket_id,
    bool succeeded) {
  if (!g_id_to_socket) {
    NOTREACHED();
    return;
  }
  IDToSocketMap::iterator iter = g_id_to_socket->find(socket_id);
  if (iter == g_id_to_socket->end())
    return;
  iter->second->OnSSLHandshakeCompleted(succeeded);
}

void PPB_Flash_TCPSocket_Proxy::OnMsgReadACK(uint32 /* plugin_dispatcher_id */,
                                             uint32 socket_id,
                                             bool succeeded,
                                             const std::string& data) {
  if (!g_id_to_socket) {
    NOTREACHED();
    return;
  }
  IDToSocketMap::iterator iter = g_id_to_socket->find(socket_id);
  if (iter == g_id_to_socket->end())
    return;
  iter->second->OnReadCompleted(succeeded, data);
}

void PPB_Flash_TCPSocket_Proxy::OnMsgWriteACK(uint32 /* plugin_dispatcher_id */,
                                              uint32 socket_id,
                                              bool succeeded,
                                              int32_t bytes_written) {
  if (!g_id_to_socket) {
    NOTREACHED();
    return;
  }
  IDToSocketMap::iterator iter = g_id_to_socket->find(socket_id);
  if (iter == g_id_to_socket->end())
    return;
  iter->second->OnWriteCompleted(succeeded, bytes_written);
}

}  // namespace proxy
}  // namespace ppapi
