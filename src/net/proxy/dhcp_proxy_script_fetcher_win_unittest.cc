// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/dhcp_proxy_script_fetcher_win.h"

#include <vector>

#include "base/message_loop.h"
#include "base/perftimer.h"
#include "base/rand_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "net/base/completion_callback.h"
#include "net/proxy/dhcp_proxy_script_adapter_fetcher_win.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(DhcpProxyScriptFetcherWin, AdapterNamesAndPacURLFromDhcp) {
  // This tests our core Win32 implementation without any of the wrappers
  // we layer on top to achieve asynchronous and parallel operations.
  //
  // We don't make assumptions about the environment this unit test is
  // running in, so it just exercises the code to make sure there
  // is no crash and no error returned, but does not assert on the number
  // of interfaces or the information returned via DHCP.
  std::set<std::string> adapter_names;
  DhcpProxyScriptFetcherWin::GetCandidateAdapterNames(&adapter_names);
  for (std::set<std::string>::const_iterator it = adapter_names.begin();
       it != adapter_names.end();
       ++it) {
    const std::string& adapter_name = *it;
    std::string pac_url =
        DhcpProxyScriptAdapterFetcher::GetPacURLFromDhcp(adapter_name);
    printf("Adapter '%s' has PAC URL '%s' configured in DHCP.\n",
           adapter_name.c_str(),
           pac_url.c_str());
  }
}

// Helper for RealFetch* tests below.
class RealFetchTester {
 public:
  RealFetchTester()
      : context_((new TestURLRequestContext())),
        fetcher_(new DhcpProxyScriptFetcherWin(context_.get())),
        finished_(false),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            completion_callback_(this, &RealFetchTester::OnCompletion)),
        on_completion_is_error_(false) {
    // Make sure the test ends.
    timeout_.Start(FROM_HERE,
        base::TimeDelta::FromSeconds(5), this, &RealFetchTester::OnTimeout);
  }

  void RunTest() {
    int result = fetcher_->Fetch(&pac_text_, &completion_callback_);
    if (result != ERR_IO_PENDING)
      finished_ = true;
  }

  void RunTestWithCancel() {
    RunTest();
    fetcher_->Cancel();
  }

  void RunTestWithDeferredCancel() {
    // Put the cancellation into the queue before even running the
    // test to avoid the chance of one of the adapter fetcher worker
    // threads completing before cancellation.  See http://crbug.com/86756.
    cancel_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(0),
                        this, &RealFetchTester::OnCancelTimer);
    RunTest();
  }

  void OnCompletion(int result) {
    if (on_completion_is_error_) {
      FAIL() << "Received completion for test in which this is error.";
    }
    finished_ = true;
    printf("Result code %d PAC data length %d\n", result, pac_text_.size());
  }

  void OnTimeout() {
    printf("Timeout!");
    OnCompletion(0);
  }

  void OnCancelTimer() {
    fetcher_->Cancel();
    finished_ = true;
  }

  void WaitUntilDone() {
    while (!finished_) {
      MessageLoop::current()->RunAllPending();
    }
    MessageLoop::current()->RunAllPending();
  }

  // Attempts to give worker threads time to finish.  This is currently
  // very simplistic as completion (via completion callback or cancellation)
  // immediately "detaches" any worker threads, so the best we can do is give
  // them a little time.  If we start running into Valgrind leaks, we can
  // do something a bit more clever to track worker threads even when the
  // DhcpProxyScriptFetcherWin state machine has finished.
  void FinishTestAllowCleanup() {
    base::PlatformThread::Sleep(30);
  }

  scoped_refptr<URLRequestContext> context_;
  scoped_ptr<DhcpProxyScriptFetcherWin> fetcher_;
  bool finished_;
  string16 pac_text_;
  OldCompletionCallbackImpl<RealFetchTester> completion_callback_;
  base::OneShotTimer<RealFetchTester> timeout_;
  base::OneShotTimer<RealFetchTester> cancel_timer_;
  bool on_completion_is_error_;
};

