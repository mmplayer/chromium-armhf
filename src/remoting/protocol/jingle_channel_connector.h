// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_CHANNEL_CONNECTOR_H_
#define REMOTING_PROTOCOL_JINGLE_CHANNEL_CONNECTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/threading/non_thread_safe.h"

namespace cricket {
class TransportChannel;
}  // namespace cricket

namespace crypto {
class RSAPrivateKey;
}  // namespace crypto

namespace remoting {
namespace protocol {

class JingleChannelConnector : public base::NonThreadSafe {
 public:
  JingleChannelConnector() { }
  virtual ~JingleChannelConnector() { }

  virtual void Connect(bool initiator,
                       const std::string& local_cert,
                       const std::string& remote_cert,
                       crypto::RSAPrivateKey* local_private_key,
                       cricket::TransportChannel* raw_channel) = 0;

 protected:
  DISALLOW_COPY_AND_ASSIGN(JingleChannelConnector);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_JINGLE_CHANNEL_CONNECTOR_H_
