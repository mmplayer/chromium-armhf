// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RESOLVE_PROXY_MSG_HELPER_H_
#define CONTENT_BROWSER_RESOLVE_PROXY_MSG_HELPER_H_
#pragma once

#include <deque>
#include <string>

#include "base/memory/ref_counted.h"
#include "content/browser/browser_message_filter.h"
#include "content/common/content_export.h"
#include "googleurl/src/gurl.h"
#include "net/base/completion_callback.h"
#include "net/proxy/proxy_service.h"

namespace net {
class URLRequestContextGetter;
}

// Responds to ChildProcessHostMsg_ResolveProxy, kicking off a ProxyResolve
// request on the IO thread using the specified proxy service.  Completion is
// notified through the delegate.  If multiple requests are started at the same
// time, they will run in FIFO order, with only 1 being outstanding at a time.
//
// When an instance of ResolveProxyMsgHelper is destroyed, it cancels any
// outstanding proxy resolve requests with the proxy service. It also deletes
// the stored IPC::Message pointers for pending requests.
//
// This object is expected to live on the IO thread.
class CONTENT_EXPORT ResolveProxyMsgHelper : public BrowserMessageFilter {
 public:
  explicit ResolveProxyMsgHelper(net::URLRequestContextGetter* getter);
  // Constructor used by unittests.
  explicit ResolveProxyMsgHelper(net::ProxyService* proxy_service);

  // Destruction cancels the current outstanding request, and clears the
  // pending queue.
  virtual ~ResolveProxyMsgHelper();

  // BrowserMessageFilter implementation
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);

  void OnResolveProxy(const GURL& url, IPC::Message* reply_msg);

 private:
  // Callback for the ProxyService (bound to |callback_|).
  void OnResolveProxyCompleted(int result);

  // Starts the first pending request.
  void StartPendingRequest();

  // A PendingRequest is a resolve request that is in progress, or queued.
  struct PendingRequest {
   public:
     PendingRequest(const GURL& url, IPC::Message* reply_msg) :
         url(url), reply_msg(reply_msg), pac_req(NULL) { }

     // The URL of the request.
     GURL url;

     // Data to pass back to the delegate on completion (we own it until then).
     IPC::Message* reply_msg;

     // Handle for cancelling the current request if it has started (else NULL).
     net::ProxyService::PacRequest* pac_req;
  };

  // Members for the current outstanding proxy request.
  net::OldCompletionCallbackImpl<ResolveProxyMsgHelper> callback_;
  net::ProxyInfo proxy_info_;

  // FIFO queue of pending requests. The first entry is always the current one.
  typedef std::deque<PendingRequest> PendingRequestList;
  PendingRequestList pending_requests_;

  scoped_refptr<net::URLRequestContextGetter> context_getter_;
  net::ProxyService* proxy_service_;
};

#endif  // CONTENT_BROWSER_RESOLVE_PROXY_MSG_HELPER_H_
