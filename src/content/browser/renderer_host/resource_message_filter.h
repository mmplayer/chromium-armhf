// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RESOURCE_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RESOURCE_MESSAGE_FILTER_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/browser_message_filter.h"
#include "content/common/content_export.h"
#include "content/common/child_process_info.h"
#include "webkit/glue/resource_type.h"

class ResourceDispatcherHost;

namespace content {
class ResourceContext;
}  // namespace content

namespace net {
class URLRequestContext;
}  // namespace net

// This class filters out incoming IPC messages for network requests and
// processes them on the IPC thread.  As a result, network requests are not
// delayed by costly UI processing that may be occuring on the main thread of
// the browser.  It also means that any hangs in starting a network request
// will not interfere with browser UI.
class CONTENT_EXPORT ResourceMessageFilter : public BrowserMessageFilter {
 public:
  // Allows selecting the net::URLRequestContext used to service requests.
  class URLRequestContextSelector {
   public:
    URLRequestContextSelector() {}
    virtual ~URLRequestContextSelector() {}

    virtual net::URLRequestContext* GetRequestContext(
        ResourceType::Type request_type) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(URLRequestContextSelector);
  };

  ResourceMessageFilter(int child_id,
                        ChildProcessInfo::ProcessType process_type,
                        const content::ResourceContext* resource_context,
                        URLRequestContextSelector* url_request_context_selector,
                        ResourceDispatcherHost* resource_dispatcher_host);

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing();
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);

  const content::ResourceContext& resource_context() const {
    return *resource_context_;
  }

  // Returns the net::URLRequestContext for the given request.
  net::URLRequestContext* GetURLRequestContext(
      ResourceType::Type request_type);

  int child_id() const { return child_id_; }
  ChildProcessInfo::ProcessType process_type() const { return process_type_; }

 protected:
  // Protected destructor so that we can be overriden in tests.
  virtual ~ResourceMessageFilter();

 private:
  // The ID of the child process.
  int child_id_;

  ChildProcessInfo::ProcessType process_type_;

  // Owned by ProfileIOData* which is guaranteed to outlive us.
  const content::ResourceContext* const resource_context_;

  const scoped_ptr<URLRequestContextSelector> url_request_context_selector_;

  // Owned by BrowserProcess, which is guaranteed to outlive us.
  ResourceDispatcherHost* resource_dispatcher_host_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ResourceMessageFilter);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_RESOURCE_MESSAGE_FILTER_H_
