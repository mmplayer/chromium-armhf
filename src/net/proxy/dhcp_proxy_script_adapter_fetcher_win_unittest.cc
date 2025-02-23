// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/dhcp_proxy_script_adapter_fetcher_win.h"

#include "base/perftimer.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/timer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/proxy/mock_proxy_script_fetcher.h"
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/test/test_server.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const char* const kPacUrl = "http://pacserver/script.pac";

// In net/proxy/dhcp_proxy_script_fetcher_win_unittest.cc there are a few
// tests that exercise DhcpProxyScriptAdapterFetcher end-to-end along with
// DhcpProxyScriptFetcherWin, i.e. it tests the end-to-end usage of Win32
// APIs and the network.  In this file we test only by stubbing out
// functionality.

// Version of DhcpProxyScriptAdapterFetcher that mocks out dependencies
// to allow unit testing.
class MockDhcpProxyScriptAdapterFetcher
    : public DhcpProxyScriptAdapterFetcher {
 public:
  explicit MockDhcpProxyScriptAdapterFetcher(URLRequestContext* context)
      : DhcpProxyScriptAdapterFetcher(context),
        dhcp_delay_ms_(1),
        timeout_ms_(TestTimeouts::action_timeout_ms()),
        configured_url_(kPacUrl),
        fetcher_delay_ms_(1),
        fetcher_result_(OK),
        pac_script_("bingo") {
  }

  void Cancel() {
    DhcpProxyScriptAdapterFetcher::Cancel();
    fetcher_ = NULL;
  }

  virtual ProxyScriptFetcher* ImplCreateScriptFetcher() OVERRIDE {
    // We don't maintain ownership of the fetcher, it is transferred to
    // the caller.
    fetcher_ = new MockProxyScriptFetcher();
    if (fetcher_delay_ms_ != -1) {
      fetcher_timer_.Start(FROM_HERE,
          base::TimeDelta::FromMilliseconds(fetcher_delay_ms_),
          this, &MockDhcpProxyScriptAdapterFetcher::OnFetcherTimer);
    }
    return fetcher_;
  }

  class DelayingDhcpQuery : public DhcpQuery {
   public:
    explicit DelayingDhcpQuery()
        : DhcpQuery(),
          test_finished_event_(true, false) {
    }

    std::string ImplGetPacURLFromDhcp(
        const std::string& adapter_name) OVERRIDE {
      PerfTimer timer;
      test_finished_event_.TimedWait(dhcp_delay_);
      return configured_url_;
    }

    base::WaitableEvent test_finished_event_;
    TimeDelta dhcp_delay_;
    std::string configured_url_;
  };

  virtual DhcpQuery* ImplCreateDhcpQuery() OVERRIDE {
    dhcp_query_ = new DelayingDhcpQuery();
    dhcp_query_->dhcp_delay_ = TimeDelta::FromMilliseconds(dhcp_delay_ms_);
    dhcp_query_->configured_url_ = configured_url_;
    return dhcp_query_;
  }

  // Use a shorter timeout so tests can finish more quickly.
  virtual base::TimeDelta ImplGetTimeout() const OVERRIDE {
    return base::TimeDelta::FromMilliseconds(timeout_ms_);
  }

  void OnFetcherTimer() {
    // Note that there is an assumption by this mock implementation that
    // DhcpProxyScriptAdapterFetcher::Fetch will call ImplCreateScriptFetcher
    // and call Fetch on the fetcher before the message loop is re-entered.
    // This holds true today, but if you hit this DCHECK the problem can
    // possibly be resolved by having a separate subclass of
    // MockProxyScriptFetcher that adds the delay internally (instead of
    // the simple approach currently used in ImplCreateScriptFetcher above).
    DCHECK(fetcher_ && fetcher_->has_pending_request());
    fetcher_->NotifyFetchCompletion(fetcher_result_, pac_script_);
    fetcher_ = NULL;
  }

  bool IsWaitingForFetcher() const {
    return state() == STATE_WAIT_URL;
  }

  bool WasCancelled() const {
    return state() == STATE_CANCEL;
  }

  void FinishTest() {
    DCHECK(dhcp_query_);
    dhcp_query_->test_finished_event_.Signal();
  }

  int dhcp_delay_ms_;
  int timeout_ms_;
  std::string configured_url_;
  int fetcher_delay_ms_;
  int fetcher_result_;
  std::string pac_script_;
  MockProxyScriptFetcher* fetcher_;
  base::OneShotTimer<MockDhcpProxyScriptAdapterFetcher> fetcher_timer_;
  scoped_refptr<DelayingDhcpQuery> dhcp_query_;
};

