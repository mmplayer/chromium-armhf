// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socks5_client_socket.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/format_macros.h"
#include "base/string_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/base/sys_addrinfo.h"
#include "net/socket/client_socket_handle.h"

namespace net {

const unsigned int SOCKS5ClientSocket::kGreetReadHeaderSize = 2;
const unsigned int SOCKS5ClientSocket::kWriteHeaderSize = 10;
const unsigned int SOCKS5ClientSocket::kReadHeaderSize = 5;
const uint8 SOCKS5ClientSocket::kSOCKS5Version = 0x05;
const uint8 SOCKS5ClientSocket::kTunnelCommand = 0x01;
const uint8 SOCKS5ClientSocket::kNullByte = 0x00;

COMPILE_ASSERT(sizeof(struct in_addr) == 4, incorrect_system_size_of_IPv4);
COMPILE_ASSERT(sizeof(struct in6_addr) == 16, incorrect_system_size_of_IPv6);

SOCKS5ClientSocket::SOCKS5ClientSocket(
    ClientSocketHandle* transport_socket,
    const HostResolver::RequestInfo& req_info)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          io_callback_(this, &SOCKS5ClientSocket::OnIOComplete)),
      transport_(transport_socket),
      next_state_(STATE_NONE),
      user_callback_(NULL),
      completed_handshake_(false),
      bytes_sent_(0),
      bytes_received_(0),
      read_header_size(kReadHeaderSize),
      host_request_info_(req_info),
      net_log_(transport_socket->socket()->NetLog()) {
}

SOCKS5ClientSocket::SOCKS5ClientSocket(
    StreamSocket* transport_socket,
    const HostResolver::RequestInfo& req_info)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          io_callback_(this, &SOCKS5ClientSocket::OnIOComplete)),
      transport_(new ClientSocketHandle()),
      next_state_(STATE_NONE),
      user_callback_(NULL),
      completed_handshake_(false),
      bytes_sent_(0),
      bytes_received_(0),
      read_header_size(kReadHeaderSize),
      host_request_info_(req_info),
      net_log_(transport_socket->NetLog()) {
  transport_->set_socket(transport_socket);
}

SOCKS5ClientSocket::~SOCKS5ClientSocket() {
  Disconnect();
}

int SOCKS5ClientSocket::Connect(OldCompletionCallback* callback) {
  DCHECK(transport_.get());
  DCHECK(transport_->socket());
  DCHECK_EQ(STATE_NONE, next_state_);
  DCHECK(!user_callback_);

  // If already connected, then just return OK.
  if (completed_handshake_)
    return OK;

  net_log_.BeginEvent(NetLog::TYPE_SOCKS5_CONNECT, NULL);

  next_state_ = STATE_GREET_WRITE;
  buffer_.clear();

  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING) {
    user_callback_ = callback;
  } else {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SOCKS5_CONNECT, rv);
  }
  return rv;
}

void SOCKS5ClientSocket::Disconnect() {
  completed_handshake_ = false;
  transport_->socket()->Disconnect();

  // Reset other states to make sure they aren't mistakenly used later.
  // These are the states initialized by Connect().
  next_state_ = STATE_NONE;
  user_callback_ = NULL;
}

bool SOCKS5ClientSocket::IsConnected() const {
  return completed_handshake_ && transport_->socket()->IsConnected();
}

bool SOCKS5ClientSocket::IsConnectedAndIdle() const {
  return completed_handshake_ && transport_->socket()->IsConnectedAndIdle();
}

const BoundNetLog& SOCKS5ClientSocket::NetLog() const {
  return net_log_;
}

void SOCKS5ClientSocket::SetSubresourceSpeculation() {
  if (transport_.get() && transport_->socket()) {
    transport_->socket()->SetSubresourceSpeculation();
  } else {
    NOTREACHED();
  }
}

void SOCKS5ClientSocket::SetOmniboxSpeculation() {
  if (transport_.get() && transport_->socket()) {
    transport_->socket()->SetOmniboxSpeculation();
  } else {
    NOTREACHED();
  }
}

bool SOCKS5ClientSocket::WasEverUsed() const {
  if (transport_.get() && transport_->socket()) {
    return transport_->socket()->WasEverUsed();
  }
  NOTREACHED();
  return false;
}

bool SOCKS5ClientSocket::UsingTCPFastOpen() const {
  if (transport_.get() && transport_->socket()) {
    return transport_->socket()->UsingTCPFastOpen();
  }
  NOTREACHED();
  return false;
}

