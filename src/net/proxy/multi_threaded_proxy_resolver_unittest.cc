// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/multi_threaded_proxy_resolver.h"

#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/test_completion_callback.h"
#include "net/proxy/proxy_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// A synchronous mock ProxyResolver implementation, which can be used in
// conjunction with MultiThreadedProxyResolver.
//       - returns a single-item proxy list with the query's host.
class MockProxyResolver : public ProxyResolver {
 public:
  MockProxyResolver()
      : ProxyResolver(true /*expects_pac_bytes*/),
        wrong_loop_(MessageLoop::current()),
        request_count_(0),
        purge_count_(0),
        resolve_latency_ms_(0) {}

  // ProxyResolver implementation:
  virtual int GetProxyForURL(const GURL& query_url,
                             ProxyInfo* results,
                             OldCompletionCallback* callback,
                             RequestHandle* request,
                             const BoundNetLog& net_log) OVERRIDE {
    if (resolve_latency_ms_)
      base::PlatformThread::Sleep(resolve_latency_ms_);

    CheckIsOnWorkerThread();

    EXPECT_TRUE(callback == NULL);
    EXPECT_TRUE(request == NULL);

    // Write something into |net_log| (doesn't really have any meaning.)
    net_log.BeginEvent(NetLog::TYPE_PAC_JAVASCRIPT_DNS_RESOLVE, NULL);

    results->UseNamedProxy(query_url.host());

    // Return a success code which represents the request's order.
    return request_count_++;
  }

  virtual void CancelRequest(RequestHandle request) OVERRIDE {
    NOTREACHED();
  }

  virtual void CancelSetPacScript() OVERRIDE {
    NOTREACHED();
  }

  virtual int SetPacScript(
      const scoped_refptr<ProxyResolverScriptData>& script_data,
      OldCompletionCallback* callback) OVERRIDE {
    CheckIsOnWorkerThread();
    last_script_data_ = script_data;
    return OK;
  }

  virtual void PurgeMemory() OVERRIDE {
    CheckIsOnWorkerThread();
    ++purge_count_;
  }

  int purge_count() const { return purge_count_; }
  int request_count() const { return request_count_; }

  const ProxyResolverScriptData* last_script_data() const {
    return last_script_data_;
  }

  void SetResolveLatency(int latency_ms) {
    resolve_latency_ms_ = latency_ms;
  }

 private:
  void CheckIsOnWorkerThread() {
    // We should be running on the worker thread -- while we don't know the
    // message loop of MultiThreadedProxyResolver's worker thread, we do
    // know that it is going to be distinct from the loop running the
    // test, so at least make sure it isn't the main loop.
    EXPECT_NE(MessageLoop::current(), wrong_loop_);
  }

  MessageLoop* wrong_loop_;
  int request_count_;
  int purge_count_;
  scoped_refptr<ProxyResolverScriptData> last_script_data_;
  int resolve_latency_ms_;
};


// A mock synchronous ProxyResolver which can be set to block upon reaching
// GetProxyForURL().
// TODO(eroman): WaitUntilBlocked() *must* be called before calling Unblock(),
//               otherwise there will be a race on |should_block_| since it is
//               read without any synchronization.
class BlockableProxyResolver : public MockProxyResolver {
 public:
  BlockableProxyResolver()
      : should_block_(false),
        unblocked_(true, true),
        blocked_(true, false) {
  }

  void Block() {
    should_block_ = true;
    unblocked_.Reset();
  }

  void Unblock() {
    should_block_ = false;
    blocked_.Reset();
    unblocked_.Signal();
  }

  void WaitUntilBlocked() {
    blocked_.Wait();
  }

  virtual int GetProxyForURL(const GURL& query_url,
                             ProxyInfo* results,
                             OldCompletionCallback* callback,
                             RequestHandle* request,
                             const BoundNetLog& net_log) OVERRIDE {
    if (should_block_) {
      blocked_.Signal();
      unblocked_.Wait();
    }

    return MockProxyResolver::GetProxyForURL(
        query_url, results, callback, request, net_log);
  }

