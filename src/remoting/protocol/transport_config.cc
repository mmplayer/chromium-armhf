// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/transport_config.h"

namespace remoting {
namespace protocol {

TransportConfig::TransportConfig()
    : nat_traversal(false) {
}

TransportConfig::~TransportConfig() {
}

}  // namespace protocol
}  // namespace remoting