int64 SOCKS5ClientSocket::NumBytesRead() const {
  if (transport_.get() && transport_->socket()) {
    return transport_->socket()->NumBytesRead();
  }
  NOTREACHED();
  return -1;
}

base::TimeDelta SOCKS5ClientSocket::GetConnectTimeMicros() const {
  if (transport_.get() && transport_->socket()) {
    return transport_->socket()->GetConnectTimeMicros();
  }
  NOTREACHED();
  return base::TimeDelta::FromMicroseconds(-1);
}

// Read is called by the transport layer above to read. This can only be done
// if the SOCKS handshake is complete.
int SOCKS5ClientSocket::Read(IOBuffer* buf, int buf_len,
                             OldCompletionCallback* callback) {
  DCHECK(completed_handshake_);
  DCHECK_EQ(STATE_NONE, next_state_);
  DCHECK(!user_callback_);

  return transport_->socket()->Read(buf, buf_len, callback);
}

// Write is called by the transport layer. This can only be done if the
// SOCKS handshake is complete.
int SOCKS5ClientSocket::Write(IOBuffer* buf, int buf_len,
                             OldCompletionCallback* callback) {
  DCHECK(completed_handshake_);
  DCHECK_EQ(STATE_NONE, next_state_);
  DCHECK(!user_callback_);

  return transport_->socket()->Write(buf, buf_len, callback);
}

bool SOCKS5ClientSocket::SetReceiveBufferSize(int32 size) {
  return transport_->socket()->SetReceiveBufferSize(size);
}

bool SOCKS5ClientSocket::SetSendBufferSize(int32 size) {
  return transport_->socket()->SetSendBufferSize(size);
}

void SOCKS5ClientSocket::DoCallback(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  DCHECK(user_callback_);

  // Since Run() may result in Read being called,
  // clear user_callback_ up front.
  OldCompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  c->Run(result);
}

void SOCKS5ClientSocket::OnIOComplete(int result) {
  DCHECK_NE(STATE_NONE, next_state_);
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING) {
    net_log_.EndEvent(NetLog::TYPE_SOCKS5_CONNECT, NULL);
    DoCallback(rv);
  }
}

int SOCKS5ClientSocket::DoLoop(int last_io_result) {
  DCHECK_NE(next_state_, STATE_NONE);
  int rv = last_io_result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_GREET_WRITE:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLog::TYPE_SOCKS5_GREET_WRITE, NULL);
        rv = DoGreetWrite();
        break;
      case STATE_GREET_WRITE_COMPLETE:
        rv = DoGreetWriteComplete(rv);
        net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SOCKS5_GREET_WRITE, rv);
        break;
      case STATE_GREET_READ:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLog::TYPE_SOCKS5_GREET_READ, NULL);
        rv = DoGreetRead();
        break;
      case STATE_GREET_READ_COMPLETE:
        rv = DoGreetReadComplete(rv);
        net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SOCKS5_GREET_READ, rv);
        break;
      case STATE_HANDSHAKE_WRITE:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLog::TYPE_SOCKS5_HANDSHAKE_WRITE, NULL);
        rv = DoHandshakeWrite();
        break;
      case STATE_HANDSHAKE_WRITE_COMPLETE:
        rv = DoHandshakeWriteComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLog::TYPE_SOCKS5_HANDSHAKE_WRITE, rv);
        break;
      case STATE_HANDSHAKE_READ:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLog::TYPE_SOCKS5_HANDSHAKE_READ, NULL);
        rv = DoHandshakeRead();
        break;
      case STATE_HANDSHAKE_READ_COMPLETE:
        rv = DoHandshakeReadComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLog::TYPE_SOCKS5_HANDSHAKE_READ, rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
  return rv;
}

const char kSOCKS5GreetWriteData[] = { 0x05, 0x01, 0x00 };  // no authentication
const char kSOCKS5GreetReadData[] = { 0x05, 0x00 };

