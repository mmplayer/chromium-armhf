// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoWriter is a generic interface for a video stream writer. RtpVideoWriter
// and ProtobufVideoWriter implement this interface for RTP and protobuf video
// streams. VideoWriter is used by ConnectionToClient to write into the video
// stream.

#ifndef REMOTING_PROTOCOL_VIDEO_WRITER_H_
#define REMOTING_PROTOCOL_VIDEO_WRITER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "remoting/protocol/video_stub.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace remoting {
namespace protocol {

class Session;
class SessionConfig;

class VideoWriter : public VideoStub {
 public:
  virtual ~VideoWriter();

  // The callback is called when initialization is finished. The
  // parameter is set to true on success.
  typedef base::Callback<void(bool)> InitializedCallback;

  static VideoWriter* Create(base::MessageLoopProxy* message_loop,
                             const SessionConfig& config);

  // Initializes the writer.
  virtual void Init(Session* session, const InitializedCallback& callback) = 0;

  // Stops writing. Must be called on the network thread before this
  // object is destroyed.
  virtual void Close() = 0;

 protected:
  VideoWriter() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoWriter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_VIDEO_WRITER_H_
