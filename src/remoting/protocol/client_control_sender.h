// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of ClientStub using sockets created from jingle connection.
// It sends messages through the socket after serializing it.
//
// Object of this class can only be created by ConnectionToClient.
//
// This class can be used on any thread.

#ifndef REMOTING_PROTOCOL_CLIENT_STUB_IMPL_H_
#define REMOTING_PROTOCOL_CLIENT_STUB_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "remoting/protocol/client_stub.h"

class Task;

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace net {
class Socket;
}  // namespace net

namespace remoting {
namespace protocol {

class BufferedSocketWriter;

// Implementation of ClientStub that sends commands on a socket. Must
// be created and closed on the network thread, but can be used on any
// other thread.
class ClientControlSender : public ClientStub {
 public:
  explicit ClientControlSender(base::MessageLoopProxy* message_loop,
                               net::Socket* socket);
  virtual ~ClientControlSender();

  virtual void BeginSessionResponse(const LocalLoginStatus* msg,
                                    const base::Closure& done) OVERRIDE;

  // Stop writing. Must be called on the network thread when the
  // underlying socket is being destroyed.
  void Close();

 private:
  // Buffered socket writer holds the serialized message and send it on the
  // right thread.
  scoped_refptr<BufferedSocketWriter> buffered_writer_;

  DISALLOW_COPY_AND_ASSIGN(ClientControlSender);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CLIENT_STUB_IMPL_H_