TEST(DhcpProxyScriptFetcherWin, RealFetch) {
  // This tests a call to Fetch() with no stubbing out of dependencies.
  //
  // We don't make assumptions about the environment this unit test is
  // running in, so it just exercises the code to make sure there
  // is no crash and no unexpected error returned, but does not assert on
  // results beyond that.
  RealFetchTester fetcher;
  fetcher.RunTest();

  fetcher.WaitUntilDone();
  printf("PAC URL was %s\n",
      fetcher.fetcher_->GetPacURL().possibly_invalid_spec().c_str());

  fetcher.FinishTestAllowCleanup();
}

TEST(DhcpProxyScriptFetcherWin, RealFetchWithCancel) {
  // Does a Fetch() with an immediate cancel.  As before, just
  // exercises the code without stubbing out dependencies.
  RealFetchTester fetcher;
  fetcher.RunTestWithCancel();
  MessageLoop::current()->RunAllPending();

  // Attempt to avoid Valgrind leak reports in case worker thread is
  // still running.
  fetcher.FinishTestAllowCleanup();
}

// For RealFetchWithDeferredCancel, below.
class DelayingDhcpProxyScriptAdapterFetcher
    : public DhcpProxyScriptAdapterFetcher {
 public:
  explicit DelayingDhcpProxyScriptAdapterFetcher(
      URLRequestContext* url_request_context)
      : DhcpProxyScriptAdapterFetcher(url_request_context) {
  }

  class DelayingDhcpQuery : public DhcpQuery {
   public:
    explicit DelayingDhcpQuery()
        : DhcpQuery() {
    }

    std::string ImplGetPacURLFromDhcp(
        const std::string& adapter_name) OVERRIDE {
      base::PlatformThread::Sleep(20);
      return DhcpQuery::ImplGetPacURLFromDhcp(adapter_name);
    }
  };

  DhcpQuery* ImplCreateDhcpQuery() OVERRIDE {
    return new DelayingDhcpQuery();
  }
};

// For RealFetchWithDeferredCancel, below.
class DelayingDhcpProxyScriptFetcherWin
    : public DhcpProxyScriptFetcherWin {
 public:
  explicit DelayingDhcpProxyScriptFetcherWin(
      URLRequestContext* context)
      : DhcpProxyScriptFetcherWin(context) {
  }

  DhcpProxyScriptAdapterFetcher* ImplCreateAdapterFetcher() OVERRIDE {
    return new DelayingDhcpProxyScriptAdapterFetcher(url_request_context());
  }
};

TEST(DhcpProxyScriptFetcherWin, RealFetchWithDeferredCancel) {
  // Does a Fetch() with a slightly delayed cancel.  As before, just
  // exercises the code without stubbing out dependencies, but
  // introduces a guaranteed 20 ms delay on the worker threads so that
  // the cancel is called before they complete.
  RealFetchTester fetcher;
  fetcher.fetcher_.reset(
      new DelayingDhcpProxyScriptFetcherWin(fetcher.context_));
  fetcher.on_completion_is_error_ = true;
  fetcher.RunTestWithDeferredCancel();
  fetcher.WaitUntilDone();
}

// The remaining tests are to exercise our state machine in various
// situations, with actual network access fully stubbed out.

