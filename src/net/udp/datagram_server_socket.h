// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_UDP_DATAGRAM_SERVER_SOCKET_H_
#define NET_UDP_DATAGRAM_SERVER_SOCKET_H_
#pragma once

#include "net/base/completion_callback.h"
#include "net/udp/datagram_socket.h"

namespace net {

class IPEndPoint;
class IOBuffer;

// A UDP Socket.
class NET_EXPORT DatagramServerSocket : public DatagramSocket {
 public:
  virtual ~DatagramServerSocket() {}

  // Initialize this socket as a server socket listening at |address|.
  // Returns a network error code.
  virtual int Listen(const IPEndPoint& address) = 0;

  // Read from a socket and receive sender address information.
  // |buf| is the buffer to read data into.
  // |buf_len| is the maximum amount of data to read.
  // |address| is a buffer provided by the caller for receiving the sender
  //   address information about the received data.  This buffer must be kept
  //   alive by the caller until the callback is placed.
  // |address_length| is a ptr to the length of the |address| buffer.  This
  //   is an input parameter containing the maximum size |address| can hold
  //   and also an output parameter for the size of |address| upon completion.
  // |callback| the callback on completion of the Recv.
  // Returns a net error code, or ERR_IO_PENDING if the IO is in progress.
  // If ERR_IO_PENDING is returned, the caller must keep |buf|, |address|,
  // and |address_length| alive until the callback is called.
  virtual int RecvFrom(IOBuffer* buf,
                       int buf_len,
                       IPEndPoint* address,
                       OldCompletionCallback* callback) = 0;

  // Send to a socket with a particular destination.
  // |buf| is the buffer to send
  // |buf_len| is the number of bytes to send
  // |address| is the recipient address.
  // |address_length| is the size of the recipient address
  // |callback| is the user callback function to call on complete.
  // Returns a net error code, or ERR_IO_PENDING if the IO is in progress.
  // If ERR_IO_PENDING is returned, the caller must keep |buf| and |address|
  // alive until the callback is called.
  virtual int SendTo(IOBuffer* buf,
                     int buf_len,
                     const IPEndPoint& address,
                     OldCompletionCallback* callback) = 0;

  // Set the receive buffer size (in bytes) for the socket.
  virtual bool SetReceiveBufferSize(int32 size) = 0;

  // Set the send buffer size (in bytes) for the socket.
  virtual bool SetSendBufferSize(int32 size) = 0;
};

}  // namespace net

#endif  // NET_UDP_DATAGRAM_SERVER_SOCKET_H_
