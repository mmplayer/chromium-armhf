// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface of a client that receives commands from a Chromoting host.
//
// This interface is responsible for a subset of control messages sent to
// the Chromoting client.

#ifndef REMOTING_PROTOCOL_CLIENT_STUB_H_
#define REMOTING_PROTOCOL_CLIENT_STUB_H_

#include "base/basictypes.h"
#include "base/callback.h"

namespace remoting {
namespace protocol {

class LocalLoginStatus;
class NotifyResolutionRequest;

class ClientStub {
 public:
  ClientStub() {}
  virtual ~ClientStub() {}

  virtual void BeginSessionResponse(const LocalLoginStatus* msg,
                                    const base::Closure& done) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientStub);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CLIENT_STUB_H_