class DummyDhcpProxyScriptAdapterFetcher
    : public DhcpProxyScriptAdapterFetcher {
 public:
  DummyDhcpProxyScriptAdapterFetcher()
      : DhcpProxyScriptAdapterFetcher(new TestURLRequestContext()),
        did_finish_(false),
        result_(OK),
        pac_script_(L"bingo"),
        fetch_delay_ms_(1),
        client_callback_(NULL) {
  }

  void Fetch(const std::string& adapter_name,
             OldCompletionCallback* callback) OVERRIDE {
    client_callback_ = callback;
    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(fetch_delay_ms_),
                 this, &DummyDhcpProxyScriptAdapterFetcher::OnTimer);
  }

  void Cancel() OVERRIDE {
    timer_.Stop();
  }

  bool DidFinish() const OVERRIDE {
    return did_finish_;
  }

  int GetResult() const OVERRIDE {
    return result_;
  }

  string16 GetPacScript() const OVERRIDE {
    return pac_script_;
  }

  void OnTimer() {
    client_callback_->Run(result_);
  }

  void Configure(
      bool did_finish, int result, string16 pac_script, int fetch_delay_ms) {
    did_finish_ = did_finish;
    result_ = result;
    pac_script_ = pac_script;
    fetch_delay_ms_ = fetch_delay_ms;
  }

 private:
  bool did_finish_;
  int result_;
  string16 pac_script_;
  int fetch_delay_ms_;
  OldCompletionCallback* client_callback_;
  base::OneShotTimer<DummyDhcpProxyScriptAdapterFetcher> timer_;
};

class MockDhcpProxyScriptFetcherWin : public DhcpProxyScriptFetcherWin {
 public:
  class MockAdapterQuery : public AdapterQuery {
   public:
    MockAdapterQuery() {
    }

    virtual ~MockAdapterQuery() {
    }

    virtual bool ImplGetCandidateAdapterNames(
        std::set<std::string>* adapter_names) OVERRIDE {
      adapter_names->insert(
          mock_adapter_names_.begin(), mock_adapter_names_.end());
      return true;
    }

    std::vector<std::string> mock_adapter_names_;
  };

  MockDhcpProxyScriptFetcherWin()
      : DhcpProxyScriptFetcherWin(new TestURLRequestContext()),
        num_fetchers_created_(0),
        worker_finished_event_(true, false) {
    ResetTestState();
  }

  virtual ~MockDhcpProxyScriptFetcherWin() {
    ResetTestState();
  }

  // Adds a fetcher object to the queue of fetchers used by
  // |ImplCreateAdapterFetcher()|, and its name to the list of adapters
  // returned by ImplGetCandidateAdapterNames.
  void PushBackAdapter(const std::string& adapter_name,
                       DhcpProxyScriptAdapterFetcher* fetcher) {
    adapter_query_->mock_adapter_names_.push_back(adapter_name);
    adapter_fetchers_.push_back(fetcher);
  }

  void ConfigureAndPushBackAdapter(const std::string& adapter_name,
                                   bool did_finish,
                                   int result,
                                   string16 pac_script,
                                   int fetch_delay_ms) {
    scoped_ptr<DummyDhcpProxyScriptAdapterFetcher> adapter_fetcher(
        new DummyDhcpProxyScriptAdapterFetcher());
    adapter_fetcher->Configure(did_finish, result, pac_script, fetch_delay_ms);
    PushBackAdapter(adapter_name, adapter_fetcher.release());
  }

  DhcpProxyScriptAdapterFetcher* ImplCreateAdapterFetcher() OVERRIDE {
    ++num_fetchers_created_;
    return adapter_fetchers_[next_adapter_fetcher_index_++];
  }

  virtual AdapterQuery* ImplCreateAdapterQuery() OVERRIDE {
    DCHECK(adapter_query_);
    return adapter_query_.get();
  }

  int ImplGetMaxWaitMs() OVERRIDE {
    return max_wait_ms_;
  }

  void ImplOnGetCandidateAdapterNamesDone() OVERRIDE {
    worker_finished_event_.Signal();
  }

  void ResetTestState() {
    // Delete any adapter fetcher objects we didn't hand out.
    std::vector<DhcpProxyScriptAdapterFetcher*>::const_iterator it
        = adapter_fetchers_.begin();
    for (; it != adapter_fetchers_.end(); ++it) {
      if (num_fetchers_created_-- <= 0) {
        delete (*it);
      }
    }

    next_adapter_fetcher_index_ = 0;
    num_fetchers_created_ = 0;
    adapter_fetchers_.clear();
    adapter_query_ = new MockAdapterQuery();
    max_wait_ms_ = TestTimeouts::tiny_timeout_ms();
  }