int SOCKS5ClientSocket::DoGreetWrite() {
  // Since we only have 1 byte to send the hostname length in, if the
  // URL has a hostname longer than 255 characters we can't send it.
  if (0xFF < host_request_info_.hostname().size()) {
    net_log_.AddEvent(NetLog::TYPE_SOCKS_HOSTNAME_TOO_BIG, NULL);
    return ERR_SOCKS_CONNECTION_FAILED;
  }

  if (buffer_.empty()) {
    buffer_ = std::string(kSOCKS5GreetWriteData,
                          arraysize(kSOCKS5GreetWriteData));
    bytes_sent_ = 0;
  }

  next_state_ = STATE_GREET_WRITE_COMPLETE;
  size_t handshake_buf_len = buffer_.size() - bytes_sent_;
  handshake_buf_ = new IOBuffer(handshake_buf_len);
  memcpy(handshake_buf_->data(), &buffer_.data()[bytes_sent_],
         handshake_buf_len);
  return transport_->socket()->Write(handshake_buf_, handshake_buf_len,
                                     &io_callback_);
}

int SOCKS5ClientSocket::DoGreetWriteComplete(int result) {
  if (result < 0)
    return result;

  bytes_sent_ += result;
  if (bytes_sent_ == buffer_.size()) {
    buffer_.clear();
    bytes_received_ = 0;
    next_state_ = STATE_GREET_READ;
  } else {
    next_state_ = STATE_GREET_WRITE;
  }
  return OK;
}

int SOCKS5ClientSocket::DoGreetRead() {
  next_state_ = STATE_GREET_READ_COMPLETE;
  size_t handshake_buf_len = kGreetReadHeaderSize - bytes_received_;
  handshake_buf_ = new IOBuffer(handshake_buf_len);
  return transport_->socket()->Read(handshake_buf_, handshake_buf_len,
                                    &io_callback_);
}

int SOCKS5ClientSocket::DoGreetReadComplete(int result) {
  if (result < 0)
    return result;

  if (result == 0) {
    net_log_.AddEvent(NetLog::TYPE_SOCKS_UNEXPECTEDLY_CLOSED_DURING_GREETING,
                      NULL);
    return ERR_SOCKS_CONNECTION_FAILED;
  }

  bytes_received_ += result;
  buffer_.append(handshake_buf_->data(), result);
  if (bytes_received_ < kGreetReadHeaderSize) {
    next_state_ = STATE_GREET_READ;
    return OK;
  }

  // Got the greet data.
  if (buffer_[0] != kSOCKS5Version) {
    net_log_.AddEvent(
        NetLog::TYPE_SOCKS_UNEXPECTED_VERSION,
        make_scoped_refptr(new NetLogIntegerParameter("version", buffer_[0])));
    return ERR_SOCKS_CONNECTION_FAILED;
  }
  if (buffer_[1] != 0x00) {
    net_log_.AddEvent(
        NetLog::TYPE_SOCKS_UNEXPECTED_AUTH,
        make_scoped_refptr(new NetLogIntegerParameter("method", buffer_[1])));
    return ERR_SOCKS_CONNECTION_FAILED;
  }

  buffer_.clear();
  next_state_ = STATE_HANDSHAKE_WRITE;
  return OK;
}

int SOCKS5ClientSocket::BuildHandshakeWriteBuffer(std::string* handshake)
    const {
  DCHECK(handshake->empty());

  handshake->push_back(kSOCKS5Version);
  handshake->push_back(kTunnelCommand);  // Connect command
  handshake->push_back(kNullByte);  // Reserved null

  handshake->push_back(kEndPointDomain);  // The type of the address.

  DCHECK_GE(static_cast<size_t>(0xFF), host_request_info_.hostname().size());

  // First add the size of the hostname, followed by the hostname.
  handshake->push_back(static_cast<unsigned char>(
      host_request_info_.hostname().size()));
  handshake->append(host_request_info_.hostname());

  uint16 nw_port = htons(host_request_info_.port());
  handshake->append(reinterpret_cast<char*>(&nw_port), sizeof(nw_port));
  return OK;
}

// Writes the SOCKS handshake data to the underlying socket connection.
int SOCKS5ClientSocket::DoHandshakeWrite() {
  next_state_ = STATE_HANDSHAKE_WRITE_COMPLETE;

  if (buffer_.empty()) {
    int rv = BuildHandshakeWriteBuffer(&buffer_);
    if (rv != OK)
      return rv;
    bytes_sent_ = 0;
  }

  int handshake_buf_len = buffer_.size() - bytes_sent_;
  DCHECK_LT(0, handshake_buf_len);
  handshake_buf_ = new IOBuffer(handshake_buf_len);
  memcpy(handshake_buf_->data(), &buffer_[bytes_sent_],
         handshake_buf_len);
  return transport_->socket()->Write(handshake_buf_, handshake_buf_len,
                                     &io_callback_);
}

