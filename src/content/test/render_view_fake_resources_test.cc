// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/render_view_fake_resources_test.h"

#include <string.h>

#include "base/process.h"
#include "base/shared_memory.h"
#include "base/time.h"
#include "content/common/dom_storage_common.h"
#include "content/common/resource_messages.h"
#include "content/common/resource_response.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/test/mock_render_process.h"
#include "googleurl/src/gurl.h"
#include "net/base/upload_data.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_status.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHistoryItem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_MACOSX)
#include "third_party/WebKit/Source/WebKit/mac/WebCoreSupport/WebSystemInterface.h"
#endif

const int32 RenderViewFakeResourcesTest::kViewId = 5;

RenderViewFakeResourcesTest::RenderViewFakeResourcesTest() {}
RenderViewFakeResourcesTest::~RenderViewFakeResourcesTest() {}

bool RenderViewFakeResourcesTest::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(RenderViewFakeResourcesTest, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewReady, OnRenderViewReady)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidStopLoading, OnDidStopLoading)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_RequestResource, OnRequestResource)
  IPC_END_MESSAGE_MAP()
  return true;
}

bool RenderViewFakeResourcesTest::Visit(content::RenderView* render_view) {
  view_ = render_view;
  return false;
}

void RenderViewFakeResourcesTest::SetUp() {
  // Set up the renderer.  This code is largely adapted from
  // render_view_test.cc and renderer_main.cc.  Note that we use a
  // MockRenderProcess (because we don't need to use IPC for painting),
  // but we use a real RenderThread so that we can use the ResourceDispatcher
  // to fetch network resources.  These are then served canned content
  // in OnRequestResource().
  content::GetContentClient()->set_renderer(&content_renderer_client_);
  static const char kThreadName[] = "RenderViewFakeResourcesTest";
  channel_.reset(new IPC::Channel(kThreadName,
                                  IPC::Channel::MODE_SERVER, this));
  ASSERT_TRUE(channel_->Connect());

  webkit_glue::SetJavaScriptFlags("--expose-gc");
  mock_process_.reset(new MockRenderProcess);
  render_thread_ = new RenderThreadImpl(kThreadName);
  mock_process_->set_main_thread(render_thread_);
#if defined(OS_MACOSX)
  InitWebCoreSystemInterface();
#endif

  // Tell the renderer to create a view, then wait until it's ready.
  // We can't call View::Create() directly here or else we won't get
  // RenderProcess's lazy initialization of WebKit.
  view_ = NULL;
  ViewMsg_New_Params params;
  params.parent_window = 0;
  params.view_id = kViewId;
  params.session_storage_namespace_id = kInvalidSessionStorageNamespaceId;
  ASSERT_TRUE(channel_->Send(new ViewMsg_New(params)));
  message_loop_.Run();
}

void RenderViewFakeResourcesTest::TearDown() {
  // Try very hard to collect garbage before shutting down.
  GetMainFrame()->collectGarbage();
  GetMainFrame()->collectGarbage();

  ASSERT_TRUE(channel_->Send(new ViewMsg_Close(kViewId)));
  do {
    message_loop_.RunAllPending();
    view_ = NULL;
    content::RenderView::ForEach(this);
  } while (view_);

  mock_process_.reset();
}

content::RenderView* RenderViewFakeResourcesTest::view() {
  return view_;
}

WebKit::WebFrame* RenderViewFakeResourcesTest::GetMainFrame() {
  return view_->GetWebView()->mainFrame();
}

void RenderViewFakeResourcesTest::LoadURL(const std::string& url) {
  GURL g_url(url);
  GetMainFrame()->loadRequest(WebKit::WebURLRequest(g_url));
  message_loop_.Run();
}

void RenderViewFakeResourcesTest::LoadURLWithPost(const std::string& url) {
  GURL g_url(url);
  WebKit::WebURLRequest request(g_url);
  request.setHTTPMethod(WebKit::WebString::fromUTF8("POST"));
  GetMainFrame()->loadRequest(request);
  message_loop_.Run();
}

void RenderViewFakeResourcesTest::GoBack() {
  GoToOffset(-1, GetMainFrame()->previousHistoryItem());
}

void RenderViewFakeResourcesTest::GoForward(
    const WebKit::WebHistoryItem& history_item) {
  GoToOffset(1, history_item);
}

void RenderViewFakeResourcesTest::OnDidStopLoading() {
  message_loop_.Quit();
}

void RenderViewFakeResourcesTest::OnRequestResource(
    const IPC::Message& message,
    int request_id,
    const ResourceHostMsg_Request& request_data) {
  std::string headers, body;
  std::map<std::string, std::string>::const_iterator it =
      responses_.find(request_data.url.spec());
  if (it == responses_.end()) {
    headers = "HTTP/1.1 404 Not Found\0Content-Type:text/html\0\0";
    body = "content not found";
  } else {
    headers = "HTTP/1.1 200 OK\0Content-Type:text/html\0\0";
    body = it->second;
  }

  ResourceResponseHead response_head;
  response_head.headers = new net::HttpResponseHeaders(headers);
  response_head.mime_type = "text/html";
  ASSERT_TRUE(channel_->Send(new ResourceMsg_ReceivedResponse(
      message.routing_id(), request_id, response_head)));

  base::SharedMemory shared_memory;
  ASSERT_TRUE(shared_memory.CreateAndMapAnonymous(body.size()));
  memcpy(shared_memory.memory(), body.data(), body.size());

  base::SharedMemoryHandle handle;
  ASSERT_TRUE(shared_memory.GiveToProcess(base::Process::Current().handle(),
                                          &handle));
  ASSERT_TRUE(channel_->Send(new ResourceMsg_DataReceived(
      message.routing_id(),
      request_id,
      handle,
      body.size(),
      body.size())));

  ASSERT_TRUE(channel_->Send(new ResourceMsg_RequestComplete(
      message.routing_id(),
      request_id,
      net::URLRequestStatus(),
      std::string(),
      base::Time())));
}

void RenderViewFakeResourcesTest::OnRenderViewReady() {
  // Grab a pointer to the new view using RenderViewVisitor.
  ASSERT_TRUE(!view_);
  content::RenderView::ForEach(this);
  ASSERT_TRUE(view_);
  message_loop_.Quit();
}

void RenderViewFakeResourcesTest::GoToOffset(
    int offset,
    const WebKit::WebHistoryItem& history_item) {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  ViewMsg_Navigate_Params params;
  params.page_id = impl->GetPageId() + offset;
  params.pending_history_list_offset =
      impl->history_list_offset() + offset;
  params.current_history_list_offset = impl->history_list_offset();
  params.current_history_list_length = (impl->historyBackListCount() +
                                        impl->historyForwardListCount() + 1);
  params.url = GURL(history_item.urlString());
  params.transition = content::PAGE_TRANSITION_FORWARD_BACK;
  params.state = webkit_glue::HistoryItemToString(history_item);
  params.navigation_type = ViewMsg_Navigate_Type::NORMAL;
  params.request_time = base::Time::Now();
  channel_->Send(new ViewMsg_Navigate(impl->routing_id(), params));
  message_loop_.Run();
}
