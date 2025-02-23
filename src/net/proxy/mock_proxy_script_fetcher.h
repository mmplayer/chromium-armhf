// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_MOCK_PROXY_SCRIPT_FETCHER_H_
#define NET_PROXY_MOCK_PROXY_SCRIPT_FETCHER_H_
#pragma once

#include "base/compiler_specific.h"
#include "googleurl/src/gurl.h"
#include "net/proxy/proxy_script_fetcher.h"

#include <string>

namespace net {

class URLRequestContext;

// A mock ProxyScriptFetcher. No result will be returned to the fetch client
// until we call NotifyFetchCompletion() to set the results.
class MockProxyScriptFetcher : public ProxyScriptFetcher {
 public:
  MockProxyScriptFetcher();

  // ProxyScriptFetcher implementation.
  virtual int Fetch(const GURL& url,
                    string16* text,
                    OldCompletionCallback* callback) OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual URLRequestContext* GetRequestContext() const OVERRIDE;

  void NotifyFetchCompletion(int result, const std::string& ascii_text);
  const GURL& pending_request_url() const;
  bool has_pending_request() const;

 private:
  GURL pending_request_url_;
  OldCompletionCallback* pending_request_callback_;
  string16* pending_request_text_;
};

}  // namespace net

#endif  // NET_PROXY_MOCK_PROXY_SCRIPT_FETCHER_H_