int SOCKS5ClientSocket::DoHandshakeWriteComplete(int result) {
  if (result < 0)
    return result;

  // We ignore the case when result is 0, since the underlying Write
  // may return spurious writes while waiting on the socket.

  bytes_sent_ += result;
  if (bytes_sent_ == buffer_.size()) {
    next_state_ = STATE_HANDSHAKE_READ;
    buffer_.clear();
  } else if (bytes_sent_ < buffer_.size()) {
    next_state_ = STATE_HANDSHAKE_WRITE;
  } else {
    NOTREACHED();
  }

  return OK;
}

int SOCKS5ClientSocket::DoHandshakeRead() {
  next_state_ = STATE_HANDSHAKE_READ_COMPLETE;

  if (buffer_.empty()) {
    bytes_received_ = 0;
    read_header_size = kReadHeaderSize;
  }

  int handshake_buf_len = read_header_size - bytes_received_;
  handshake_buf_ = new IOBuffer(handshake_buf_len);
  return transport_->socket()->Read(handshake_buf_, handshake_buf_len,
                                    &io_callback_);
}

int SOCKS5ClientSocket::DoHandshakeReadComplete(int result) {
  if (result < 0)
    return result;

  // The underlying socket closed unexpectedly.
  if (result == 0) {
    net_log_.AddEvent(NetLog::TYPE_SOCKS_UNEXPECTEDLY_CLOSED_DURING_HANDSHAKE,
                      NULL);
    return ERR_SOCKS_CONNECTION_FAILED;
  }

  buffer_.append(handshake_buf_->data(), result);
  bytes_received_ += result;

  // When the first few bytes are read, check how many more are required
  // and accordingly increase them
  if (bytes_received_ == kReadHeaderSize) {
    if (buffer_[0] != kSOCKS5Version || buffer_[2] != kNullByte) {
      net_log_.AddEvent(
          NetLog::TYPE_SOCKS_UNEXPECTED_VERSION,
          make_scoped_refptr(
              new NetLogIntegerParameter("version", buffer_[0])));
      return ERR_SOCKS_CONNECTION_FAILED;
    }
    if (buffer_[1] != 0x00) {
      net_log_.AddEvent(
          NetLog::TYPE_SOCKS_SERVER_ERROR,
          make_scoped_refptr(
              new NetLogIntegerParameter("error_code", buffer_[1])));
      return ERR_SOCKS_CONNECTION_FAILED;
    }

    // We check the type of IP/Domain the server returns and accordingly
    // increase the size of the response. For domains, we need to read the
    // size of the domain, so the initial request size is upto the domain
    // size. Since for IPv4/IPv6 the size is fixed and hence no 'size' is
    // read, we substract 1 byte from the additional request size.
    SocksEndPointAddressType address_type =
        static_cast<SocksEndPointAddressType>(buffer_[3]);
    if (address_type == kEndPointDomain)
      read_header_size += static_cast<uint8>(buffer_[4]);
    else if (address_type == kEndPointResolvedIPv4)
      read_header_size += sizeof(struct in_addr) - 1;
    else if (address_type == kEndPointResolvedIPv6)
      read_header_size += sizeof(struct in6_addr) - 1;
    else {
      net_log_.AddEvent(
          NetLog::TYPE_SOCKS_UNKNOWN_ADDRESS_TYPE,
          make_scoped_refptr(
              new NetLogIntegerParameter("address_type", buffer_[3])));
      return ERR_SOCKS_CONNECTION_FAILED;
    }

    read_header_size += 2;  // for the port.
    next_state_ = STATE_HANDSHAKE_READ;
    return OK;
  }

  // When the final bytes are read, setup handshake. We ignore the rest
  // of the response since they represent the SOCKSv5 endpoint and have
  // no use when doing a tunnel connection.
  if (bytes_received_ == read_header_size) {
    completed_handshake_ = true;
    buffer_.clear();
    next_state_ = STATE_NONE;
    return OK;
  }

  next_state_ = STATE_HANDSHAKE_READ;
  return OK;
}

int SOCKS5ClientSocket::GetPeerAddress(AddressList* address) const {
  return transport_->socket()->GetPeerAddress(address);
}

int SOCKS5ClientSocket::GetLocalAddress(IPEndPoint* address) const {
  return transport_->socket()->GetLocalAddress(address);
}

}  // namespace net
