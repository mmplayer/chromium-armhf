// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_MOCK_PROXY_RESOLVER_H_
#define NET_PROXY_MOCK_PROXY_RESOLVER_H_
#pragma once

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_resolver.h"

class MessageLoop;

namespace net {

// Asynchronous mock proxy resolver. All requests complete asynchronously,
// user must call Request::CompleteNow() on a pending request to signal it.
class MockAsyncProxyResolverBase : public ProxyResolver {
 public:
  class Request : public base::RefCounted<Request> {
   public:
    Request(MockAsyncProxyResolverBase* resolver,
            const GURL& url,
            ProxyInfo* results,
            OldCompletionCallback* callback);

    const GURL& url() const { return url_; }
    ProxyInfo* results() const { return results_; }
    OldCompletionCallback* callback() const { return callback_; }

    void CompleteNow(int rv);

   private:
    friend class base::RefCounted<Request>;

    virtual ~Request();

    MockAsyncProxyResolverBase* resolver_;
    const GURL url_;
    ProxyInfo* results_;
    OldCompletionCallback* callback_;
    MessageLoop* origin_loop_;
  };

  class SetPacScriptRequest {
   public:
    SetPacScriptRequest(
        MockAsyncProxyResolverBase* resolver,
        const scoped_refptr<ProxyResolverScriptData>& script_data,
        OldCompletionCallback* callback);
    ~SetPacScriptRequest();

    const ProxyResolverScriptData* script_data() const { return script_data_; }

    void CompleteNow(int rv);

   private:
    MockAsyncProxyResolverBase* resolver_;
    const scoped_refptr<ProxyResolverScriptData> script_data_;
    OldCompletionCallback* callback_;
    MessageLoop* origin_loop_;
  };

  typedef std::vector<scoped_refptr<Request> > RequestsList;

  virtual ~MockAsyncProxyResolverBase();

  // ProxyResolver implementation:
  virtual int GetProxyForURL(const GURL& url,
                             ProxyInfo* results,
                             OldCompletionCallback* callback,
                             RequestHandle* request_handle,
                             const BoundNetLog& /*net_log*/) OVERRIDE;
  virtual void CancelRequest(RequestHandle request_handle) OVERRIDE;
  virtual int SetPacScript(
      const scoped_refptr<ProxyResolverScriptData>& script_data,
      OldCompletionCallback* callback) OVERRIDE;
  virtual void CancelSetPacScript() OVERRIDE;

  const RequestsList& pending_requests() const {
    return pending_requests_;
  }

  const RequestsList& cancelled_requests() const {
    return cancelled_requests_;
  }

  SetPacScriptRequest* pending_set_pac_script_request() const;

  void RemovePendingRequest(Request* request);

  void RemovePendingSetPacScriptRequest(SetPacScriptRequest* request);

 protected:
  explicit MockAsyncProxyResolverBase(bool expects_pac_bytes);

 private:
  RequestsList pending_requests_;
  RequestsList cancelled_requests_;
  scoped_ptr<SetPacScriptRequest> pending_set_pac_script_request_;
};

class MockAsyncProxyResolver : public MockAsyncProxyResolverBase {
 public:
  MockAsyncProxyResolver()
      : MockAsyncProxyResolverBase(false /*expects_pac_bytes*/) {}
};

class MockAsyncProxyResolverExpectsBytes : public MockAsyncProxyResolverBase {
 public:
  MockAsyncProxyResolverExpectsBytes()
      : MockAsyncProxyResolverBase(true /*expects_pac_bytes*/) {}
};

}  // namespace net

#endif  // NET_PROXY_MOCK_PROXY_RESOLVER_H_
