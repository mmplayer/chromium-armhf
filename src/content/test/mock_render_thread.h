// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_RENDER_THREAD_H_
#define CONTENT_TEST_MOCK_RENDER_THREAD_H_
#pragma once

#include "base/shared_memory.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_test_sink.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"

namespace IPC {
class MessageReplyDeserializer;
}

namespace content {

// This class is a very simple mock of RenderThread. It simulates an IPC channel
// which supports only two messages:
// ViewHostMsg_CreateWidget : sync message sent by the Widget.
// ViewMsg_Close : async, send to the Widget.
class MockRenderThread : public content::RenderThread {
 public:
  MockRenderThread();
  virtual ~MockRenderThread();

  // Provides access to the messages that have been received by this thread.
  IPC::TestSink& sink() { return sink_; }

  // content::RenderThread implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;
  virtual MessageLoop* GetMessageLoop() OVERRIDE;
  virtual IPC::SyncChannel* GetChannel() OVERRIDE;
  virtual std::string GetLocale() OVERRIDE;
  virtual void AddRoute(int32 routing_id,
                        IPC::Channel::Listener* listener) OVERRIDE;
  virtual void RemoveRoute(int32 routing_id) OVERRIDE;
  virtual int GenerateRoutingID() OVERRIDE;
  virtual void AddFilter(IPC::ChannelProxy::MessageFilter* filter) OVERRIDE;
  virtual void RemoveFilter(IPC::ChannelProxy::MessageFilter* filter) OVERRIDE;
  virtual void SetOutgoingMessageFilter(
      IPC::ChannelProxy::OutgoingMessageFilter* filter) OVERRIDE;
  virtual void AddObserver(content::RenderProcessObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      content::RenderProcessObserver* observer) OVERRIDE;
  virtual void SetResourceDispatcherDelegate(
      content::ResourceDispatcherDelegate* delegate) OVERRIDE;
  virtual void WidgetHidden() OVERRIDE;
  virtual void WidgetRestored() OVERRIDE;
  virtual void EnsureWebKitInitialized() OVERRIDE;
  virtual void RecordUserMetrics(const std::string& action) OVERRIDE;
  virtual base::SharedMemoryHandle HostAllocateSharedMemoryBuffer(
      uint32 buffer_size) OVERRIDE;
  virtual void RegisterExtension(v8::Extension* extension) OVERRIDE;
  virtual bool IsRegisteredExtension(
      const std::string& v8_extension_name) const OVERRIDE;
  virtual void ScheduleIdleHandler(double initial_delay_s) OVERRIDE;
  virtual void IdleHandler() OVERRIDE;
  virtual double GetIdleNotificationDelayInS() const OVERRIDE;
  virtual void SetIdleNotificationDelayInS(
      double idle_notification_delay_in_s) OVERRIDE;
#if defined(OS_WIN)
  virtual void PreCacheFont(const LOGFONT& log_font) OVERRIDE;
  virtual void ReleaseCachedFonts() OVERRIDE;
#endif

  //////////////////////////////////////////////////////////////////////////
  // The following functions are called by the test itself.

  void set_routing_id(int32 id) {
    routing_id_ = id;
  }

  int32 opener_id() const {
    return opener_id_;
  }

  bool has_widget() const {
    return widget_ ? true : false;
  }

  // Simulates the Widget receiving a close message. This should result
  // on releasing the internal reference counts and destroying the internal
  // state.
  void SendCloseMessage();

 protected:
  // This function operates as a regular IPC listener. Subclasses
  // overriding this should first delegate to this implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  // The Widget expects to be returned valid route_id.
  void OnMsgCreateWidget(int opener_id,
                         WebKit::WebPopupType popup_type,
                         int* route_id);

#if defined(OS_WIN)
  void OnDuplicateSection(base::SharedMemoryHandle renderer_handle,
                          base::SharedMemoryHandle* browser_handle);
#endif

  IPC::TestSink sink_;

  // Routing id what will be assigned to the Widget.
  int32 routing_id_;

  // Opener id reported by the Widget.
  int32 opener_id_;

  // We only keep track of one Widget, we learn its pointer when it
  // adds a new route.
  IPC::Channel::Listener* widget_;

  // The last known good deserializer for sync messages.
  scoped_ptr<IPC::MessageReplyDeserializer> reply_deserializer_;
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_RENDER_THREAD_H_