  bool HasPendingFetchers() {
    return num_pending_fetchers() > 0;
  }

  int next_adapter_fetcher_index_;

  // Ownership gets transferred to the implementation class via
  // ImplCreateAdapterFetcher, but any objects not handed out are
  // deleted on destruction.
  std::vector<DhcpProxyScriptAdapterFetcher*> adapter_fetchers_;

  scoped_refptr<MockAdapterQuery> adapter_query_;

  int max_wait_ms_;
  int num_fetchers_created_;
  base::WaitableEvent worker_finished_event_;
};

class FetcherClient {
public:
  FetcherClient()
      : finished_(false),
        result_(ERR_UNEXPECTED),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            completion_callback_(this, &FetcherClient::OnCompletion)) {
  }

  void RunTest() {
    int result = fetcher_.Fetch(&pac_text_, &completion_callback_);
    ASSERT_EQ(ERR_IO_PENDING, result);
  }

  void RunMessageLoopUntilComplete() {
    while (!finished_) {
      MessageLoop::current()->RunAllPending();
    }
    MessageLoop::current()->RunAllPending();
  }

  void RunMessageLoopUntilWorkerDone() {
    DCHECK(fetcher_.adapter_query_.get());
    while (!fetcher_.worker_finished_event_.TimedWait(
        base::TimeDelta::FromMilliseconds(10))) {
      MessageLoop::current()->RunAllPending();
    }
  }

  void OnCompletion(int result) {
    finished_ = true;
    result_ = result;
  }

  void ResetTestState() {
    finished_ = false;
    result_ = ERR_UNEXPECTED;
    pac_text_ = L"";
    fetcher_.ResetTestState();
  }

  MockDhcpProxyScriptFetcherWin fetcher_;
  bool finished_;
  int result_;
  string16 pac_text_;
  OldCompletionCallbackImpl<FetcherClient> completion_callback_;
};

// We separate out each test's logic so that we can easily implement
// the ReuseFetcher test at the bottom.
void TestNormalCaseURLConfiguredOneAdapter(FetcherClient* client) {
  scoped_ptr<DummyDhcpProxyScriptAdapterFetcher> adapter_fetcher(
      new DummyDhcpProxyScriptAdapterFetcher());
  adapter_fetcher->Configure(true, OK, L"bingo", 1);
  client->fetcher_.PushBackAdapter("a", adapter_fetcher.release());
  client->RunTest();
  client->RunMessageLoopUntilComplete();
  ASSERT_EQ(OK, client->result_);
  ASSERT_EQ(L"bingo", client->pac_text_);
}

TEST(DhcpProxyScriptFetcherWin, NormalCaseURLConfiguredOneAdapter) {
  FetcherClient client;
  TestNormalCaseURLConfiguredOneAdapter(&client);
}

void TestNormalCaseURLConfiguredMultipleAdapters(FetcherClient* client) {
  client->fetcher_.ConfigureAndPushBackAdapter(
      "most_preferred", true, ERR_PAC_NOT_IN_DHCP, L"", 1);
  client->fetcher_.ConfigureAndPushBackAdapter(
      "second", true, OK, L"bingo", 50);
  client->fetcher_.ConfigureAndPushBackAdapter(
      "third", true, OK, L"rocko", 1);
  client->RunTest();
  client->RunMessageLoopUntilComplete();
  ASSERT_EQ(OK, client->result_);
  ASSERT_EQ(L"bingo", client->pac_text_);
}

TEST(DhcpProxyScriptFetcherWin, NormalCaseURLConfiguredMultipleAdapters) {
  FetcherClient client;
  TestNormalCaseURLConfiguredMultipleAdapters(&client);
}