 private:
  bool should_block_;
  base::WaitableEvent unblocked_;
  base::WaitableEvent blocked_;
};

// ForwardingProxyResolver forwards all requests to |impl|.
class ForwardingProxyResolver : public ProxyResolver {
 public:
  explicit ForwardingProxyResolver(ProxyResolver* impl)
      : ProxyResolver(impl->expects_pac_bytes()),
        impl_(impl) {}

  virtual int GetProxyForURL(const GURL& query_url,
                             ProxyInfo* results,
                             OldCompletionCallback* callback,
                             RequestHandle* request,
                             const BoundNetLog& net_log) OVERRIDE {
    return impl_->GetProxyForURL(
        query_url, results, callback, request, net_log);
  }

  virtual void CancelRequest(RequestHandle request) OVERRIDE {
    impl_->CancelRequest(request);
  }

  virtual void CancelSetPacScript() OVERRIDE {
    impl_->CancelSetPacScript();
  }

  virtual int SetPacScript(
      const scoped_refptr<ProxyResolverScriptData>& script_data,
      OldCompletionCallback* callback) OVERRIDE {
    return impl_->SetPacScript(script_data, callback);
  }

  virtual void PurgeMemory() OVERRIDE {
    impl_->PurgeMemory();
  }

 private:
  ProxyResolver* impl_;
};

// This factory returns ProxyResolvers that forward all requests to
// |resolver|.
class ForwardingProxyResolverFactory : public ProxyResolverFactory {
 public:
  explicit ForwardingProxyResolverFactory(ProxyResolver* resolver)
      : ProxyResolverFactory(resolver->expects_pac_bytes()),
        resolver_(resolver) {}

  virtual ProxyResolver* CreateProxyResolver() OVERRIDE {
    return new ForwardingProxyResolver(resolver_);
  }

 private:
  ProxyResolver* resolver_;
};

// This factory returns new instances of BlockableProxyResolver.
class BlockableProxyResolverFactory : public ProxyResolverFactory {
 public:
  BlockableProxyResolverFactory() : ProxyResolverFactory(true) {}

  ~BlockableProxyResolverFactory() {
    STLDeleteElements(&resolvers_);
  }

  virtual ProxyResolver* CreateProxyResolver() OVERRIDE {
    BlockableProxyResolver* resolver = new BlockableProxyResolver;
    resolvers_.push_back(resolver);
    return new ForwardingProxyResolver(resolver);
  }

  std::vector<BlockableProxyResolver*> resolvers() {
    return resolvers_;
  }

 private:
  std::vector<BlockableProxyResolver*> resolvers_;
};

