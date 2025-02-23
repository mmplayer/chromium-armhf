// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PepperViewProxy is used to invoke PepperView object on pepper thread. It
// has the same interface as PepperView. When a method calls is received on
// any chromoting threads it delegates the method call to pepper thread.
// It also provide a detach mechanism so that when PepperView object is
// destroyed PepperViewProxy will not call it anymore. This is important in
// providing a safe shutdown of ChromotingInstance and PepperView.

// This object is accessed on chromoting threads and pepper thread. The internal
// PepperView object is only accessed on pepper thread so as the Detach() method
// call.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_PROXY_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_PROXY_H_

#include "base/memory/ref_counted.h"
#include "remoting/client/plugin/pepper_view.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace remoting {

class ChromotingInstance;
class ClientContext;

class PepperViewProxy : public base::RefCountedThreadSafe<PepperViewProxy>,
                        public ChromotingView,
                        public FrameConsumer {
 public:
  PepperViewProxy(ChromotingInstance* instance, PepperView* view,
                  base::MessageLoopProxy* plugin_message_loop);
  virtual ~PepperViewProxy();

  // ChromotingView implementation.
  virtual bool Initialize() OVERRIDE;
  virtual void TearDown() OVERRIDE;
  virtual void Paint() OVERRIDE;
  virtual void SetSolidFill(uint32 color) OVERRIDE;
  virtual void UnsetSolidFill() OVERRIDE;
  virtual void SetConnectionState(
      protocol::ConnectionToHost::State state,
      protocol::ConnectionToHost::Error error) OVERRIDE;
  virtual void UpdateLoginStatus(bool success, const std::string& info)
      OVERRIDE;

  // This method returns a value, so must run synchronously, so must be
  // called only on the pepper thread.
  virtual double GetHorizontalScaleRatio() const OVERRIDE;
  virtual double GetVerticalScaleRatio() const OVERRIDE;

  // FrameConsumer implementation.
  virtual void AllocateFrame(media::VideoFrame::Format format,
                             size_t width,
                             size_t height,
                             base::TimeDelta timestamp,
                             base::TimeDelta duration,
                             scoped_refptr<media::VideoFrame>* frame_out,
                             Task* done);
  virtual void ReleaseFrame(media::VideoFrame* frame);
  virtual void OnPartialFrameOutput(media::VideoFrame* frame,
                                    RectVector* rects,
                                    Task* done);

  // Remove the reference to |instance_| and |view_| by setting the value to
  // NULL.
  // This method should only be called on pepper thread.
  void Detach();

 private:
  // This variable is accessed on chromoting threads and pepper thread.
  // This is initialized when this object is constructed. Its value is reset
  // to NULL on pepper thread when Detach() is called and there will be no
  // other threads accessing this variable at the same time. Given the above
  // conditions locking this variable is not necessary.
  ChromotingInstance* instance_;

  // This variable is only accessed on the pepper thread. Locking is not
  // necessary.
  PepperView* view_;

  scoped_refptr<base::MessageLoopProxy> plugin_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(PepperViewProxy);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_VIEW_PROXY_H_