void TestNormalCaseURLConfiguredMultipleAdaptersWithTimeout(
    FetcherClient* client) {
  client->fetcher_.ConfigureAndPushBackAdapter(
      "most_preferred", true, ERR_PAC_NOT_IN_DHCP, L"", 1);
  // This will time out.
  client->fetcher_.ConfigureAndPushBackAdapter(
      "second", false, ERR_IO_PENDING, L"bingo",
      TestTimeouts::action_timeout_ms());
  client->fetcher_.ConfigureAndPushBackAdapter(
      "third", true, OK, L"rocko", 1);
  client->RunTest();
  client->RunMessageLoopUntilComplete();
  ASSERT_EQ(OK, client->result_);
  ASSERT_EQ(L"rocko", client->pac_text_);
}

TEST(DhcpProxyScriptFetcherWin,
     NormalCaseURLConfiguredMultipleAdaptersWithTimeout) {
  FetcherClient client;
  TestNormalCaseURLConfiguredMultipleAdaptersWithTimeout(&client);
}

void TestFailureCaseURLConfiguredMultipleAdaptersWithTimeout(
    FetcherClient* client) {
  client->fetcher_.ConfigureAndPushBackAdapter(
      "most_preferred", true, ERR_PAC_NOT_IN_DHCP, L"", 1);
  // This will time out.
  client->fetcher_.ConfigureAndPushBackAdapter(
      "second", false, ERR_IO_PENDING, L"bingo",
      TestTimeouts::action_timeout_ms());
  // This is the first non-ERR_PAC_NOT_IN_DHCP error and as such
  // should be chosen.
  client->fetcher_.ConfigureAndPushBackAdapter(
      "third", true, ERR_PAC_STATUS_NOT_OK, L"", 1);
  client->fetcher_.ConfigureAndPushBackAdapter(
      "fourth", true, ERR_NOT_IMPLEMENTED, L"", 1);
  client->RunTest();
  client->RunMessageLoopUntilComplete();
  ASSERT_EQ(ERR_PAC_STATUS_NOT_OK, client->result_);
  ASSERT_EQ(L"", client->pac_text_);
}

TEST(DhcpProxyScriptFetcherWin,
     FailureCaseURLConfiguredMultipleAdaptersWithTimeout) {
  FetcherClient client;
  TestFailureCaseURLConfiguredMultipleAdaptersWithTimeout(&client);
}

void TestFailureCaseNoURLConfigured(FetcherClient* client) {
  client->fetcher_.ConfigureAndPushBackAdapter(
      "most_preferred", true, ERR_PAC_NOT_IN_DHCP, L"", 1);
  // This will time out.
  client->fetcher_.ConfigureAndPushBackAdapter(
      "second", false, ERR_IO_PENDING, L"bingo",
      TestTimeouts::action_timeout_ms());
  // This is the first non-ERR_PAC_NOT_IN_DHCP error and as such
  // should be chosen.
  client->fetcher_.ConfigureAndPushBackAdapter(
      "third", true, ERR_PAC_NOT_IN_DHCP, L"", 1);
  client->RunTest();
  client->RunMessageLoopUntilComplete();
  ASSERT_EQ(ERR_PAC_NOT_IN_DHCP, client->result_);
  ASSERT_EQ(L"", client->pac_text_);
}

TEST(DhcpProxyScriptFetcherWin, FailureCaseNoURLConfigured) {
  FetcherClient client;
  TestFailureCaseNoURLConfigured(&client);
}

void TestFailureCaseNoDhcpAdapters(FetcherClient* client) {
  client->RunTest();
  client->RunMessageLoopUntilComplete();
  ASSERT_EQ(ERR_PAC_NOT_IN_DHCP, client->result_);
  ASSERT_EQ(L"", client->pac_text_);
  ASSERT_EQ(0, client->fetcher_.num_fetchers_created_);
}

TEST(DhcpProxyScriptFetcherWin, FailureCaseNoDhcpAdapters) {
  FetcherClient client;
  TestFailureCaseNoDhcpAdapters(&client);
}