class FetcherClient {
 public:
  FetcherClient()
      : url_request_context_(new TestURLRequestContext()),
        fetcher_(
            new MockDhcpProxyScriptAdapterFetcher(url_request_context_.get())) {
  }

  void WaitForResult(int expected_error) {
    EXPECT_EQ(expected_error, callback_.WaitForResult());
  }

  void RunTest() {
    fetcher_->Fetch("adapter name", &callback_);
  }

  void FinishTestAllowCleanup() {
    fetcher_->FinishTest();
    MessageLoop::current()->RunAllPending();
  }

  TestOldCompletionCallback callback_;
  scoped_refptr<URLRequestContext> url_request_context_;
  scoped_ptr<MockDhcpProxyScriptAdapterFetcher> fetcher_;
  string16 pac_text_;
};

TEST(DhcpProxyScriptAdapterFetcher, NormalCaseURLNotInDhcp) {
  FetcherClient client;
  client.fetcher_->configured_url_ = "";
  client.RunTest();
  client.WaitForResult(ERR_PAC_NOT_IN_DHCP);
  ASSERT_TRUE(client.fetcher_->DidFinish());
  EXPECT_EQ(ERR_PAC_NOT_IN_DHCP, client.fetcher_->GetResult());
  EXPECT_EQ(string16(L""), client.fetcher_->GetPacScript());
}

TEST(DhcpProxyScriptAdapterFetcher, NormalCaseURLInDhcp) {
  FetcherClient client;
  client.RunTest();
  client.WaitForResult(OK);
  ASSERT_TRUE(client.fetcher_->DidFinish());
  EXPECT_EQ(OK, client.fetcher_->GetResult());
  EXPECT_EQ(string16(L"bingo"), client.fetcher_->GetPacScript());
  EXPECT_EQ(GURL(kPacUrl), client.fetcher_->GetPacURL());
}

TEST(DhcpProxyScriptAdapterFetcher, TimeoutDuringDhcp) {
  // Does a Fetch() with a long enough delay on accessing DHCP that the
  // fetcher should time out.  This is to test a case manual testing found,
  // where under certain circumstances (e.g. adapter enabled for DHCP and
  // needs to retrieve its configuration from DHCP, but no DHCP server
  // present on the network) accessing DHCP can take on the order of tens
  // of seconds.
  FetcherClient client;
  client.fetcher_->dhcp_delay_ms_ = TestTimeouts::action_max_timeout_ms();
  client.fetcher_->timeout_ms_ = 25;

  PerfTimer timer;
  client.RunTest();
  // An error different from this would be received if the timeout didn't
  // kick in.
  client.WaitForResult(ERR_TIMED_OUT);

  ASSERT_TRUE(client.fetcher_->DidFinish());
  EXPECT_EQ(ERR_TIMED_OUT, client.fetcher_->GetResult());
  EXPECT_EQ(string16(L""), client.fetcher_->GetPacScript());
  EXPECT_EQ(GURL(), client.fetcher_->GetPacURL());
  client.FinishTestAllowCleanup();
}

