// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_SCRIPT_FETCHER_IMPL_H_
#define NET_PROXY_PROXY_SCRIPT_FETCHER_IMPL_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/task.h"
#include "base/time.h"
#include "net/proxy/proxy_script_fetcher.h"
#include "net/url_request/url_request.h"

class GURL;
class X509Certificate;

namespace net {

class URLRequestContext;

// Implementation of ProxyScriptFetcher that downloads scripts using the
// specified request context.
class NET_EXPORT ProxyScriptFetcherImpl : public ProxyScriptFetcher,
                                          public URLRequest::Delegate {
 public:
  // Creates a ProxyScriptFetcher that issues requests through
  // |url_request_context|. |url_request_context| must remain valid for the
  // lifetime of ProxyScriptFetcherImpl.
  // Note that while a request is in progress, we will be holding a reference
  // to |url_request_context|. Be careful not to create cycles between the
  // fetcher and the context; you can break such cycles by calling Cancel().
  explicit ProxyScriptFetcherImpl(URLRequestContext* url_request_context);

  virtual ~ProxyScriptFetcherImpl();

  // Used by unit-tests to modify the default limits.
  base::TimeDelta SetTimeoutConstraint(base::TimeDelta timeout);
  size_t SetSizeConstraint(size_t size_bytes);

  void OnResponseCompleted(URLRequest* request);

  // ProxyScriptFetcher methods:
  virtual int Fetch(const GURL& url, string16* text,
                    OldCompletionCallback* callback) OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual URLRequestContext* GetRequestContext() const OVERRIDE;

  // URLRequest::Delegate methods:
  virtual void OnAuthRequired(URLRequest* request,
                              AuthChallengeInfo* auth_info) OVERRIDE;
  virtual void OnSSLCertificateError(URLRequest* request,
                                     const SSLInfo& ssl_info,
                                     bool is_hsts_ok) OVERRIDE;
  virtual void OnResponseStarted(URLRequest* request) OVERRIDE;
  virtual void OnReadCompleted(URLRequest* request, int num_bytes) OVERRIDE;

 private:
  enum { kBufSize = 4096 };

  // Read more bytes from the response.
  void ReadBody(URLRequest* request);

  // Handles a response from Read(). Returns true if we should continue trying
  // to read. |num_bytes| is 0 for EOF, and < 0 on errors.
  bool ConsumeBytesRead(URLRequest* request, int num_bytes);

  // Called once the request has completed to notify the caller of
  // |response_code_| and |response_text_|.
  void FetchCompleted();

  // Clear out the state for the current request.
  void ResetCurRequestState();

  // Callback for time-out task of request with id |id|.
  void OnTimeout(int id);

  // Factory for creating the time-out task. This takes care of revoking
  // outstanding tasks when |this| is deleted.
  ScopedRunnableMethodFactory<ProxyScriptFetcherImpl> task_factory_;

  // The context used for making network requests.
  URLRequestContext* url_request_context_;

  // Buffer that URLRequest writes into.
  scoped_refptr<IOBuffer> buf_;

  // The next ID to use for |cur_request_| (monotonically increasing).
  int next_id_;

  // The current (in progress) request, or NULL.
  scoped_ptr<URLRequest> cur_request_;

  // State for current request (only valid when |cur_request_| is not NULL):

  // Unique ID for the current request.
  int cur_request_id_;

  // Callback to invoke on completion of the fetch.
  OldCompletionCallback* callback_;

  // Holds the error condition that was hit on the current request, or OK.
  int result_code_;

  // Holds the bytes read so far. Will not exceed |max_response_bytes|.
  std::string bytes_read_so_far_;

  // This buffer is owned by the owner of |callback|, and will be filled with
  // UTF16 response on completion.
  string16* result_text_;

  // The maximum number of bytes to allow in responses.
  size_t max_response_bytes_;

  // The maximum amount of time to wait for download to complete.
  base::TimeDelta max_duration_;

  DISALLOW_COPY_AND_ASSIGN(ProxyScriptFetcherImpl);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_SCRIPT_FETCHER_IMPL_H_