TEST(MultiThreadedProxyResolverTest, SingleThread_Basic) {
  const size_t kNumThreads = 1u;
  scoped_ptr<MockProxyResolver> mock(new MockProxyResolver);
  MultiThreadedProxyResolver resolver(
      new ForwardingProxyResolverFactory(mock.get()), kNumThreads);

  int rv;

  EXPECT_TRUE(resolver.expects_pac_bytes());

  // Call SetPacScriptByData() -- verify that it reaches the synchronous
  // resolver.
  TestOldCompletionCallback set_script_callback;
  rv = resolver.SetPacScript(
      ProxyResolverScriptData::FromUTF8("pac script bytes"),
      &set_script_callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, set_script_callback.WaitForResult());
  EXPECT_EQ(ASCIIToUTF16("pac script bytes"),
            mock->last_script_data()->utf16());

  // Start request 0.
  TestOldCompletionCallback callback0;
  CapturingBoundNetLog log0(CapturingNetLog::kUnbounded);
  ProxyInfo results0;
  rv = resolver.GetProxyForURL(
      GURL("http://request0"), &results0, &callback0, NULL, log0.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Wait for request 0 to finish.
  rv = callback0.WaitForResult();
  EXPECT_EQ(0, rv);
  EXPECT_EQ("PROXY request0:80", results0.ToPacString());

  // The mock proxy resolver should have written 1 log entry. And
  // on completion, this should have been copied into |log0|.
  // We also have 1 log entry that was emitted by the
  // MultiThreadedProxyResolver.
  net::CapturingNetLog::EntryList entries0;
  log0.GetEntries(&entries0);

  ASSERT_EQ(2u, entries0.size());
  EXPECT_EQ(NetLog::TYPE_SUBMITTED_TO_RESOLVER_THREAD, entries0[0].type);

  // Start 3 more requests (request1 to request3).

  TestOldCompletionCallback callback1;
  ProxyInfo results1;
  rv = resolver.GetProxyForURL(
      GURL("http://request1"), &results1, &callback1, NULL, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TestOldCompletionCallback callback2;
  ProxyInfo results2;
  rv = resolver.GetProxyForURL(
      GURL("http://request2"), &results2, &callback2, NULL, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TestOldCompletionCallback callback3;
  ProxyInfo results3;
  rv = resolver.GetProxyForURL(
      GURL("http://request3"), &results3, &callback3, NULL, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Wait for the requests to finish (they must finish in the order they were
  // started, which is what we check for from their magic return value)

  rv = callback1.WaitForResult();
  EXPECT_EQ(1, rv);
  EXPECT_EQ("PROXY request1:80", results1.ToPacString());

  rv = callback2.WaitForResult();
  EXPECT_EQ(2, rv);
  EXPECT_EQ("PROXY request2:80", results2.ToPacString());

  rv = callback3.WaitForResult();
  EXPECT_EQ(3, rv);
  EXPECT_EQ("PROXY request3:80", results3.ToPacString());

  // Ensure that PurgeMemory() reaches the wrapped resolver and happens on the
  // right thread.
  EXPECT_EQ(0, mock->purge_count());
  resolver.PurgeMemory();
  // There is no way to get a callback directly when PurgeMemory() completes, so
  // we queue up a dummy request after the PurgeMemory() call and wait until it
  // finishes to ensure PurgeMemory() has had a chance to run.
  TestOldCompletionCallback dummy_callback;
  rv = resolver.SetPacScript(ProxyResolverScriptData::FromUTF8("dummy"),
                             &dummy_callback);
  EXPECT_EQ(OK, dummy_callback.WaitForResult());
  EXPECT_EQ(1, mock->purge_count());
}

// Tests that the NetLog is updated to include the time the request was waiting
// to be scheduled to a thread.
TEST(MultiThreadedProxyResolverTest,
     SingleThread_UpdatesNetLogWithThreadWait) {
  const size_t kNumThreads = 1u;
  scoped_ptr<BlockableProxyResolver> mock(new BlockableProxyResolver);
  MultiThreadedProxyResolver resolver(
      new ForwardingProxyResolverFactory(mock.get()), kNumThreads);

  int rv;

  // Initialize the resolver.
  TestOldCompletionCallback init_callback;
  rv = resolver.SetPacScript(ProxyResolverScriptData::FromUTF8("foo"),
                             &init_callback);
  EXPECT_EQ(OK, init_callback.WaitForResult());

  // Block the proxy resolver, so no request can complete.
  mock->Block();

  // Start request 0.
  ProxyResolver::RequestHandle request0;
  TestOldCompletionCallback callback0;
  ProxyInfo results0;
  CapturingBoundNetLog log0(CapturingNetLog::kUnbounded);
  rv = resolver.GetProxyForURL(
      GURL("http://request0"), &results0, &callback0, &request0, log0.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Start 2 more requests (request1 and request2).

  TestOldCompletionCallback callback1;
  ProxyInfo results1;
  CapturingBoundNetLog log1(CapturingNetLog::kUnbounded);
  rv = resolver.GetProxyForURL(
      GURL("http://request1"), &results1, &callback1, NULL, log1.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  ProxyResolver::RequestHandle request2;
  TestOldCompletionCallback callback2;
  ProxyInfo results2;
  CapturingBoundNetLog log2(CapturingNetLog::kUnbounded);
  rv = resolver.GetProxyForURL(
      GURL("http://request2"), &results2, &callback2, &request2, log2.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Unblock the worker thread so the requests can continue running.
  mock->WaitUntilBlocked();
  mock->Unblock();

  // Check that request 0 completed as expected.
  // The NetLog has 1 entry that came from the MultiThreadedProxyResolver, and
  // 1 entry from the mock proxy resolver.
  EXPECT_EQ(0, callback0.WaitForResult());
  EXPECT_EQ("PROXY request0:80", results0.ToPacString());

  net::CapturingNetLog::EntryList entries0;
  log0.GetEntries(&entries0);

  ASSERT_EQ(2u, entries0.size());
  EXPECT_EQ(NetLog::TYPE_SUBMITTED_TO_RESOLVER_THREAD,
            entries0[0].type);

  // Check that request 1 completed as expected.
  EXPECT_EQ(1, callback1.WaitForResult());
  EXPECT_EQ("PROXY request1:80", results1.ToPacString());

  net::CapturingNetLog::EntryList entries1;
  log1.GetEntries(&entries1);

  ASSERT_EQ(4u, entries1.size());
  EXPECT_TRUE(LogContainsBeginEvent(
      entries1, 0,
      NetLog::TYPE_WAITING_FOR_PROXY_RESOLVER_THREAD));
  EXPECT_TRUE(LogContainsEndEvent(
      entries1, 1,
      NetLog::TYPE_WAITING_FOR_PROXY_RESOLVER_THREAD));

  // Check that request 2 completed as expected.
  EXPECT_EQ(2, callback2.WaitForResult());
  EXPECT_EQ("PROXY request2:80", results2.ToPacString());

  net::CapturingNetLog::EntryList entries2;
  log2.GetEntries(&entries2);

  ASSERT_EQ(4u, entries2.size());
  EXPECT_TRUE(LogContainsBeginEvent(
      entries2, 0,
      NetLog::TYPE_WAITING_FOR_PROXY_RESOLVER_THREAD));
  EXPECT_TRUE(LogContainsEndEvent(
      entries2, 1,
      NetLog::TYPE_WAITING_FOR_PROXY_RESOLVER_THREAD));
}

// Cancel a request which is in progress, and then cancel a request which
// is pending.
TEST(MultiThreadedProxyResolverTest, SingleThread_CancelRequest) {
  const size_t kNumThreads = 1u;
  scoped_ptr<BlockableProxyResolver> mock(new BlockableProxyResolver);
  MultiThreadedProxyResolver resolver(
      new ForwardingProxyResolverFactory(mock.get()),
                                      kNumThreads);

  int rv;

  // Initialize the resolver.
  TestOldCompletionCallback init_callback;
  rv = resolver.SetPacScript(ProxyResolverScriptData::FromUTF8("foo"),
                             &init_callback);
  EXPECT_EQ(OK, init_callback.WaitForResult());

  // Block the proxy resolver, so no request can complete.
  mock->Block();

  // Start request 0.
  ProxyResolver::RequestHandle request0;
  TestOldCompletionCallback callback0;
  ProxyInfo results0;
  rv = resolver.GetProxyForURL(
      GURL("http://request0"), &results0, &callback0, &request0, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Wait until requests 0 reaches the worker thread.
  mock->WaitUntilBlocked();

  // Start 3 more requests (request1 : request3).

  TestOldCompletionCallback callback1;
  ProxyInfo results1;
  rv = resolver.GetProxyForURL(
      GURL("http://request1"), &results1, &callback1, NULL, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  ProxyResolver::RequestHandle request2;
  TestOldCompletionCallback callback2;
  ProxyInfo results2;
  rv = resolver.GetProxyForURL(
      GURL("http://request2"), &results2, &callback2, &request2, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TestOldCompletionCallback callback3;
  ProxyInfo results3;
  rv = resolver.GetProxyForURL(
      GURL("http://request3"), &results3, &callback3, NULL, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Cancel request0 (inprogress) and request2 (pending).
  resolver.CancelRequest(request0);
  resolver.CancelRequest(request2);

  // Unblock the worker thread so the requests can continue running.
  mock->Unblock();

  // Wait for requests 1 and 3 to finish.

  rv = callback1.WaitForResult();
  EXPECT_EQ(1, rv);
  EXPECT_EQ("PROXY request1:80", results1.ToPacString());

  rv = callback3.WaitForResult();
  // Note that since request2 was cancelled before reaching the resolver,
  // the request count is 2 and not 3 here.
  EXPECT_EQ(2, rv);
  EXPECT_EQ("PROXY request3:80", results3.ToPacString());

  // Requests 0 and 2 which were cancelled, hence their completion callbacks
  // were never summoned.
  EXPECT_FALSE(callback0.have_result());
  EXPECT_FALSE(callback2.have_result());
}

// Test that deleting MultiThreadedProxyResolver while requests are
// outstanding cancels them (and doesn't leak anything).
TEST(MultiThreadedProxyResolverTest, SingleThread_CancelRequestByDeleting) {
  const size_t kNumThreads = 1u;
  scoped_ptr<BlockableProxyResolver> mock(new BlockableProxyResolver);
  scoped_ptr<MultiThreadedProxyResolver> resolver(
      new MultiThreadedProxyResolver(
          new ForwardingProxyResolverFactory(mock.get()), kNumThreads));

  int rv;

  // Initialize the resolver.
  TestOldCompletionCallback init_callback;
  rv = resolver->SetPacScript(ProxyResolverScriptData::FromUTF8("foo"),
                              &init_callback);
  EXPECT_EQ(OK, init_callback.WaitForResult());

  // Block the proxy resolver, so no request can complete.
  mock->Block();

  // Start 3 requests.

  TestOldCompletionCallback callback0;
  ProxyInfo results0;
  rv = resolver->GetProxyForURL(
      GURL("http://request0"), &results0, &callback0, NULL, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TestOldCompletionCallback callback1;
  ProxyInfo results1;
  rv = resolver->GetProxyForURL(
      GURL("http://request1"), &results1, &callback1, NULL, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TestOldCompletionCallback callback2;
  ProxyInfo results2;
  rv = resolver->GetProxyForURL(
      GURL("http://request2"), &results2, &callback2, NULL, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Wait until request 0 reaches the worker thread.
  mock->WaitUntilBlocked();

  // Add some latency, to improve the chance that when
  // MultiThreadedProxyResolver is deleted below we are still running inside
  // of the worker thread. The test will pass regardless, so this race doesn't
  // cause flakiness. However the destruction during execution is a more
  // interesting case to test.
  mock->SetResolveLatency(100);

  // Unblock the worker thread and delete the underlying
  // MultiThreadedProxyResolver immediately.
  mock->Unblock();
  resolver.reset();

  // Give any posted tasks a chance to run (in case there is badness).
  MessageLoop::current()->RunAllPending();

  // Check that none of the outstanding requests were completed.
  EXPECT_FALSE(callback0.have_result());
  EXPECT_FALSE(callback1.have_result());
  EXPECT_FALSE(callback2.have_result());
}

// Cancel an outstanding call to SetPacScriptByData().
TEST(MultiThreadedProxyResolverTest, SingleThread_CancelSetPacScript) {
  const size_t kNumThreads = 1u;
  scoped_ptr<BlockableProxyResolver> mock(new BlockableProxyResolver);
  MultiThreadedProxyResolver resolver(
      new ForwardingProxyResolverFactory(mock.get()), kNumThreads);

  int rv;

  TestOldCompletionCallback set_pac_script_callback;
  rv = resolver.SetPacScript(ProxyResolverScriptData::FromUTF8("data"),
                             &set_pac_script_callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Cancel the SetPacScriptByData request.
  resolver.CancelSetPacScript();

  // Start another SetPacScript request
  TestOldCompletionCallback set_pac_script_callback2;
  rv = resolver.SetPacScript(ProxyResolverScriptData::FromUTF8("data2"),
                             &set_pac_script_callback2);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Wait for the initialization to complete.

  rv = set_pac_script_callback2.WaitForResult();
  EXPECT_EQ(0, rv);
  EXPECT_EQ(ASCIIToUTF16("data2"), mock->last_script_data()->utf16());

  // The first SetPacScript callback should never have been completed.
  EXPECT_FALSE(set_pac_script_callback.have_result());
}

// Tests setting the PAC script once, lazily creating new threads, and
// cancelling requests.
TEST(MultiThreadedProxyResolverTest, ThreeThreads_Basic) {
  const size_t kNumThreads = 3u;
  BlockableProxyResolverFactory* factory = new BlockableProxyResolverFactory;
  MultiThreadedProxyResolver resolver(factory, kNumThreads);

  int rv;

  EXPECT_TRUE(resolver.expects_pac_bytes());

  // Call SetPacScriptByData() -- verify that it reaches the synchronous
  // resolver.
  TestOldCompletionCallback set_script_callback;
  rv = resolver.SetPacScript(
      ProxyResolverScriptData::FromUTF8("pac script bytes"),
      &set_script_callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, set_script_callback.WaitForResult());
  // One thread has been provisioned (i.e. one ProxyResolver was created).
  ASSERT_EQ(1u, factory->resolvers().size());
  EXPECT_EQ(ASCIIToUTF16("pac script bytes"),
            factory->resolvers()[0]->last_script_data()->utf16());

  const int kNumRequests = 9;
  TestOldCompletionCallback callback[kNumRequests];
  ProxyInfo results[kNumRequests];
  ProxyResolver::RequestHandle request[kNumRequests];

  // Start request 0 -- this should run on thread 0 as there is nothing else
  // going on right now.
  rv = resolver.GetProxyForURL(
      GURL("http://request0"), &results[0], &callback[0], &request[0],
      BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Wait for request 0 to finish.
  rv = callback[0].WaitForResult();
  EXPECT_EQ(0, rv);
  EXPECT_EQ("PROXY request0:80", results[0].ToPacString());
  ASSERT_EQ(1u, factory->resolvers().size());
  EXPECT_EQ(1, factory->resolvers()[0]->request_count());

  MessageLoop::current()->RunAllPending();

  // We now start 8 requests in parallel -- this will cause the maximum of
  // three threads to be provisioned (an additional two from what we already
  // have).

  for (int i = 1; i < kNumRequests; ++i) {
    rv = resolver.GetProxyForURL(
        GURL(base::StringPrintf("http://request%d", i)), &results[i],
        &callback[i], &request[i], BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);
  }

  // We should now have a total of 3 threads, each with its own ProxyResolver
  // that will get initialized with the same data. (We check this later since
  // the assignment happens on the worker threads and may not have occurred
  // yet.)
  ASSERT_EQ(3u, factory->resolvers().size());

  // Cancel 3 of the 8 oustanding requests.
  resolver.CancelRequest(request[1]);
  resolver.CancelRequest(request[3]);
  resolver.CancelRequest(request[6]);

  // Wait for the remaining requests to complete.
  int kNonCancelledRequests[] = {2, 4, 5, 7, 8};
  for (size_t i = 0; i < arraysize(kNonCancelledRequests); ++i) {
    int request_index = kNonCancelledRequests[i];
    EXPECT_GE(callback[request_index].WaitForResult(), 0);
  }

  // Check that the cancelled requests never invoked their callback.
  EXPECT_FALSE(callback[1].have_result());
  EXPECT_FALSE(callback[3].have_result());
  EXPECT_FALSE(callback[6].have_result());

  // We call SetPacScript again, solely to stop the current worker threads.
  // (That way we can test to see the values observed by the synchronous
  // resolvers in a non-racy manner).
  TestOldCompletionCallback set_script_callback2;
  rv = resolver.SetPacScript(ProxyResolverScriptData::FromUTF8("xyz"),
                             &set_script_callback2);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, set_script_callback2.WaitForResult());
  ASSERT_EQ(4u, factory->resolvers().size());

  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(
        ASCIIToUTF16("pac script bytes"),
        factory->resolvers()[i]->last_script_data()->utf16()) << "i=" << i;
  }

  EXPECT_EQ(ASCIIToUTF16("xyz"),
            factory->resolvers()[3]->last_script_data()->utf16());

  // We don't know the exact ordering that requests ran on threads with,
  // but we do know the total count that should have reached the threads.
  // 8 total were submitted, and three were cancelled. Of the three that
  // were cancelled, one of them (request 1) was cancelled after it had
  // already been posted to the worker thread. So the resolvers will
  // have seen 6 total (and 1 from the run prior).
  ASSERT_EQ(4u, factory->resolvers().size());
  int total_count = 0;
  for (int i = 0; i < 3; ++i) {
    total_count += factory->resolvers()[i]->request_count();
  }
  EXPECT_EQ(7, total_count);
}

// Tests using two threads. The first request hangs the first thread. Checks
// that other requests are able to complete while this first request remains
// stalled.
TEST(MultiThreadedProxyResolverTest, OneThreadBlocked) {
  const size_t kNumThreads = 2u;
  BlockableProxyResolverFactory* factory = new BlockableProxyResolverFactory;
  MultiThreadedProxyResolver resolver(factory, kNumThreads);

  int rv;

  EXPECT_TRUE(resolver.expects_pac_bytes());

  // Initialize the resolver.
  TestOldCompletionCallback set_script_callback;
  rv = resolver.SetPacScript(
      ProxyResolverScriptData::FromUTF8("pac script bytes"),
      &set_script_callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, set_script_callback.WaitForResult());
  // One thread has been provisioned (i.e. one ProxyResolver was created).
  ASSERT_EQ(1u, factory->resolvers().size());
  EXPECT_EQ(ASCIIToUTF16("pac script bytes"),
            factory->resolvers()[0]->last_script_data()->utf16());

  const int kNumRequests = 4;
  TestOldCompletionCallback callback[kNumRequests];
  ProxyInfo results[kNumRequests];
  ProxyResolver::RequestHandle request[kNumRequests];

  // Start a request that will block the first thread.

  factory->resolvers()[0]->Block();

  rv = resolver.GetProxyForURL(
      GURL("http://request0"), &results[0], &callback[0], &request[0],
      BoundNetLog());

  EXPECT_EQ(ERR_IO_PENDING, rv);
  factory->resolvers()[0]->WaitUntilBlocked();

  // Start 3 more requests -- they should all be serviced by thread #2
  // since thread #1 is blocked.

  for (int i = 1; i < kNumRequests; ++i) {
    rv = resolver.GetProxyForURL(
        GURL(base::StringPrintf("http://request%d", i)),
        &results[i], &callback[i], &request[i], BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);
  }

  // Wait for the three requests to complete (they should complete in FIFO
  // order).
  for (int i = 1; i < kNumRequests; ++i) {
    EXPECT_EQ(i - 1, callback[i].WaitForResult());
  }

  // Unblock the first thread.
  factory->resolvers()[0]->Unblock();
  EXPECT_EQ(0, callback[0].WaitForResult());

  // All in all, the first thread should have seen just 1 request. And the
  // second thread 3 requests.
  ASSERT_EQ(2u, factory->resolvers().size());
  EXPECT_EQ(1, factory->resolvers()[0]->request_count());
  EXPECT_EQ(3, factory->resolvers()[1]->request_count());
}

}  // namespace

}  // namespace net