TEST(DhcpProxyScriptAdapterFetcher, CancelWhileDhcp) {
  FetcherClient client;
  client.RunTest();
  client.fetcher_->Cancel();
  MessageLoop::current()->RunAllPending();
  ASSERT_FALSE(client.fetcher_->DidFinish());
  ASSERT_TRUE(client.fetcher_->WasCancelled());
  EXPECT_EQ(ERR_ABORTED, client.fetcher_->GetResult());
  EXPECT_EQ(string16(L""), client.fetcher_->GetPacScript());
  EXPECT_EQ(GURL(), client.fetcher_->GetPacURL());
  client.FinishTestAllowCleanup();
}

TEST(DhcpProxyScriptAdapterFetcher, CancelWhileFetcher) {
  FetcherClient client;
  // This causes the mock fetcher not to pretend the
  // fetcher finishes after a timeout.
  client.fetcher_->fetcher_delay_ms_ = -1;
  client.RunTest();
  int max_loops = 4;
  while (!client.fetcher_->IsWaitingForFetcher() && max_loops--) {
    base::PlatformThread::Sleep(10);
    MessageLoop::current()->RunAllPending();
  }
  client.fetcher_->Cancel();
  MessageLoop::current()->RunAllPending();
  ASSERT_FALSE(client.fetcher_->DidFinish());
  ASSERT_TRUE(client.fetcher_->WasCancelled());
  EXPECT_EQ(ERR_ABORTED, client.fetcher_->GetResult());
  EXPECT_EQ(string16(L""), client.fetcher_->GetPacScript());
  // GetPacURL() still returns the URL fetched in this case.
  EXPECT_EQ(GURL(kPacUrl), client.fetcher_->GetPacURL());
  client.FinishTestAllowCleanup();
}

TEST(DhcpProxyScriptAdapterFetcher, CancelAtCompletion) {
  FetcherClient client;
  client.RunTest();
  client.WaitForResult(OK);
  client.fetcher_->Cancel();
  // Canceling after you're done should have no effect, so these
  // are identical expectations to the NormalCaseURLInDhcp test.
  ASSERT_TRUE(client.fetcher_->DidFinish());
  EXPECT_EQ(OK, client.fetcher_->GetResult());
  EXPECT_EQ(string16(L"bingo"), client.fetcher_->GetPacScript());
  EXPECT_EQ(GURL(kPacUrl), client.fetcher_->GetPacURL());
  client.FinishTestAllowCleanup();
}

// Does a real fetch on a mock DHCP configuration.
class MockDhcpRealFetchProxyScriptAdapterFetcher
    : public MockDhcpProxyScriptAdapterFetcher {
 public:
  explicit MockDhcpRealFetchProxyScriptAdapterFetcher(
      URLRequestContext* context)
      : MockDhcpProxyScriptAdapterFetcher(context),
        url_request_context_(context) {
  }

  // Returns a real proxy script fetcher.
  ProxyScriptFetcher* ImplCreateScriptFetcher() OVERRIDE {
    ProxyScriptFetcher* fetcher =
        new ProxyScriptFetcherImpl(url_request_context_);
    return fetcher;
  }

  URLRequestContext* url_request_context_;
};

TEST(DhcpProxyScriptAdapterFetcher, MockDhcpRealFetch) {
  TestServer test_server(
      TestServer::TYPE_HTTP,
      FilePath(FILE_PATH_LITERAL("net/data/proxy_script_fetcher_unittest")));
  ASSERT_TRUE(test_server.Start());

  GURL configured_url = test_server.GetURL("files/downloadable.pac");

  FetcherClient client;
  scoped_refptr<URLRequestContext> url_request_context(
      new TestURLRequestContext());
  client.fetcher_.reset(
      new MockDhcpRealFetchProxyScriptAdapterFetcher(
          url_request_context.get()));
  client.fetcher_->configured_url_ = configured_url.spec();
  client.RunTest();
  client.WaitForResult(OK);
  ASSERT_TRUE(client.fetcher_->DidFinish());
  EXPECT_EQ(OK, client.fetcher_->GetResult());
  EXPECT_EQ(string16(L"-downloadable.pac-\n"), client.fetcher_->GetPacScript());
  EXPECT_EQ(configured_url,
            client.fetcher_->GetPacURL());
}

}  // namespace

}  // namespace net