void TestShortCircuitLessPreferredAdapters(FetcherClient* client) {
  // Here we have a bunch of adapters; the first reports no PAC in DHCP,
  // the second responds quickly with a PAC file, the rest take a long
  // time.  Verify that we complete quickly and do not wait for the slow
  // adapters, i.e. we finish before timeout.
  client->fetcher_.ConfigureAndPushBackAdapter(
      "1", true, ERR_PAC_NOT_IN_DHCP, L"", 1);
  client->fetcher_.ConfigureAndPushBackAdapter(
      "2", true, OK, L"bingo", 1);
  client->fetcher_.ConfigureAndPushBackAdapter(
      "3", true, OK, L"wrongo", TestTimeouts::action_max_timeout_ms());

  // Increase the timeout to ensure the short circuit mechanism has
  // time to kick in before the timeout waiting for more adapters kicks in.
  client->fetcher_.max_wait_ms_ = TestTimeouts::action_timeout_ms();

  PerfTimer timer;
  client->RunTest();
  client->RunMessageLoopUntilComplete();
  ASSERT_TRUE(client->fetcher_.HasPendingFetchers());
  // Assert that the time passed is definitely less than the wait timer
  // timeout, to get a second signal that it was the shortcut mechanism
  // (in OnFetcherDone) that kicked in, and not the timeout waiting for
  // more adapters.
  ASSERT_GT(base::TimeDelta::FromMilliseconds(
      client->fetcher_.max_wait_ms_ - (client->fetcher_.max_wait_ms_ / 10)),
      timer.Elapsed());
}

TEST(DhcpProxyScriptFetcherWin, ShortCircuitLessPreferredAdapters) {
  FetcherClient client;
  TestShortCircuitLessPreferredAdapters(&client);
}

void TestImmediateCancel(FetcherClient* client) {
  scoped_ptr<DummyDhcpProxyScriptAdapterFetcher> adapter_fetcher(
      new DummyDhcpProxyScriptAdapterFetcher());
  adapter_fetcher->Configure(true, OK, L"bingo", 1);
  client->fetcher_.PushBackAdapter("a", adapter_fetcher.release());
  client->RunTest();
  client->fetcher_.Cancel();
  client->RunMessageLoopUntilWorkerDone();
  ASSERT_EQ(0, client->fetcher_.num_fetchers_created_);
}

// Regression test to check that when we cancel immediately, no
// adapter fetchers get created.
TEST(DhcpProxyScriptFetcherWin, ImmediateCancel) {
  FetcherClient client;
  TestImmediateCancel(&client);
}

TEST(DhcpProxyScriptFetcherWin, ReuseFetcher) {
  FetcherClient client;

  // The ProxyScriptFetcher interface stipulates that only a single
  // |Fetch()| may be in flight at once, but allows reuse, so test
  // that the state transitions correctly from done to start in all
  // cases we're testing.

  typedef void (*FetcherClientTestFunction)(FetcherClient*);
  typedef std::vector<FetcherClientTestFunction> TestVector;
  TestVector test_functions;
  test_functions.push_back(TestNormalCaseURLConfiguredOneAdapter);
  test_functions.push_back(TestNormalCaseURLConfiguredMultipleAdapters);
  test_functions.push_back(
      TestNormalCaseURLConfiguredMultipleAdaptersWithTimeout);
  test_functions.push_back(
      TestFailureCaseURLConfiguredMultipleAdaptersWithTimeout);
  test_functions.push_back(TestFailureCaseNoURLConfigured);
  test_functions.push_back(TestFailureCaseNoDhcpAdapters);
  test_functions.push_back(TestShortCircuitLessPreferredAdapters);
  test_functions.push_back(TestImmediateCancel);

  std::random_shuffle(test_functions.begin(),
                      test_functions.end(),
                      base::RandGenerator);
  for (TestVector::const_iterator it = test_functions.begin();
       it != test_functions.end();
       ++it) {
    (*it)(&client);
    client.ResetTestState();
  }

  // Re-do the first test to make sure the last test that was run did
  // not leave things in a bad state.
  (*test_functions.begin())(&client);
}

}  // namespace

}  // namespace net
