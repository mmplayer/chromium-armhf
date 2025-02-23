// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/stl_util.h"
#include "base/threading/thread.h"
#include "base/synchronization/waitable_event.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_host.h"
#include "webkit/appcache/appcache_response.h"
#include "webkit/appcache/appcache_update_job.h"
#include "webkit/appcache/mock_appcache_service.h"

namespace appcache {
class AppCacheUpdateJobTest;

namespace {

const char kManifest1Contents[] =
    "CACHE MANIFEST\n"
    "explicit1\n"
    "FALLBACK:\n"
    "fallback1 fallback1a\n"
    "NETWORK:\n"
    "*\n";

// There are a handful of http accessible resources that we need to conduct
// these tests. Instead of running a seperate server to host these resources,
// we mock them up.
class MockHttpServer {
 public:
  static GURL GetMockUrl(const std::string& path) {
    return GURL("http://mockhost/" + path);
  }

  static GURL GetMockHttpsUrl(const std::string& path) {
    return GURL("https://mockhost/" + path);
  }

  static GURL GetMockCrossOriginHttpsUrl(const std::string& path) {
    return GURL("https://cross_origin_host/" + path);
  }

  static net::URLRequestJob* JobFactory(net::URLRequest* request) {
    if (request->url().host() != "mockhost" &&
        request->url().host() != "cross_origin_host")
      return new net::URLRequestErrorJob(request, -100);

    std::string headers, body;
    GetMockResponse(request->url().path(), &headers, &body);
    return new net::URLRequestTestJob(request, headers, body, true);
  }

 private:
  static void GetMockResponse(const std::string& path,
                              std::string* headers,
                              std::string* body) {
    const char ok_headers[] =
        "HTTP/1.1 200 OK\0"
        "\0";
    const char error_headers[] =
        "HTTP/1.1 500 BOO HOO\0"
        "\0";
    const char manifest_headers[] =
        "HTTP/1.1 200 OK\0"
        "Content-type: text/cache-manifest\0"
        "\0";
    const char not_modified_headers[] =
        "HTTP/1.1 304 NOT MODIFIED\0"
        "\0";
    const char gone_headers[] =
        "HTTP/1.1 410 GONE\0"
        "\0";
    const char not_found_headers[] =
        "HTTP/1.1 404 NOT FOUND\0"
        "\0";
    const char no_store_headers[] =
        "HTTP/1.1 200 OK\0"
        "Cache-Control: no-store\0"
        "\0";

    if (path == "/files/wrong-mime-manifest") {
      (*headers) = std::string(ok_headers, arraysize(ok_headers));
      (*body) = "CACHE MANIFEST\n";
    } else if (path == "/files/bad-manifest") {
      (*headers) = std::string(manifest_headers, arraysize(manifest_headers));
      (*body) = "BAD CACHE MANIFEST";
    } else if (path == "/files/empty1") {
      (*headers) = std::string(ok_headers, arraysize(ok_headers));
      (*body) = "";
    } else if (path == "/files/empty-file-manifest") {
      (*headers) = std::string(manifest_headers, arraysize(manifest_headers));
      (*body) = "CACHE MANIFEST\n"
                "empty1\n";
    } else if (path == "/files/empty-manifest") {
      (*headers) = std::string(manifest_headers, arraysize(manifest_headers));
      (*body) = "CACHE MANIFEST\n";
    } else if (path == "/files/explicit1") {
      (*headers) = std::string(ok_headers, arraysize(ok_headers));
      (*body) = "explicit1";
    } else if (path == "/files/explicit2") {
      (*headers) = std::string(ok_headers, arraysize(ok_headers));
      (*body) = "explicit2";
    } else if (path == "/files/fallback1a") {
      (*headers) = std::string(ok_headers, arraysize(ok_headers));
      (*body) = "fallback1a";
    } else if (path == "/files/gone") {
      (*headers) = std::string(gone_headers, arraysize(gone_headers));
      (*body) = "";
    } else if (path == "/files/manifest1") {
      (*headers) = std::string(manifest_headers, arraysize(manifest_headers));
      (*body) = kManifest1Contents;
    } else if (path == "/files/manifest1-with-notmodified") {
      (*headers) = std::string(manifest_headers, arraysize(manifest_headers));
      (*body) = kManifest1Contents;
      (*body).append("CACHE:\n"
                     "notmodified\n");
    } else if (path == "/files/manifest-fb-404") {
      (*headers) = std::string(manifest_headers, arraysize(manifest_headers));
      (*body) = "CACHE MANIFEST\n"
                "explicit1\n"
                "FALLBACK:\n"
                "fallback1 fallback1a\n"
                "fallback404 fallback-404\n"
                "NETWORK:\n"
                "online1\n";
    } else if (path == "/files/manifest-merged-types") {
      (*headers) = std::string(manifest_headers, arraysize(manifest_headers));
      (*body) = "CACHE MANIFEST\n"
                "explicit1\n"
                "# manifest is also an explicit entry\n"
                "manifest-merged-types\n"
                "FALLBACK:\n"
                "# fallback is also explicit entry\n"
                "fallback1 explicit1\n"
                "NETWORK:\n"
                "online1\n";
    } else if (path == "/files/manifest-with-404") {
      (*headers) = std::string(manifest_headers, arraysize(manifest_headers));
      (*body) = "CACHE MANIFEST\n"
                "explicit-404\n"
                "explicit1\n"
                "explicit2\n"
                "explicit3\n"
                "FALLBACK:\n"
                "fallback1 fallback1a\n"
                "NETWORK:\n"
                "online1\n";
    } else if (path == "/files/notmodified") {
      (*headers) = std::string(not_modified_headers,
                               arraysize(not_modified_headers));
      (*body) = "";
    } else if (path == "/files/servererror") {
      (*headers) = std::string(error_headers,
                               arraysize(error_headers));
      (*body) = "error";
    } else if (path == "/files/valid_cross_origin_https_manifest") {
      (*headers) = std::string(manifest_headers, arraysize(manifest_headers));
      (*body) = "CACHE MANIFEST\n"
                "https://cross_origin_host/files/explicit1\n";
    } else if (path == "/files/invalid_cross_origin_https_manifest") {
      (*headers) = std::string(manifest_headers, arraysize(manifest_headers));
      (*body) = "CACHE MANIFEST\n"
                "https://cross_origin_host/files/no-store-headers\n";
    } else if (path == "/files/no-store-headers") {
      (*headers) = std::string(no_store_headers, arraysize(no_store_headers));
      (*body) = "no-store";
    } else {
      (*headers) = std::string(not_found_headers,
                               arraysize(not_found_headers));
      (*body) = "";
    }
  }
};

class MockHttpServerJobFactory
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  virtual net::URLRequestJob* MaybeCreateJob(net::URLRequest* request) const {
    return MockHttpServer::JobFactory(request);
  }
};

}  // namespace

class MockFrontend : public AppCacheFrontend {
 public:
  MockFrontend()
      : ignore_progress_events_(false), verify_progress_events_(false),
        last_progress_total_(-1), last_progress_complete_(-1),
        start_update_trigger_(CHECKING_EVENT), update_(NULL) {
  }

  virtual void OnCacheSelected(
      int host_id, const appcache::AppCacheInfo& info) {
  }

  virtual void OnStatusChanged(const std::vector<int>& host_ids,
                               Status status) {
  }

  virtual void OnEventRaised(const std::vector<int>& host_ids,
                             EventID event_id) {
    raised_events_.push_back(RaisedEvent(host_ids, event_id));

    // Trigger additional updates if requested.
    if (event_id == start_update_trigger_ && update_) {
      for (std::vector<AppCacheHost*>::iterator it = update_hosts_.begin();
           it != update_hosts_.end(); ++it) {
        AppCacheHost* host = *it;
        update_->StartUpdate(host,
            (host ? host->pending_master_entry_url() : GURL()));
      }
      update_hosts_.clear();  // only trigger once
    }
  }

  virtual void OnErrorEventRaised(const std::vector<int>& host_ids,
                                  const std::string& message) {
    error_message_ = message;
    OnEventRaised(host_ids, ERROR_EVENT);
  }

  virtual void OnProgressEventRaised(const std::vector<int>& host_ids,
                                     const GURL& url,
                                     int num_total, int num_complete) {
    if (!ignore_progress_events_)
      OnEventRaised(host_ids, PROGRESS_EVENT);

    if (verify_progress_events_) {
      EXPECT_GE(num_total, num_complete);
      EXPECT_GE(num_complete, 0);

      if (last_progress_total_ == -1) {
        // Should start at zero.
        EXPECT_EQ(0, num_complete);
      } else {
        // Total should be stable and complete should bump up by one at a time.
        EXPECT_EQ(last_progress_total_, num_total);
        EXPECT_EQ(last_progress_complete_ + 1, num_complete);
      }

      // Url should be valid for all except the 'final' event.
      if (num_total == num_complete)
        EXPECT_TRUE(url.is_empty());
      else
        EXPECT_TRUE(url.is_valid());

      last_progress_total_ = num_total;
      last_progress_complete_ = num_complete;
    }
  }

  virtual void OnLogMessage(int host_id, appcache::LogLevel log_level,
                            const std::string& message) {
  }

  virtual void OnContentBlocked(int host_id, const GURL& manifest_url) {
  }

  void AddExpectedEvent(const std::vector<int>& host_ids, EventID event_id) {
    DCHECK(!ignore_progress_events_ || event_id != PROGRESS_EVENT);
    expected_events_.push_back(RaisedEvent(host_ids, event_id));
  }

  void SetIgnoreProgressEvents(bool ignore) {
    // Some tests involve joining new hosts to an already running update job
    // or intentionally failing. The timing and sequencing of the progress
    // events generated by an update job are dependent on the behavior of
    // an external HTTP server. For jobs that do not run fully till completion,
    // due to either joining late or early exit, we skip monitoring the
    // progress events to avoid flakiness.
    ignore_progress_events_ = ignore;
  }

  void SetVerifyProgressEvents(bool verify) {
    verify_progress_events_ = verify;
  }

  void TriggerAdditionalUpdates(EventID trigger_event,
                                AppCacheUpdateJob* update) {
    start_update_trigger_ = trigger_event;
    update_ = update;
  }

  void AdditionalUpdateHost(AppCacheHost* host) {
    update_hosts_.push_back(host);
  }

  typedef std::vector<int> HostIds;
  typedef std::pair<HostIds, EventID> RaisedEvent;
  typedef std::vector<RaisedEvent> RaisedEvents;
  RaisedEvents raised_events_;
  std::string error_message_;

  // Set the expected events if verification needs to happen asynchronously.
  RaisedEvents expected_events_;
  std::string expected_error_message_;

  bool ignore_progress_events_;

  bool verify_progress_events_;
  int last_progress_total_;
  int last_progress_complete_;

  // Add ability for frontend to add master entries to an inprogress update.
  EventID start_update_trigger_;
  AppCacheUpdateJob* update_;
  std::vector<AppCacheHost*> update_hosts_;
};

// Helper factories to simulate redirected URL responses for tests.
class RedirectFactory : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  virtual net::URLRequestJob* MaybeCreateJob(net::URLRequest* request) const {
    return new net::URLRequestTestJob(
        request,
        net::URLRequestTestJob::test_redirect_headers(),
        net::URLRequestTestJob::test_data_1(),
        true);
  }
};

// Helper class to simulate a URL that returns retry or success.
class RetryRequestTestJob : public net::URLRequestTestJob {
 public:
  enum RetryHeader {
    NO_RETRY_AFTER,
    NONZERO_RETRY_AFTER,
    RETRY_AFTER_0,
  };

  static const GURL kRetryUrl;

  // Call this at the start of each retry test.
  static void Initialize(int num_retry_responses, RetryHeader header,
      int expected_requests) {
    num_requests_ = 0;
    num_retries_ = num_retry_responses;
    retry_after_ = header;
    expected_requests_ = expected_requests;
  }

  // Verifies results at end of test and resets counters.
  static void Verify() {
    EXPECT_EQ(expected_requests_, num_requests_);
    num_requests_ = 0;
    expected_requests_ = 0;
  }

  static net::URLRequestJob* RetryFactory(net::URLRequest* request) {
    ++num_requests_;
    if (num_retries_ > 0 && request->original_url() == kRetryUrl) {
      --num_retries_;
      return new RetryRequestTestJob(
          request, RetryRequestTestJob::retry_headers(), 503);
    } else {
      return new RetryRequestTestJob(
          request, RetryRequestTestJob::manifest_headers(), 200);
    }
  }

  virtual int GetResponseCode() const { return response_code_; }

 private:
  ~RetryRequestTestJob() {}

  static std::string retry_headers() {
    const char no_retry_after[] =
        "HTTP/1.1 503 BOO HOO\0"
        "\0";
    const char nonzero[] =
        "HTTP/1.1 503 BOO HOO\0"
        "Retry-After: 60\0"
        "\0";
    const char retry_after_0[] =
        "HTTP/1.1 503 BOO HOO\0"
        "Retry-After: 0\0"
        "\0";

    switch (retry_after_) {
      case NO_RETRY_AFTER:
        return std::string(no_retry_after, arraysize(no_retry_after));
      case NONZERO_RETRY_AFTER:
        return std::string(nonzero, arraysize(nonzero));
      case RETRY_AFTER_0:
      default:
        return std::string(retry_after_0, arraysize(retry_after_0));
    }
  }

  static std::string manifest_headers() {
    const char headers[] =
        "HTTP/1.1 200 OK\0"
        "Content-type: text/cache-manifest\0"
        "\0";
    return std::string(headers, arraysize(headers));
  }

  static std::string data() {
    return std::string("CACHE MANIFEST\r"
        "http://retry\r");  // must be same as kRetryUrl
  }

  RetryRequestTestJob(net::URLRequest* request,
                      const std::string& headers,
                      int response_code)
      : net::URLRequestTestJob(request, headers, data(), true),
        response_code_(response_code) {
  }

  int response_code_;

  static int num_requests_;
  static int num_retries_;
  static RetryHeader retry_after_;
  static int expected_requests_;
};

class RetryRequestTestJobFactory
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  virtual net::URLRequestJob* MaybeCreateJob(net::URLRequest* request) const {
    return RetryRequestTestJob::RetryFactory(request);
  }
};

// static
const GURL RetryRequestTestJob::kRetryUrl("http://retry");
int RetryRequestTestJob::num_requests_ = 0;
int RetryRequestTestJob::num_retries_;
RetryRequestTestJob::RetryHeader RetryRequestTestJob::retry_after_;
int RetryRequestTestJob::expected_requests_ = 0;

// Helper class to check for certain HTTP headers.
class HttpHeadersRequestTestJob : public net::URLRequestTestJob {
 public:
  // Call this at the start of each HTTP header-related test.
  static void Initialize(const std::string& expect_if_modified_since,
                         const std::string& expect_if_none_match) {
    expect_if_modified_since_ = expect_if_modified_since;
    expect_if_none_match_ = expect_if_none_match;
  }

  // Verifies results at end of test and resets class.
  static void Verify() {
    if (!expect_if_modified_since_.empty())
      EXPECT_TRUE(saw_if_modified_since_);
    if (!expect_if_none_match_.empty())
      EXPECT_TRUE(saw_if_none_match_);

    // Reset.
    expect_if_modified_since_.clear();
    saw_if_modified_since_ = false;
    expect_if_none_match_.clear();
    saw_if_none_match_ = false;
    already_checked_ = false;
  }

  static net::URLRequestJob* IfModifiedSinceFactory(net::URLRequest* request) {
    if (!already_checked_) {
      already_checked_ = true;  // only check once for a test
      const net::HttpRequestHeaders& extra_headers =
          request->extra_request_headers();
      std::string header_value;
      saw_if_modified_since_ =
          extra_headers.GetHeader(
              net::HttpRequestHeaders::kIfModifiedSince, &header_value) &&
          header_value == expect_if_modified_since_;

      saw_if_none_match_ =
          extra_headers.GetHeader(
              net::HttpRequestHeaders::kIfNoneMatch, &header_value) &&
          header_value == expect_if_none_match_;
    }
    return MockHttpServer::JobFactory(request);
  }

 private:
  static std::string expect_if_modified_since_;
  static bool saw_if_modified_since_;
  static std::string expect_if_none_match_;
  static bool saw_if_none_match_;
  static bool already_checked_;
};

// static
std::string HttpHeadersRequestTestJob::expect_if_modified_since_;
bool HttpHeadersRequestTestJob::saw_if_modified_since_ = false;
std::string HttpHeadersRequestTestJob::expect_if_none_match_;
bool HttpHeadersRequestTestJob::saw_if_none_match_ = false;
bool HttpHeadersRequestTestJob::already_checked_ = false;

class IfModifiedSinceJobFactory
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  virtual net::URLRequestJob* MaybeCreateJob(net::URLRequest* request) const {
    return HttpHeadersRequestTestJob::IfModifiedSinceFactory(request);
  }
};

class IOThread : public base::Thread {
 public:
  explicit IOThread(const char* name)
      : base::Thread(name) {
  }

  ~IOThread() {
    // We cannot rely on our base class to stop the thread since we want our
    // CleanUp function to run.
    Stop();
  }

  const scoped_refptr<net::URLRequestContext>& request_context() {
    return request_context_;
  }

  void SetNewJobFactory(net::URLRequestJobFactory* job_factory) {
    DCHECK(job_factory);
    job_factory_.reset(job_factory);
    request_context_->set_job_factory(job_factory_.get());
  }

  virtual void Init() {
    job_factory_.reset(new net::URLRequestJobFactory);
    job_factory_->SetProtocolHandler("http", new MockHttpServerJobFactory);
    job_factory_->SetProtocolHandler("https", new MockHttpServerJobFactory);
    request_context_ = new TestURLRequestContext();
    request_context_->set_job_factory(job_factory_.get());
  }

  virtual void CleanUp() {
    request_context_ = NULL;
    job_factory_.reset();
  }

 private:
  scoped_ptr<net::URLRequestJobFactory> job_factory_;
  scoped_refptr<net::URLRequestContext> request_context_;
};

class AppCacheUpdateJobTest : public testing::Test,
                              public AppCacheGroup::UpdateObserver {
 public:
  AppCacheUpdateJobTest()
      : do_checks_after_update_finished_(false),
        expect_group_obsolete_(false),
        expect_group_has_cache_(false),
        expect_old_cache_(NULL),
        expect_newest_cache_(NULL),
        expect_non_null_update_time_(false),
        tested_manifest_(NONE),
        tested_manifest_path_override_(NULL) {
    io_thread_.reset(new IOThread("AppCacheUpdateJob IO test thread"));
    base::Thread::Options options(MessageLoop::TYPE_IO, 0);
    io_thread_->StartWithOptions(options);
  }

  // Use a separate IO thread to run a test. Thread will be destroyed
  // when it goes out of scope.
  template <class Method>
  void RunTestOnIOThread(Method method) {
    event_.reset(new base::WaitableEvent(false, false));
    io_thread_->message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this, method));

    // Wait until task is done before exiting the test.
    event_->Wait();
  }

  void StartCacheAttemptTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(), GURL("http://failme"),
                               service_->storage()->NewGroupId());

    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend mock_frontend;
    AppCacheHost host(1, &mock_frontend, service_.get());

    update->StartUpdate(&host, GURL());

    // Verify state.
    EXPECT_EQ(AppCacheUpdateJob::CACHE_ATTEMPT, update->update_type_);
    EXPECT_EQ(AppCacheUpdateJob::FETCH_MANIFEST, update->internal_state_);
    EXPECT_EQ(AppCacheGroup::CHECKING, group_->update_status());

    // Verify notifications.
    MockFrontend::RaisedEvents& events = mock_frontend.raised_events_;
    size_t expected = 1;
    EXPECT_EQ(expected, events.size());
    EXPECT_EQ(expected, events[0].first.size());
    EXPECT_EQ(host.host_id(), events[0].first[0]);
    EXPECT_EQ(CHECKING_EVENT, events[0].second);

    // Abort as we're not testing actual URL fetches in this test.
    delete update;
    UpdateFinished();
  }

  void StartUpgradeAttemptTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    {
      MakeService();
      group_ = new AppCacheGroup(service_.get(), GURL("http://failme"),
                                 service_->storage()->NewGroupId());

      // Give the group some existing caches.
      AppCache* cache1 = MakeCacheForGroup(1, 111);
      AppCache* cache2 = MakeCacheForGroup(2, 222);

      // Associate some hosts with caches in the group.
      MockFrontend mock_frontend1;
      MockFrontend mock_frontend2;
      MockFrontend mock_frontend3;

      AppCacheHost host1(1, &mock_frontend1, service_.get());
      host1.AssociateCache(cache1);

      AppCacheHost host2(2, &mock_frontend2, service_.get());
      host2.AssociateCache(cache2);

      AppCacheHost host3(3, &mock_frontend1, service_.get());
      host3.AssociateCache(cache1);

      AppCacheHost host4(4, &mock_frontend3, service_.get());

      AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
      group_->update_job_ = update;
      update->StartUpdate(&host4, GURL());

      // Verify state after starting an update.
      EXPECT_EQ(AppCacheUpdateJob::UPGRADE_ATTEMPT, update->update_type_);
      EXPECT_EQ(AppCacheUpdateJob::FETCH_MANIFEST, update->internal_state_);
      EXPECT_EQ(AppCacheGroup::CHECKING, group_->update_status());

      // Verify notifications.
      MockFrontend::RaisedEvents& events = mock_frontend1.raised_events_;
      size_t expected = 1;
      EXPECT_EQ(expected, events.size());
      expected = 2;  // 2 hosts using frontend1
      EXPECT_EQ(expected, events[0].first.size());
      MockFrontend::HostIds& host_ids = events[0].first;
      EXPECT_TRUE(std::find(host_ids.begin(), host_ids.end(), host1.host_id())
          != host_ids.end());
      EXPECT_TRUE(std::find(host_ids.begin(), host_ids.end(), host3.host_id())
          != host_ids.end());
      EXPECT_EQ(CHECKING_EVENT, events[0].second);

      events = mock_frontend2.raised_events_;
      expected = 1;
      EXPECT_EQ(expected, events.size());
      EXPECT_EQ(expected, events[0].first.size());  // 1 host using frontend2
      EXPECT_EQ(host2.host_id(), events[0].first[0]);
      EXPECT_EQ(CHECKING_EVENT, events[0].second);

      events = mock_frontend3.raised_events_;
      EXPECT_TRUE(events.empty());

      // Abort as we're not testing actual URL fetches in this test.
      delete update;
    }
    UpdateFinished();
  }

  void CacheAttemptFetchManifestFailTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(), GURL("http://failme"),
                               service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    update->manifest_fetcher_->request()->SimulateError(-100);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeFetchManifestFailTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(), GURL("http://failme"),
                               service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1, 111);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    update->StartUpdate(NULL, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    update->manifest_fetcher_->request()->SimulateError(-100);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffected by update
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, ERROR_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void ManifestRedirectTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    net::URLRequestJobFactory* new_factory(new net::URLRequestJobFactory);
    new_factory->SetProtocolHandler("http", new RedirectFactory);
    io_thread_->SetNewJobFactory(new_factory);

    MakeService();
    group_ = new AppCacheGroup(service_.get(), GURL("http://testme"),
                               service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // redirect is like a failed request
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void ManifestWrongMimeTypeTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/wrong-mime-manifest"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // bad mime type is like a failed request
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void ManifestNotFoundTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/nosuchfile"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1, 111);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    update->StartUpdate(NULL, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = true;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffected by update
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, OBSOLETE_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, OBSOLETE_EVENT);

    WaitForUpdateToFinish();
  }

  void ManifestGoneTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/gone"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void CacheAttemptNotModifiedTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/notmodified"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // treated like cache failure
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeNotModifiedTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/notmodified"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1, 111);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    update->StartUpdate(NULL, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffected by update
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, NO_UPDATE_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, NO_UPDATE_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeManifestDataUnchangedTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    // Create response writer to get a response id.
    response_writer_.reset(
        service_->storage()->CreateResponseWriter(group_->manifest_url()));

    AppCache* cache = MakeCacheForGroup(1, response_writer_->response_id());
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffected by update
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, NO_UPDATE_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, NO_UPDATE_EVENT);

    // Seed storage with expected manifest data.
    const std::string seed_data(kManifest1Contents);
    scoped_refptr<net::StringIOBuffer> io_buffer(
        new net::StringIOBuffer(seed_data));
    write_callback_.reset(
        new net::OldCompletionCallbackImpl<AppCacheUpdateJobTest>(this,
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData));
    response_writer_->WriteData(io_buffer, seed_data.length(),
                                write_callback_.get());

    // Start update after data write completes asynchronously.
  }

  void StartUpdateAfterSeedingStorageData(int result) {
    ASSERT_GT(result, 0);
    write_callback_.reset();
    response_writer_.reset();

    AppCacheUpdateJob* update = group_->update_job_;
    update->StartUpdate(NULL, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    WaitForUpdateToFinish();
  }

  void BasicCacheAttemptSuccessTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    GURL manifest_url = MockHttpServer::GetMockUrl("files/manifest1");

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), manifest_url,
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    tested_manifest_ = MANIFEST1;
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void BasicUpgradeSuccessTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    // Create a response writer to get a response id.
    response_writer_.reset(
        service_->storage()->CreateResponseWriter(group_->manifest_url()));

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(),
                                        response_writer_->response_id());
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);
    frontend1->SetVerifyProgressEvents(true);
    frontend2->SetVerifyProgressEvents(true);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(ids1, UPDATE_READY_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);  // final
    frontend2->AddExpectedEvent(ids2, UPDATE_READY_EVENT);

    // Seed storage with expected manifest data different from manifest1.
    const std::string seed_data("different");
    scoped_refptr<net::StringIOBuffer> io_buffer(
        new net::StringIOBuffer(seed_data));
    write_callback_.reset(
        new net::OldCompletionCallbackImpl<AppCacheUpdateJobTest>(this,
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData));
    response_writer_->WriteData(io_buffer, seed_data.length(),
                                write_callback_.get());

    // Start update after data write completes asynchronously.
  }

  void UpgradeLoadFromNewestCacheTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 42);
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    host->AssociateCache(cache);

    // Give the newest cache an entry that is in storage.
    response_writer_.reset(
        service_->storage()->CreateResponseWriter(group_->manifest_url()));
    cache->AddEntry(MockHttpServer::GetMockUrl("files/explicit1"),
                    AppCacheEntry(AppCacheEntry::EXPLICIT,
                                  response_writer_->response_id()));

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    expect_response_ids_.insert(
        std::map<GURL, int64>::value_type(
            MockHttpServer::GetMockUrl("files/explicit1"),
            response_writer_->response_id()));
    tested_manifest_ = MANIFEST1;
    MockFrontend::HostIds ids(1, host->host_id());
    frontend->AddExpectedEvent(ids, CHECKING_EVENT);
    frontend->AddExpectedEvent(ids, DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(ids, PROGRESS_EVENT);
    frontend->AddExpectedEvent(ids, PROGRESS_EVENT);
    frontend->AddExpectedEvent(ids, PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(ids, UPDATE_READY_EVENT);

    // Seed storage with expected http response info for entry. Allow reuse.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "Cache-Control: max-age=8675309\0"
        "\0";
    net::HttpResponseHeaders* headers =
        new net::HttpResponseHeaders(std::string(data, arraysize(data)));
    net::HttpResponseInfo* response_info = new net::HttpResponseInfo();
    response_info->request_time = base::Time::Now();
    response_info->response_time = base::Time::Now();
    response_info->headers = headers;  // adds ref to headers
    scoped_refptr<HttpResponseInfoIOBuffer> io_buffer(
        new HttpResponseInfoIOBuffer(response_info));  // adds ref to info
    write_callback_.reset(
        new net::OldCompletionCallbackImpl<AppCacheUpdateJobTest>(this,
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData));
    response_writer_->WriteInfo(io_buffer, write_callback_.get());

    // Start update after data write completes asynchronously.
  }

  void UpgradeNoLoadFromNewestCacheTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 42);
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    host->AssociateCache(cache);

    // Give the newest cache an entry that is in storage.
    response_writer_.reset(
        service_->storage()->CreateResponseWriter(group_->manifest_url()));
    cache->AddEntry(MockHttpServer::GetMockUrl("files/explicit1"),
                    AppCacheEntry(AppCacheEntry::EXPLICIT,
                                  response_writer_->response_id()));

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    MockFrontend::HostIds ids(1, host->host_id());
    frontend->AddExpectedEvent(ids, CHECKING_EVENT);
    frontend->AddExpectedEvent(ids, DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(ids, PROGRESS_EVENT);
    frontend->AddExpectedEvent(ids, PROGRESS_EVENT);
    frontend->AddExpectedEvent(ids, PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(ids, UPDATE_READY_EVENT);

    // Seed storage with expected http response info for entry. Do NOT
    // allow reuse by setting an expires header in the past.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "Expires: Thu, 01 Dec 1994 16:00:00 GMT\0"
        "\0";
    net::HttpResponseHeaders* headers =
        new net::HttpResponseHeaders(std::string(data, arraysize(data)));
    net::HttpResponseInfo* response_info = new net::HttpResponseInfo();
    response_info->request_time = base::Time::Now();
    response_info->response_time = base::Time::Now();
    response_info->headers = headers;  // adds ref to headers
    scoped_refptr<HttpResponseInfoIOBuffer> io_buffer(
        new HttpResponseInfoIOBuffer(response_info));  // adds ref to info
    write_callback_.reset(
        new net::OldCompletionCallbackImpl<AppCacheUpdateJobTest>(this,
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData));
    response_writer_->WriteInfo(io_buffer, write_callback_.get());

    // Start update after data write completes asynchronously.
  }

  void UpgradeLoadFromNewestCacheVaryHeaderTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 42);
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    host->AssociateCache(cache);

    // Give the newest cache an entry that is in storage.
    response_writer_.reset(
        service_->storage()->CreateResponseWriter(group_->manifest_url()));
    cache->AddEntry(MockHttpServer::GetMockUrl("files/explicit1"),
                    AppCacheEntry(AppCacheEntry::EXPLICIT,
                                  response_writer_->response_id()));

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    MockFrontend::HostIds ids(1, host->host_id());
    frontend->AddExpectedEvent(ids, CHECKING_EVENT);
    frontend->AddExpectedEvent(ids, DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(ids, PROGRESS_EVENT);
    frontend->AddExpectedEvent(ids, PROGRESS_EVENT);
    frontend->AddExpectedEvent(ids, PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(ids, UPDATE_READY_EVENT);

    // Seed storage with expected http response info for entry: a vary header.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "Cache-Control: max-age=8675309\0"
        "Vary: blah\0"
        "\0";
    net::HttpResponseHeaders* headers =
        new net::HttpResponseHeaders(std::string(data, arraysize(data)));
    net::HttpResponseInfo* response_info = new net::HttpResponseInfo();
    response_info->request_time = base::Time::Now();
    response_info->response_time = base::Time::Now();
    response_info->headers = headers;  // adds ref to headers
    scoped_refptr<HttpResponseInfoIOBuffer> io_buffer(
        new HttpResponseInfoIOBuffer(response_info));  // adds ref to info
    write_callback_.reset(
        new net::OldCompletionCallbackImpl<AppCacheUpdateJobTest>(this,
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData));
    response_writer_->WriteInfo(io_buffer, write_callback_.get());

    // Start update after data write completes asynchronously.
  }

  void UpgradeSuccessMergedTypesTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(),
        MockHttpServer::GetMockUrl("files/manifest-merged-types"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 42);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    // Give the newest cache a master entry that is also one of the explicit
    // entries in the manifest.
    cache->AddEntry(MockHttpServer::GetMockUrl("files/explicit1"),
                    AppCacheEntry(AppCacheEntry::MASTER, 111));

    update->StartUpdate(NULL, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST_MERGED_TYPES;
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // explicit1
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // manifest
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(ids1, UPDATE_READY_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);  // final
    frontend2->AddExpectedEvent(ids2, UPDATE_READY_EVENT);

    WaitForUpdateToFinish();
  }

  void CacheAttemptFailUrlFetchTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(),
        MockHttpServer::GetMockUrl("files/manifest-with-404"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // 404 explicit url is cache failure
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeFailUrlFetchTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(),
        MockHttpServer::GetMockUrl("files/manifest-fb-404"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 99);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    frontend1->SetIgnoreProgressEvents(true);
    frontend2->SetIgnoreProgressEvents(true);
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    update->StartUpdate(NULL, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffectd by failed update
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, ERROR_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeFailMasterUrlFetchTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    tested_manifest_path_override_ = "files/manifest1-with-notmodified";

    MakeService();
    const GURL kManifestUrl =
        MockHttpServer::GetMockUrl(tested_manifest_path_override_);
    group_ = new AppCacheGroup(
        service_.get(), kManifestUrl,
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 25);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    // Give the newest cache some existing entries; one will fail with a 404.
    cache->AddEntry(
        MockHttpServer::GetMockUrl("files/notfound"),
        AppCacheEntry(AppCacheEntry::MASTER, 222));
    cache->AddEntry(
        MockHttpServer::GetMockUrl("files/explicit2"),
        AppCacheEntry(AppCacheEntry::MASTER | AppCacheEntry::FOREIGN, 333));
    cache->AddEntry(
        MockHttpServer::GetMockUrl("files/servererror"),
        AppCacheEntry(AppCacheEntry::MASTER, 444));
    cache->AddEntry(
        MockHttpServer::GetMockUrl("files/notmodified"),
        AppCacheEntry(AppCacheEntry::EXPLICIT, 555));

    // Seed the response_info working set with canned data for
    // files/servererror and for files/notmodified to test that the
    // existing entries for those resource are reused by the update job.
    const char kData[] =
        "HTTP/1.1 200 OK\0"
        "Last-Modified: Sat, 29 Oct 1994 19:43:31 GMT\0"
        "\0";
    const std::string kRawHeaders(kData, arraysize(kData));
    MakeAppCacheResponseInfo(kManifestUrl, 444, kRawHeaders);
    MakeAppCacheResponseInfo(kManifestUrl, 555, kRawHeaders);

    update->StartUpdate(NULL, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        MockHttpServer::GetMockUrl("files/explicit2"),
        AppCacheEntry(AppCacheEntry::MASTER)));  // foreign flag is dropped
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        MockHttpServer::GetMockUrl("files/servererror"),
        AppCacheEntry(AppCacheEntry::MASTER)));
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        MockHttpServer::GetMockUrl("files/notmodified"),
        AppCacheEntry(AppCacheEntry::EXPLICIT)));
    expect_response_ids_.insert(std::map<GURL, int64>::value_type(
        MockHttpServer::GetMockUrl("files/servererror"), 444));  // copied
    expect_response_ids_.insert(std::map<GURL, int64>::value_type(
        MockHttpServer::GetMockUrl("files/notmodified"), 555));  // copied
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // explicit1
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // fallback1a
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // notfound
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // explicit2
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // servererror
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // notmodified
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(ids1, UPDATE_READY_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);  // explicit1
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);  // fallback1a
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);  // notfound
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);  // explicit2
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);  // servererror
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);  // notmodified
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);  // final
    frontend2->AddExpectedEvent(ids2, UPDATE_READY_EVENT);

    WaitForUpdateToFinish();
  }

  void EmptyManifestTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/empty-manifest"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 33);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    frontend1->SetVerifyProgressEvents(true);

    update->StartUpdate(NULL, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = EMPTY_MANIFEST;
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(ids1, UPDATE_READY_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);  // final
    frontend2->AddExpectedEvent(ids2, UPDATE_READY_EVENT);

    WaitForUpdateToFinish();
  }

  void EmptyFileTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(),
        MockHttpServer::GetMockUrl("files/empty-file-manifest"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 22);
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    host->AssociateCache(cache);
    frontend->SetVerifyProgressEvents(true);

    update->StartUpdate(host, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    tested_manifest_ = EMPTY_FILE_MANIFEST;
    MockFrontend::HostIds ids1(1, host->host_id());
    frontend->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend->AddExpectedEvent(ids1, PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(ids1, UPDATE_READY_EVENT);

    WaitForUpdateToFinish();
  }

  void RetryRequestTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    // Set some large number of times to return retry.
    // Expect 1 manifest fetch and 3 retries.
    RetryRequestTestJob::Initialize(5, RetryRequestTestJob::RETRY_AFTER_0, 4);
    net::URLRequestJobFactory* new_factory(new net::URLRequestJobFactory);
    new_factory->SetProtocolHandler("http", new RetryRequestTestJobFactory);
    io_thread_->SetNewJobFactory(new_factory);

    MakeService();
    group_ = new AppCacheGroup(service_.get(), RetryRequestTestJob::kRetryUrl,
                               service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void RetryNoRetryAfterTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    // Set some large number of times to return retry.
    // Expect 1 manifest fetch and 0 retries.
    RetryRequestTestJob::Initialize(5, RetryRequestTestJob::NO_RETRY_AFTER, 1);
    net::URLRequestJobFactory* new_factory(new net::URLRequestJobFactory);
    new_factory->SetProtocolHandler("http", new RetryRequestTestJobFactory);
    io_thread_->SetNewJobFactory(new_factory);

    MakeService();
    group_ = new AppCacheGroup(service_.get(), RetryRequestTestJob::kRetryUrl,
                               service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void RetryNonzeroRetryAfterTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    // Set some large number of times to return retry.
    // Expect 1 request and 0 retry attempts.
    RetryRequestTestJob::Initialize(
        5, RetryRequestTestJob::NONZERO_RETRY_AFTER, 1);
    net::URLRequestJobFactory* new_factory(new net::URLRequestJobFactory);
    new_factory->SetProtocolHandler("http", new RetryRequestTestJobFactory);
    io_thread_->SetNewJobFactory(new_factory);

    MakeService();
    group_ = new AppCacheGroup(service_.get(), RetryRequestTestJob::kRetryUrl,
                               service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void RetrySuccessTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    // Set 2 as the retry limit (does not exceed the max).
    // Expect 1 manifest fetch, 2 retries, 1 url fetch, 1 manifest refetch.
    RetryRequestTestJob::Initialize(2, RetryRequestTestJob::RETRY_AFTER_0, 5);
    net::URLRequestJobFactory* new_factory(new net::URLRequestJobFactory);
    new_factory->SetProtocolHandler("http", new RetryRequestTestJobFactory);
    io_thread_->SetNewJobFactory(new_factory);

    MakeService();
    group_ = new AppCacheGroup(service_.get(), RetryRequestTestJob::kRetryUrl,
                               service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void RetryUrlTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    // Set 1 as the retry limit (does not exceed the max).
    // Expect 1 manifest fetch, 1 url fetch, 1 url retry, 1 manifest refetch.
    RetryRequestTestJob::Initialize(1, RetryRequestTestJob::RETRY_AFTER_0, 4);
    net::URLRequestJobFactory* new_factory(new net::URLRequestJobFactory);
    new_factory->SetProtocolHandler("http", new RetryRequestTestJobFactory);
    io_thread_->SetNewJobFactory(new_factory);

    MakeService();
    group_ = new AppCacheGroup(service_.get(), GURL("http://retryurl"),
                               service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void FailStoreNewestCacheTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    MockAppCacheStorage* storage =
        reinterpret_cast<MockAppCacheStorage*>(service_->storage());
    storage->SimulateStoreGroupAndNewestCacheFailure();

    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // storage failed
    frontend->AddExpectedEvent(MockFrontend::HostIds(1, host->host_id()),
                               CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeFailStoreNewestCacheTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    MockAppCacheStorage* storage =
        reinterpret_cast<MockAppCacheStorage*>(service_->storage());
    storage->SimulateStoreGroupAndNewestCacheFailure();

    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 11);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    update->StartUpdate(NULL, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // unchanged
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, ERROR_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryFailStoreNewestCacheTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    MockAppCacheStorage* storage =
        reinterpret_cast<MockAppCacheStorage*>(service_->storage());
    storage->SimulateStoreGroupAndNewestCacheFailure();

    const GURL kManifestUrl = MockHttpServer::GetMockUrl("files/notmodified");
    const int64 kManifestResponseId = 11;

    // Seed the response_info working set with canned data for
    // files/servererror and for files/notmodified to test that the
    // existing entries for those resource are reused by the update job.
    const char kData[] =
        "HTTP/1.1 200 OK\0"
        "Content-type: text/cache-manifest\0"
        "Last-Modified: Sat, 29 Oct 1994 19:43:31 GMT\0"
        "\0";
    const std::string kRawHeaders(kData, arraysize(kData));
    MakeAppCacheResponseInfo(kManifestUrl, kManifestResponseId, kRawHeaders);

    group_ = new AppCacheGroup(
        service_.get(), kManifestUrl,
        service_->storage()->NewGroupId());
    scoped_refptr<AppCache> cache(
        MakeCacheForGroup(service_->storage()->NewCacheId(),
                          kManifestResponseId));

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    host->first_party_url_ = kManifestUrl;
    host->SelectCache(MockHttpServer::GetMockUrl("files/empty1"),
                      kNoCacheId, kManifestUrl);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    tested_manifest_ = EMPTY_MANIFEST;
    tested_manifest_path_override_ = "files/notmodified";
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // unchanged
    MockFrontend::HostIds ids1(1, host->host_id());
    frontend->AddExpectedEvent(ids1, ERROR_EVENT);
    frontend->expected_error_message_ =
        "Failed to commit new cache to storage";

    WaitForUpdateToFinish();
  }

  void UpgradeFailMakeGroupObsoleteTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    MockAppCacheStorage* storage =
        reinterpret_cast<MockAppCacheStorage*>(service_->storage());
    storage->SimulateMakeGroupObsoleteFailure();

    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/nosuchfile"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1, 111);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host1->AssociateCache(cache);
    host2->AssociateCache(cache);

    update->StartUpdate(NULL, GURL());
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffected by update
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, ERROR_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryFetchManifestFailTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(), GURL("http://failme"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    host->new_master_entry_url_ = GURL("http://failme/blah");
    update->StartUpdate(host, host->new_master_entry_url_);
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    update->manifest_fetcher_->request()->SimulateError(-100);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    MockFrontend::HostIds ids1(1, host->host_id());
    frontend->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend->AddExpectedEvent(ids1, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryBadManifestTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(),
        MockHttpServer::GetMockUrl("files/bad-manifest"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    host->new_master_entry_url_ = MockHttpServer::GetMockUrl("files/blah");
    update->StartUpdate(host, host->new_master_entry_url_);
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    MockFrontend::HostIds ids1(1, host->host_id());
    frontend->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend->AddExpectedEvent(ids1, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryManifestNotFoundTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/nosuchfile"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    host->new_master_entry_url_ = MockHttpServer::GetMockUrl("files/blah");

    update->StartUpdate(host, host->new_master_entry_url_);
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    MockFrontend::HostIds ids1(1, host->host_id());
    frontend->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend->AddExpectedEvent(ids1, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryFailUrlFetchTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(),
        MockHttpServer::GetMockUrl("files/manifest-fb-404"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    frontend->SetIgnoreProgressEvents(true);
    AppCacheHost* host = MakeHost(1, frontend);
    host->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit1");

    update->StartUpdate(host, host->new_master_entry_url_);
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // 404 fallback url is cache failure
    MockFrontend::HostIds ids1(1, host->host_id());
    frontend->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(ids1, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryAllFailTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/manifest1"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend1 = MakeMockFrontend();
    frontend1->SetIgnoreProgressEvents(true);
    AppCacheHost* host1 = MakeHost(1, frontend1);
    host1->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/nosuchfile");
    update->StartUpdate(host1, host1->new_master_entry_url_);

    MockFrontend* frontend2 = MakeMockFrontend();
    frontend2->SetIgnoreProgressEvents(true);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host2->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/servererror");
    update->StartUpdate(host2, host2->new_master_entry_url_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // all pending masters failed
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, ERROR_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeMasterEntryAllFailTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/manifest1"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 42);
    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    host1->AssociateCache(cache);

    MockFrontend* frontend2 = MakeMockFrontend();
    frontend2->SetIgnoreProgressEvents(true);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host2->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/nosuchfile");
    update->StartUpdate(host2, host2->new_master_entry_url_);

    MockFrontend* frontend3 = MakeMockFrontend();
    frontend3->SetIgnoreProgressEvents(true);
    AppCacheHost* host3 = MakeHost(3, frontend3);
    host3->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/servererror");
    update->StartUpdate(host3, host3->new_master_entry_url_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(ids1, UPDATE_READY_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, ERROR_EVENT);
    MockFrontend::HostIds ids3(1, host3->host_id());
    frontend3->AddExpectedEvent(ids3, CHECKING_EVENT);
    frontend3->AddExpectedEvent(ids3, DOWNLOADING_EVENT);
    frontend3->AddExpectedEvent(ids3, ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntrySomeFailTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/manifest1"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend1 = MakeMockFrontend();
    frontend1->SetIgnoreProgressEvents(true);
    AppCacheHost* host1 = MakeHost(1, frontend1);
    host1->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/nosuchfile");
    update->StartUpdate(host1, host1->new_master_entry_url_);

    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host2->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit2");
    update->StartUpdate(host2, host2->new_master_entry_url_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;  // as long as one pending master succeeds
    tested_manifest_ = MANIFEST1;
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        MockHttpServer::GetMockUrl("files/explicit2"),
        AppCacheEntry(AppCacheEntry::MASTER)));
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, ERROR_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);  // final
    frontend2->AddExpectedEvent(ids2, CACHED_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeMasterEntrySomeFailTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/manifest1"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 42);
    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    host1->AssociateCache(cache);

    MockFrontend* frontend2 = MakeMockFrontend();
    frontend2->SetIgnoreProgressEvents(true);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host2->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/nosuchfile");
    update->StartUpdate(host2, host2->new_master_entry_url_);

    MockFrontend* frontend3 = MakeMockFrontend();
    AppCacheHost* host3 = MakeHost(3, frontend3);
    host3->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit2");
    update->StartUpdate(host3, host3->new_master_entry_url_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        MockHttpServer::GetMockUrl("files/explicit2"),
        AppCacheEntry(AppCacheEntry::MASTER)));
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(ids1, UPDATE_READY_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, ERROR_EVENT);
    MockFrontend::HostIds ids3(1, host3->host_id());
    frontend3->AddExpectedEvent(ids3, CHECKING_EVENT);
    frontend3->AddExpectedEvent(ids3, DOWNLOADING_EVENT);
    frontend3->AddExpectedEvent(ids3, PROGRESS_EVENT);
    frontend3->AddExpectedEvent(ids3, PROGRESS_EVENT);
    frontend3->AddExpectedEvent(ids3, PROGRESS_EVENT);  // final
    frontend3->AddExpectedEvent(ids3, UPDATE_READY_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryNoUpdateTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(service_.get(),
        MockHttpServer::GetMockUrl("files/notmodified"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1, 111);
    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    host1->AssociateCache(cache);

    // Give cache an existing entry that can also be fetched.
    cache->AddEntry(MockHttpServer::GetMockUrl("files/explicit2"),
                    AppCacheEntry(AppCacheEntry::EXPLICIT, 222));

    // Reset the update time to null so we can verify it gets
    // modified in this test case by the UpdateJob.
    cache->set_update_time(base::Time());

    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host2->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit1");
    update->StartUpdate(host2, host2->new_master_entry_url_);

    AppCacheHost* host3 = MakeHost(3, frontend2);  // same frontend as host2
    host3->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit2");
    update->StartUpdate(host3, host3->new_master_entry_url_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache still the same cache
    expect_non_null_update_time_ = true;
    tested_manifest_ = PENDING_MASTER_NO_UPDATE;
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, NO_UPDATE_EVENT);
    MockFrontend::HostIds ids3(1, host3->host_id());
    frontend2->AddExpectedEvent(ids3, CHECKING_EVENT);
    MockFrontend::HostIds ids2and3;
    ids2and3.push_back(host2->host_id());
    ids2and3.push_back(host3->host_id());
    frontend2->AddExpectedEvent(ids2and3, NO_UPDATE_EVENT);

    WaitForUpdateToFinish();
  }

  void StartUpdateMidCacheAttemptTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    host1->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit2");
    update->StartUpdate(host1, host1->new_master_entry_url_);
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up additional updates to be started while update is in progress.
    MockFrontend* frontend2 = MakeMockFrontend();
    frontend2->SetIgnoreProgressEvents(true);
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host2->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/nosuchfile");

    MockFrontend* frontend3 = MakeMockFrontend();
    AppCacheHost* host3 = MakeHost(3, frontend3);
    host3->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit1");

    MockFrontend* frontend4 = MakeMockFrontend();
    AppCacheHost* host4 = MakeHost(4, frontend4);
    host4->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit2");

    MockFrontend* frontend5 = MakeMockFrontend();
    AppCacheHost* host5 = MakeHost(5, frontend5);  // no master entry url

    frontend1->TriggerAdditionalUpdates(DOWNLOADING_EVENT, update);
    frontend1->AdditionalUpdateHost(host2);  // fetch will fail
    frontend1->AdditionalUpdateHost(host3);  // same as an explicit entry
    frontend1->AdditionalUpdateHost(host4);  // same as another master entry
    frontend1->AdditionalUpdateHost(NULL);   // no host
    frontend1->AdditionalUpdateHost(host5);  // no master entry url

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    tested_manifest_ = MANIFEST1;
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        MockHttpServer::GetMockUrl("files/explicit2"),
        AppCacheEntry(AppCacheEntry::MASTER)));
    MockFrontend::HostIds ids1(1, host1->host_id());
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(ids1, CACHED_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, ERROR_EVENT);
    MockFrontend::HostIds ids3(1, host3->host_id());
    frontend3->AddExpectedEvent(ids3, CHECKING_EVENT);
    frontend3->AddExpectedEvent(ids3, DOWNLOADING_EVENT);
    frontend3->AddExpectedEvent(ids3, PROGRESS_EVENT);
    frontend3->AddExpectedEvent(ids3, PROGRESS_EVENT);
    frontend3->AddExpectedEvent(ids3, PROGRESS_EVENT);  // final
    frontend3->AddExpectedEvent(ids3, CACHED_EVENT);
    MockFrontend::HostIds ids4(1, host4->host_id());
    frontend4->AddExpectedEvent(ids4, CHECKING_EVENT);
    frontend4->AddExpectedEvent(ids4, DOWNLOADING_EVENT);
    frontend4->AddExpectedEvent(ids4, PROGRESS_EVENT);
    frontend4->AddExpectedEvent(ids4, PROGRESS_EVENT);
    frontend4->AddExpectedEvent(ids4, PROGRESS_EVENT);  // final
    frontend4->AddExpectedEvent(ids4, CACHED_EVENT);

    // Host 5 is not associated with cache so no progress/cached events.
    MockFrontend::HostIds ids5(1, host5->host_id());
    frontend5->AddExpectedEvent(ids5, CHECKING_EVENT);
    frontend5->AddExpectedEvent(ids5, DOWNLOADING_EVENT);

    WaitForUpdateToFinish();
  }

  void StartUpdateMidNoUpdateTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/notmodified"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1, 111);
    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    host1->AssociateCache(cache);

    // Give cache an existing entry.
    cache->AddEntry(MockHttpServer::GetMockUrl("files/explicit2"),
                    AppCacheEntry(AppCacheEntry::EXPLICIT, 222));

    // Start update with a pending master entry that will fail to give us an
    // event to trigger other updates.
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host2->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/nosuchfile");
    update->StartUpdate(host2, host2->new_master_entry_url_);
    EXPECT_TRUE(update->manifest_fetcher_ != NULL);

    // Set up additional updates to be started while update is in progress.
    MockFrontend* frontend3 = MakeMockFrontend();
    AppCacheHost* host3 = MakeHost(3, frontend3);
    host3->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit1");

    MockFrontend* frontend4 = MakeMockFrontend();
    AppCacheHost* host4 = MakeHost(4, frontend4);  // no master entry url

    MockFrontend* frontend5 = MakeMockFrontend();
    AppCacheHost* host5 = MakeHost(5, frontend5);
    host5->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit2");  // existing entry

    MockFrontend* frontend6 = MakeMockFrontend();
    AppCacheHost* host6 = MakeHost(6, frontend6);
    host6->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit1");

    frontend2->TriggerAdditionalUpdates(ERROR_EVENT, update);
    frontend2->AdditionalUpdateHost(host3);
    frontend2->AdditionalUpdateHost(NULL);   // no host
    frontend2->AdditionalUpdateHost(host4);  // no master entry url
    frontend2->AdditionalUpdateHost(host5);  // same as existing cache entry
    frontend2->AdditionalUpdateHost(host6);  // same as another master entry

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffected by update
    tested_manifest_ = PENDING_MASTER_NO_UPDATE;
    MockFrontend::HostIds ids1(1, host1->host_id());  // prior associated host
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, NO_UPDATE_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, ERROR_EVENT);
    MockFrontend::HostIds ids3(1, host3->host_id());
    frontend3->AddExpectedEvent(ids3, CHECKING_EVENT);
    frontend3->AddExpectedEvent(ids3, NO_UPDATE_EVENT);
    MockFrontend::HostIds ids4(1, host4->host_id());  // unassociated w/cache
    frontend4->AddExpectedEvent(ids4, CHECKING_EVENT);
    MockFrontend::HostIds ids5(1, host5->host_id());
    frontend5->AddExpectedEvent(ids5, CHECKING_EVENT);
    frontend5->AddExpectedEvent(ids5, NO_UPDATE_EVENT);
    MockFrontend::HostIds ids6(1, host6->host_id());
    frontend6->AddExpectedEvent(ids6, CHECKING_EVENT);
    frontend6->AddExpectedEvent(ids6, NO_UPDATE_EVENT);

    WaitForUpdateToFinish();
  }

  void StartUpdateMidDownloadTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/manifest1"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 42);
    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(1, frontend1);
    host1->AssociateCache(cache);

    update->StartUpdate(NULL, GURL());

    // Set up additional updates to be started while update is in progress.
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host2 = MakeHost(2, frontend2);
    host2->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit1");

    MockFrontend* frontend3 = MakeMockFrontend();
    AppCacheHost* host3 = MakeHost(3, frontend3);
    host3->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit2");

    MockFrontend* frontend4 = MakeMockFrontend();
    AppCacheHost* host4 = MakeHost(4, frontend4);  // no master entry url

    MockFrontend* frontend5 = MakeMockFrontend();
    AppCacheHost* host5 = MakeHost(5, frontend5);
    host5->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit2");

    frontend1->TriggerAdditionalUpdates(PROGRESS_EVENT, update);
    frontend1->AdditionalUpdateHost(host2);  // same as entry in manifest
    frontend1->AdditionalUpdateHost(NULL);   // no host
    frontend1->AdditionalUpdateHost(host3);  // new master entry
    frontend1->AdditionalUpdateHost(host4);  // no master entry url
    frontend1->AdditionalUpdateHost(host5);  // same as another master entry

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    tested_manifest_ = MANIFEST1;
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        MockHttpServer::GetMockUrl("files/explicit2"),
        AppCacheEntry(AppCacheEntry::MASTER)));
    MockFrontend::HostIds ids1(1, host1->host_id());  // prior associated host
    frontend1->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend1->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend1->AddExpectedEvent(ids1, PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(ids1, UPDATE_READY_EVENT);
    MockFrontend::HostIds ids2(1, host2->host_id());
    frontend2->AddExpectedEvent(ids2, CHECKING_EVENT);
    frontend2->AddExpectedEvent(ids2, DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);
    frontend2->AddExpectedEvent(ids2, PROGRESS_EVENT);  // final
    frontend2->AddExpectedEvent(ids2, UPDATE_READY_EVENT);
    MockFrontend::HostIds ids3(1, host3->host_id());
    frontend3->AddExpectedEvent(ids3, CHECKING_EVENT);
    frontend3->AddExpectedEvent(ids3, DOWNLOADING_EVENT);
    frontend3->AddExpectedEvent(ids3, PROGRESS_EVENT);
    frontend3->AddExpectedEvent(ids3, PROGRESS_EVENT);  // final
    frontend3->AddExpectedEvent(ids3, UPDATE_READY_EVENT);
    MockFrontend::HostIds ids4(1, host4->host_id());  // unassociated w/cache
    frontend4->AddExpectedEvent(ids4, CHECKING_EVENT);
    frontend4->AddExpectedEvent(ids4, DOWNLOADING_EVENT);
    MockFrontend::HostIds ids5(1, host5->host_id());
    frontend5->AddExpectedEvent(ids5, CHECKING_EVENT);
    frontend5->AddExpectedEvent(ids5, DOWNLOADING_EVENT);
    frontend5->AddExpectedEvent(ids5, PROGRESS_EVENT);
    frontend5->AddExpectedEvent(ids5, PROGRESS_EVENT);  // final
    frontend5->AddExpectedEvent(ids5, UPDATE_READY_EVENT);

    WaitForUpdateToFinish();
  }

  void QueueMasterEntryTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/manifest1"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    // Pretend update job has been running and is about to terminate.
    group_->update_status_ = AppCacheGroup::DOWNLOADING;
    update->internal_state_ = AppCacheUpdateJob::REFETCH_MANIFEST;
    EXPECT_TRUE(update->IsTerminating());

    // Start an update. Should be queued.
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    host->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit2");
    update->StartUpdate(host, host->new_master_entry_url_);
    EXPECT_TRUE(update->pending_master_entries_.empty());
    EXPECT_FALSE(group_->queued_updates_.empty());

    // Delete update, causing it to finish, which should trigger a new update
    // for the queued host and master entry after a delay.
    delete update;
    EXPECT_TRUE(group_->restart_update_task_);

    // Set up checks for when queued update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    tested_manifest_ = MANIFEST1;
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        host->new_master_entry_url_, AppCacheEntry(AppCacheEntry::MASTER)));
    MockFrontend::HostIds ids1(1, host->host_id());
    frontend->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend->AddExpectedEvent(ids1, PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(ids1, CACHED_EVENT);

    // Group status will be IDLE so cannot call WaitForUpdateToFinish.
    group_->AddUpdateObserver(this);
  }

  void IfModifiedSinceTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    net::URLRequestJobFactory* new_factory(new net::URLRequestJobFactory);
    new_factory->SetProtocolHandler("http", new IfModifiedSinceJobFactory);
    io_thread_->SetNewJobFactory(new_factory);

    MakeService();
    group_ = new AppCacheGroup(service_.get(), GURL("http://headertest"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    // First test against a cache attempt. Will start manifest fetch
    // synchronously.
    HttpHeadersRequestTestJob::Initialize("", "");
    MockFrontend mock_frontend;
    AppCacheHost host(1, &mock_frontend, service_.get());
    update->StartUpdate(&host, GURL());
    HttpHeadersRequestTestJob::Verify();
    delete update;

    // Now simulate a refetch manifest request. Will start fetch request
    // synchronously.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "\0";
    net::HttpResponseHeaders* headers =
        new net::HttpResponseHeaders(std::string(data, arraysize(data)));
    net::HttpResponseInfo* response_info = new net::HttpResponseInfo();
    response_info->headers = headers;  // adds ref to headers

    HttpHeadersRequestTestJob::Initialize("", "");
    update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;
    group_->update_status_ = AppCacheGroup::DOWNLOADING;
    update->manifest_response_info_.reset(response_info);
    update->internal_state_ = AppCacheUpdateJob::REFETCH_MANIFEST;
    update->FetchManifest(false);  // not first request
    HttpHeadersRequestTestJob::Verify();
    delete update;

    // Change the headers to include a Last-Modified header. Manifest refetch
    // should include If-Modified-Since header.
    const char data2[] =
        "HTTP/1.1 200 OK\0"
        "Last-Modified: Sat, 29 Oct 1994 19:43:31 GMT\0"
        "\0";
    net::HttpResponseHeaders* headers2 =
        new net::HttpResponseHeaders(std::string(data2, arraysize(data2)));
    response_info = new net::HttpResponseInfo();
    response_info->headers = headers2;

    HttpHeadersRequestTestJob::Initialize("Sat, 29 Oct 1994 19:43:31 GMT", "");
    update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;
    group_->update_status_ = AppCacheGroup::DOWNLOADING;
    update->manifest_response_info_.reset(response_info);
    update->internal_state_ = AppCacheUpdateJob::REFETCH_MANIFEST;
    update->FetchManifest(false);  // not first request
    HttpHeadersRequestTestJob::Verify();
    delete update;

    UpdateFinished();
  }

  void IfModifiedSinceUpgradeTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    HttpHeadersRequestTestJob::Initialize("Sat, 29 Oct 1994 19:43:31 GMT", "");
    net::URLRequestJobFactory* new_factory(new net::URLRequestJobFactory);
    new_factory->SetProtocolHandler("http", new IfModifiedSinceJobFactory);
    io_thread_->SetNewJobFactory(new_factory);

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/manifest1"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    // Give the newest cache a manifest enry that is in storage.
    response_writer_.reset(
        service_->storage()->CreateResponseWriter(group_->manifest_url()));

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(),
                                        response_writer_->response_id());
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    host->AssociateCache(cache);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    MockFrontend::HostIds ids1(1, host->host_id());
    frontend->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend->AddExpectedEvent(ids1, PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(ids1, UPDATE_READY_EVENT);

    // Seed storage with expected manifest response info that will cause
    // an If-Modified-Since header to be put in the manifest fetch request.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "Last-Modified: Sat, 29 Oct 1994 19:43:31 GMT\0"
        "\0";
    net::HttpResponseHeaders* headers =
        new net::HttpResponseHeaders(std::string(data, arraysize(data)));
    net::HttpResponseInfo* response_info = new net::HttpResponseInfo();
    response_info->headers = headers;  // adds ref to headers
    scoped_refptr<HttpResponseInfoIOBuffer> io_buffer(
        new HttpResponseInfoIOBuffer(response_info));  // adds ref to info
    write_callback_.reset(
        new net::OldCompletionCallbackImpl<AppCacheUpdateJobTest>(this,
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData));
    response_writer_->WriteInfo(io_buffer, write_callback_.get());

    // Start update after data write completes asynchronously.
  }

  void IfNoneMatchUpgradeTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    HttpHeadersRequestTestJob::Initialize("", "\"LadeDade\"");
    net::URLRequestJobFactory* new_factory(new net::URLRequestJobFactory);
    new_factory->SetProtocolHandler("http", new IfModifiedSinceJobFactory);
    io_thread_->SetNewJobFactory(new_factory);

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), MockHttpServer::GetMockUrl("files/manifest1"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    // Give the newest cache a manifest enry that is in storage.
    response_writer_.reset(
        service_->storage()->CreateResponseWriter(group_->manifest_url()));

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(),
                                        response_writer_->response_id());
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    host->AssociateCache(cache);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    MockFrontend::HostIds ids1(1, host->host_id());
    frontend->AddExpectedEvent(ids1, CHECKING_EVENT);
    frontend->AddExpectedEvent(ids1, DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend->AddExpectedEvent(ids1, PROGRESS_EVENT);
    frontend->AddExpectedEvent(ids1, PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(ids1, UPDATE_READY_EVENT);

    // Seed storage with expected manifest response info that will cause
    // an If-None-Match header to be put in the manifest fetch request.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "ETag: \"LadeDade\"\0"
        "\0";
    net::HttpResponseHeaders* headers =
        new net::HttpResponseHeaders(std::string(data, arraysize(data)));
    net::HttpResponseInfo* response_info = new net::HttpResponseInfo();
    response_info->headers = headers;  // adds ref to headers
    scoped_refptr<HttpResponseInfoIOBuffer> io_buffer(
        new HttpResponseInfoIOBuffer(response_info));  // adds ref to info
    write_callback_.reset(
        new net::OldCompletionCallbackImpl<AppCacheUpdateJobTest>(this,
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData));
    response_writer_->WriteInfo(io_buffer, write_callback_.get());

    // Start update after data write completes asynchronously.
  }

  void IfNoneMatchRefetchTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    HttpHeadersRequestTestJob::Initialize("", "\"LadeDade\"");
    net::URLRequestJobFactory* new_factory(new net::URLRequestJobFactory);
    new_factory->SetProtocolHandler("http", new IfModifiedSinceJobFactory);
    io_thread_->SetNewJobFactory(new_factory);

    MakeService();
    group_ = new AppCacheGroup(service_.get(), GURL("http://headertest"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    // Simulate a refetch manifest request that uses an ETag header.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "ETag: \"LadeDade\"\0"
        "\0";
    net::HttpResponseHeaders* headers =
        new net::HttpResponseHeaders(std::string(data, arraysize(data)));
    net::HttpResponseInfo* response_info = new net::HttpResponseInfo();
    response_info->headers = headers;  // adds ref to headers

    group_->update_status_ = AppCacheGroup::DOWNLOADING;
    update->manifest_response_info_.reset(response_info);
    update->internal_state_ = AppCacheUpdateJob::REFETCH_MANIFEST;
    update->FetchManifest(false);  // not first request
    HttpHeadersRequestTestJob::Verify();
    delete update;

    UpdateFinished();
  }

  void MultipleHeadersRefetchTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    // Verify that code is correct when building multiple extra headers.
    HttpHeadersRequestTestJob::Initialize(
        "Sat, 29 Oct 1994 19:43:31 GMT", "\"LadeDade\"");
    net::URLRequestJobFactory* new_factory(new net::URLRequestJobFactory);
    new_factory->SetProtocolHandler("http", new IfModifiedSinceJobFactory);
    io_thread_->SetNewJobFactory(new_factory);

    MakeService();
    group_ = new AppCacheGroup(service_.get(), GURL("http://headertest"), 111);
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    // Simulate a refetch manifest request that uses an ETag header.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "Last-Modified: Sat, 29 Oct 1994 19:43:31 GMT\0"
        "ETag: \"LadeDade\"\0"
        "\0";
    net::HttpResponseHeaders* headers =
        new net::HttpResponseHeaders(std::string(data, arraysize(data)));
    net::HttpResponseInfo* response_info = new net::HttpResponseInfo();
    response_info->headers = headers;  // adds ref to headers

    group_->update_status_ = AppCacheGroup::DOWNLOADING;
    update->manifest_response_info_.reset(response_info);
    update->internal_state_ = AppCacheUpdateJob::REFETCH_MANIFEST;
    update->FetchManifest(false);  // not first request
    HttpHeadersRequestTestJob::Verify();
    delete update;

    UpdateFinished();
  }

  void CrossOriginHttpsSuccessTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    GURL manifest_url = MockHttpServer::GetMockHttpsUrl(
        "files/valid_cross_origin_https_manifest");

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), manifest_url,
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    tested_manifest_ = NONE;
    MockFrontend::HostIds host_ids(1, host->host_id());
    frontend->AddExpectedEvent(host_ids, CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void CrossOriginHttpsDeniedTest() {
    ASSERT_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

    GURL manifest_url = MockHttpServer::GetMockHttpsUrl(
        "files/invalid_cross_origin_https_manifest");

    MakeService();
    group_ = new AppCacheGroup(
        service_.get(), manifest_url,
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update = new AppCacheUpdateJob(service_.get(), group_);
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(1, frontend);
    update->StartUpdate(host, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    tested_manifest_ = NONE;
    MockFrontend::HostIds host_ids(1, host->host_id());
    frontend->AddExpectedEvent(host_ids, CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void WaitForUpdateToFinish() {
    if (group_->update_status() == AppCacheGroup::IDLE)
      UpdateFinished();
    else
      group_->AddUpdateObserver(this);
  }

  void OnUpdateComplete(AppCacheGroup* group) {
    ASSERT_EQ(group_, group);
    protect_newest_cache_ = group->newest_complete_cache();
    UpdateFinished();
  }

  void UpdateFinished() {
    // We unwind the stack prior to finishing up to let stack-based objects
    // get deleted.
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
            this, &AppCacheUpdateJobTest::UpdateFinishedUnwound));
  }

  void UpdateFinishedUnwound() {
    EXPECT_EQ(AppCacheGroup::IDLE, group_->update_status());
    EXPECT_TRUE(group_->update_job() == NULL);
    if (do_checks_after_update_finished_)
      VerifyExpectations();

    // Clean up everything that was created on the IO thread.
    protect_newest_cache_ = NULL;
    group_ = NULL;
    STLDeleteContainerPointers(hosts_.begin(), hosts_.end());
    STLDeleteContainerPointers(frontends_.begin(), frontends_.end());
    response_infos_.clear();
    service_.reset(NULL);

    event_->Signal();
  }

  void MakeService() {
    service_.reset(new MockAppCacheService());
    service_->set_request_context(io_thread_->request_context());
  }

  AppCache* MakeCacheForGroup(int64 cache_id, int64 manifest_response_id) {
    AppCache* cache = new AppCache(service_.get(), cache_id);
    cache->set_complete(true);
    cache->set_update_time(base::Time::Now());
    group_->AddCache(cache);

    // Add manifest entry to cache.
    cache->AddEntry(group_->manifest_url(),
        AppCacheEntry(AppCacheEntry::MANIFEST, manifest_response_id));

    return cache;
  }

  AppCacheHost* MakeHost(int host_id, AppCacheFrontend* frontend) {
    AppCacheHost* host = new AppCacheHost(host_id, frontend, service_.get());
    hosts_.push_back(host);
    return host;
  }

  AppCacheResponseInfo* MakeAppCacheResponseInfo(
      const GURL& manifest_url, int64 response_id,
      const std::string& raw_headers) {
    net::HttpResponseInfo* http_info = new net::HttpResponseInfo();
    http_info->headers = new net::HttpResponseHeaders(raw_headers);
    scoped_refptr<AppCacheResponseInfo> info(
        new AppCacheResponseInfo(service_.get(), manifest_url,
                                 response_id, http_info, 0));
    response_infos_.push_back(info);
    return info;
  }

  MockFrontend* MakeMockFrontend() {
    MockFrontend* frontend = new MockFrontend();
    frontends_.push_back(frontend);
    return frontend;
  }

  // Verifies conditions about the group and notifications after an update
  // has finished. Cannot verify update job internals as update is deleted.
  void VerifyExpectations() {
    RetryRequestTestJob::Verify();
    HttpHeadersRequestTestJob::Verify();

    EXPECT_EQ(expect_group_obsolete_, group_->is_obsolete());

    if (expect_group_has_cache_) {
      EXPECT_TRUE(group_->newest_complete_cache() != NULL);

      if (expect_non_null_update_time_)
        EXPECT_TRUE(!group_->newest_complete_cache()->update_time().is_null());

      if (expect_old_cache_) {
        EXPECT_NE(expect_old_cache_, group_->newest_complete_cache());
        EXPECT_TRUE(group_->old_caches().end() !=
            std::find(group_->old_caches().begin(),
                      group_->old_caches().end(), expect_old_cache_));
      }
      if (expect_newest_cache_) {
        EXPECT_EQ(expect_newest_cache_, group_->newest_complete_cache());
        EXPECT_TRUE(group_->old_caches().end() ==
            std::find(group_->old_caches().begin(),
                      group_->old_caches().end(), expect_newest_cache_));
      } else {
        // Tests that don't know which newest cache to expect contain updates
        // that succeed (because the update creates a new cache whose pointer
        // is unknown to the test). Check group and newest cache were stored
        // when update succeeds.
        MockAppCacheStorage* storage =
            reinterpret_cast<MockAppCacheStorage*>(service_->storage());
        EXPECT_TRUE(storage->IsGroupStored(group_));
        EXPECT_TRUE(storage->IsCacheStored(group_->newest_complete_cache()));

        // Check that all entries in the newest cache were stored.
        const AppCache::EntryMap& entries =
            group_->newest_complete_cache()->entries();
        for (AppCache::EntryMap::const_iterator it = entries.begin();
             it != entries.end(); ++it) {
          EXPECT_NE(kNoResponseId, it->second.response_id());

          // Check that any copied entries have the expected response id
          // and that entries that are not copied have a different response id.
          std::map<GURL, int64>::iterator found =
              expect_response_ids_.find(it->first);
          if (found != expect_response_ids_.end()) {
            EXPECT_EQ(found->second, it->second.response_id());
          } else if (expect_old_cache_) {
            AppCacheEntry* old_entry = expect_old_cache_->GetEntry(it->first);
            if (old_entry)
              EXPECT_NE(old_entry->response_id(), it->second.response_id());
          }
        }
      }
    } else {
      EXPECT_TRUE(group_->newest_complete_cache() == NULL);
    }

    // Check expected events.
    for (size_t i = 0; i < frontends_.size(); ++i) {
      MockFrontend* frontend = frontends_[i];

      MockFrontend::RaisedEvents& expected_events = frontend->expected_events_;
      MockFrontend::RaisedEvents& actual_events = frontend->raised_events_;
      EXPECT_EQ(expected_events.size(), actual_events.size());

      // Check each expected event.
      for (size_t j = 0;
           j < expected_events.size() && j < actual_events.size(); ++j) {
        EXPECT_EQ(expected_events[j].second, actual_events[j].second);

        MockFrontend::HostIds& expected_ids = expected_events[j].first;
        MockFrontend::HostIds& actual_ids = actual_events[j].first;
        EXPECT_EQ(expected_ids.size(), actual_ids.size());

        for (size_t k = 0; k < expected_ids.size(); ++k) {
          int id = expected_ids[k];
          EXPECT_TRUE(std::find(actual_ids.begin(), actual_ids.end(), id) !=
              actual_ids.end());
        }
      }

      if (!frontend->expected_error_message_.empty()) {
        EXPECT_EQ(frontend->expected_error_message_,
                  frontend->error_message_);
      }
    }

    // Verify expected cache contents last as some checks are asserts
    // and will abort the test if they fail.
    if (tested_manifest_) {
      AppCache* cache = group_->newest_complete_cache();
      ASSERT_TRUE(cache != NULL);
      EXPECT_EQ(group_, cache->owning_group());
      EXPECT_TRUE(cache->is_complete());

      switch (tested_manifest_) {
        case MANIFEST1:
          VerifyManifest1(cache);
          break;
        case MANIFEST_MERGED_TYPES:
          VerifyManifestMergedTypes(cache);
          break;
        case EMPTY_MANIFEST:
          VerifyEmptyManifest(cache);
          break;
        case EMPTY_FILE_MANIFEST:
          VerifyEmptyFileManifest(cache);
          break;
        case PENDING_MASTER_NO_UPDATE:
          VerifyMasterEntryNoUpdate(cache);
          break;
        case NONE:
        default:
          break;
      }
    }
  }

  void VerifyManifest1(AppCache* cache) {
    size_t expected = 3 + expect_extra_entries_.size();
    EXPECT_EQ(expected, cache->entries().size());
    const char* kManifestPath = tested_manifest_path_override_ ?
        tested_manifest_path_override_ :
        "files/manifest1";
    AppCacheEntry* entry =
        cache->GetEntry(MockHttpServer::GetMockUrl(kManifestPath));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MANIFEST, entry->types());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/explicit1"));
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->IsExplicit());
    entry = cache->GetEntry(
        MockHttpServer::GetMockUrl("files/fallback1a"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::FALLBACK, entry->types());

    for (AppCache::EntryMap::iterator i = expect_extra_entries_.begin();
         i != expect_extra_entries_.end(); ++i) {
      entry = cache->GetEntry(i->first);
      ASSERT_TRUE(entry);
      EXPECT_EQ(i->second.types(), entry->types());
    }

    expected = 1;
    EXPECT_EQ(expected, cache->fallback_namespaces_.size());
    EXPECT_TRUE(cache->fallback_namespaces_.end() !=
        std::find(cache->fallback_namespaces_.begin(),
                  cache->fallback_namespaces_.end(),
                  FallbackNamespace(
                      MockHttpServer::GetMockUrl("files/fallback1"),
                      MockHttpServer::GetMockUrl("files/fallback1a"))));

    EXPECT_TRUE(cache->online_whitelist_namespaces_.empty());
    EXPECT_TRUE(cache->online_whitelist_all_);

    EXPECT_TRUE(cache->update_time_ > base::Time());
  }

  void VerifyManifestMergedTypes(AppCache* cache) {
    size_t expected = 2;
    EXPECT_EQ(expected, cache->entries().size());
    AppCacheEntry* entry = cache->GetEntry(
        MockHttpServer::GetMockUrl("files/manifest-merged-types"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::EXPLICIT | AppCacheEntry::MANIFEST,
              entry->types());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/explicit1"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::EXPLICIT | AppCacheEntry::FALLBACK |
        AppCacheEntry::MASTER, entry->types());

    expected = 1;
    EXPECT_EQ(expected, cache->fallback_namespaces_.size());
    EXPECT_TRUE(cache->fallback_namespaces_.end() !=
        std::find(cache->fallback_namespaces_.begin(),
                  cache->fallback_namespaces_.end(),
                  FallbackNamespace(
                      MockHttpServer::GetMockUrl("files/fallback1"),
                      MockHttpServer::GetMockUrl("files/explicit1"))));

    EXPECT_EQ(expected, cache->online_whitelist_namespaces_.size());
    EXPECT_TRUE(cache->online_whitelist_namespaces_.end() !=
        std::find(cache->online_whitelist_namespaces_.begin(),
                  cache->online_whitelist_namespaces_.end(),
                  MockHttpServer::GetMockUrl("files/online1")));
    EXPECT_FALSE(cache->online_whitelist_all_);

    EXPECT_TRUE(cache->update_time_ > base::Time());
  }

  void VerifyEmptyManifest(AppCache* cache) {
    const char* kManifestPath = tested_manifest_path_override_ ?
        tested_manifest_path_override_ :
        "files/empty-manifest";
    size_t expected = 1;
    EXPECT_EQ(expected, cache->entries().size());
    AppCacheEntry* entry = cache->GetEntry(
        MockHttpServer::GetMockUrl(kManifestPath));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MANIFEST, entry->types());

    EXPECT_TRUE(cache->fallback_namespaces_.empty());
    EXPECT_TRUE(cache->online_whitelist_namespaces_.empty());
    EXPECT_FALSE(cache->online_whitelist_all_);

    EXPECT_TRUE(cache->update_time_ > base::Time());
  }

  void VerifyEmptyFileManifest(AppCache* cache) {
    EXPECT_EQ(size_t(2), cache->entries().size());
    AppCacheEntry* entry = cache->GetEntry(
        MockHttpServer::GetMockUrl("files/empty-file-manifest"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MANIFEST, entry->types());

    entry = cache->GetEntry(
        MockHttpServer::GetMockUrl("files/empty1"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::EXPLICIT, entry->types());
    EXPECT_TRUE(entry->has_response_id());

    EXPECT_TRUE(cache->fallback_namespaces_.empty());
    EXPECT_TRUE(cache->online_whitelist_namespaces_.empty());
    EXPECT_FALSE(cache->online_whitelist_all_);

    EXPECT_TRUE(cache->update_time_ > base::Time());
  }

  void VerifyMasterEntryNoUpdate(AppCache* cache) {
    EXPECT_EQ(size_t(3), cache->entries().size());
    AppCacheEntry* entry = cache->GetEntry(
        MockHttpServer::GetMockUrl("files/notmodified"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MANIFEST, entry->types());

    entry = cache->GetEntry(
        MockHttpServer::GetMockUrl("files/explicit1"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MASTER, entry->types());
    EXPECT_TRUE(entry->has_response_id());

    entry = cache->GetEntry(
        MockHttpServer::GetMockUrl("files/explicit2"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::EXPLICIT | AppCacheEntry::MASTER, entry->types());
    EXPECT_TRUE(entry->has_response_id());

    EXPECT_TRUE(cache->fallback_namespaces_.empty());
    EXPECT_TRUE(cache->online_whitelist_namespaces_.empty());
    EXPECT_FALSE(cache->online_whitelist_all_);

    EXPECT_TRUE(cache->update_time_ > base::Time());
  }

 private:
  // Various manifest files used in this test.
  enum TestedManifest {
    NONE,
    MANIFEST1,
    MANIFEST_MERGED_TYPES,
    EMPTY_MANIFEST,
    EMPTY_FILE_MANIFEST,
    PENDING_MASTER_NO_UPDATE,
  };

  scoped_ptr<IOThread> io_thread_;

  scoped_ptr<MockAppCacheService> service_;
  scoped_refptr<AppCacheGroup> group_;
  scoped_refptr<AppCache> protect_newest_cache_;
  scoped_ptr<base::WaitableEvent> event_;

  scoped_ptr<AppCacheResponseWriter> response_writer_;
  scoped_ptr<net::OldCompletionCallbackImpl<AppCacheUpdateJobTest> >
      write_callback_;

  // Hosts used by an async test that need to live until update job finishes.
  // Otherwise, test can put host on the stack instead of here.
  std::vector<AppCacheHost*> hosts_;

  // Response infos used by an async test that need to live until update job
  // finishes.
  std::vector<scoped_refptr<AppCacheResponseInfo> > response_infos_;

  // Flag indicating if test cares to verify the update after update finishes.
  bool do_checks_after_update_finished_;
  bool expect_group_obsolete_;
  bool expect_group_has_cache_;
  AppCache* expect_old_cache_;
  AppCache* expect_newest_cache_;
  bool expect_non_null_update_time_;
  std::vector<MockFrontend*> frontends_;  // to check expected events
  TestedManifest tested_manifest_;
  const char* tested_manifest_path_override_;
  AppCache::EntryMap expect_extra_entries_;
  std::map<GURL, int64> expect_response_ids_;
};

TEST_F(AppCacheUpdateJobTest, AlreadyChecking) {
  MockAppCacheService service;
  scoped_refptr<AppCacheGroup> group(
      new AppCacheGroup(&service, GURL("http://manifesturl.com"),
                        service.storage()->NewGroupId()));

  AppCacheUpdateJob update(&service, group);

  // Pretend group is in checking state.
  group->update_job_ = &update;
  group->update_status_ = AppCacheGroup::CHECKING;

  update.StartUpdate(NULL, GURL());
  EXPECT_EQ(AppCacheGroup::CHECKING, group->update_status());

  MockFrontend mock_frontend;
  AppCacheHost host(1, &mock_frontend, &service);
  update.StartUpdate(&host, GURL());

  MockFrontend::RaisedEvents events = mock_frontend.raised_events_;
  size_t expected = 1;
  EXPECT_EQ(expected, events.size());
  EXPECT_EQ(expected, events[0].first.size());
  EXPECT_EQ(host.host_id(), events[0].first[0]);
  EXPECT_EQ(CHECKING_EVENT, events[0].second);
  EXPECT_EQ(AppCacheGroup::CHECKING, group->update_status());
}

TEST_F(AppCacheUpdateJobTest, AlreadyDownloading) {
  MockAppCacheService service;
  scoped_refptr<AppCacheGroup> group(
      new AppCacheGroup(&service, GURL("http://manifesturl.com"),
                        service.storage()->NewGroupId()));

  AppCacheUpdateJob update(&service, group);

  // Pretend group is in downloading state.
  group->update_job_ = &update;
  group->update_status_ = AppCacheGroup::DOWNLOADING;

  update.StartUpdate(NULL, GURL());
  EXPECT_EQ(AppCacheGroup::DOWNLOADING, group->update_status());

  MockFrontend mock_frontend;
  AppCacheHost host(1, &mock_frontend, &service);
  update.StartUpdate(&host, GURL());

  MockFrontend::RaisedEvents events = mock_frontend.raised_events_;
  size_t expected = 2;
  EXPECT_EQ(expected, events.size());
  expected = 1;
  EXPECT_EQ(expected, events[0].first.size());
  EXPECT_EQ(host.host_id(), events[0].first[0]);
  EXPECT_EQ(CHECKING_EVENT, events[0].second);

  EXPECT_EQ(expected, events[1].first.size());
  EXPECT_EQ(host.host_id(), events[1].first[0]);
  EXPECT_EQ(appcache::DOWNLOADING_EVENT, events[1].second);

  EXPECT_EQ(AppCacheGroup::DOWNLOADING, group->update_status());
}

TEST_F(AppCacheUpdateJobTest, StartCacheAttempt) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::StartCacheAttemptTest);
}

TEST_F(AppCacheUpdateJobTest, StartUpgradeAttempt) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::StartUpgradeAttemptTest);
}

TEST_F(AppCacheUpdateJobTest, CacheAttemptFetchManifestFail) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::CacheAttemptFetchManifestFailTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeFetchManifestFail) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeFetchManifestFailTest);
}

TEST_F(AppCacheUpdateJobTest, ManifestRedirect) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::ManifestRedirectTest);
}

TEST_F(AppCacheUpdateJobTest, ManifestWrongMimeType) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::ManifestWrongMimeTypeTest);
}

TEST_F(AppCacheUpdateJobTest, ManifestNotFound) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::ManifestNotFoundTest);
}

TEST_F(AppCacheUpdateJobTest, ManifestGone) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::ManifestGoneTest);
}

TEST_F(AppCacheUpdateJobTest, CacheAttemptNotModified) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::CacheAttemptNotModifiedTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeNotModified) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeNotModifiedTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeManifestDataUnchanged) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeManifestDataUnchangedTest);
}

TEST_F(AppCacheUpdateJobTest, BasicCacheAttemptSuccess) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::BasicCacheAttemptSuccessTest);
}

TEST_F(AppCacheUpdateJobTest, BasicUpgradeSuccess) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::BasicUpgradeSuccessTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeLoadFromNewestCache) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeLoadFromNewestCacheTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeNoLoadFromNewestCache) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeNoLoadFromNewestCacheTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeLoadFromNewestCacheVaryHeader) {
  RunTestOnIOThread(
      &AppCacheUpdateJobTest::UpgradeLoadFromNewestCacheVaryHeaderTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeSuccessMergedTypes) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeSuccessMergedTypesTest);
}

TEST_F(AppCacheUpdateJobTest, CacheAttemptFailUrlFetch) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::CacheAttemptFailUrlFetchTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeFailUrlFetch) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeFailUrlFetchTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeFailMasterUrlFetch) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeFailMasterUrlFetchTest);
}

TEST_F(AppCacheUpdateJobTest, EmptyManifest) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::EmptyManifestTest);
}

TEST_F(AppCacheUpdateJobTest, EmptyFile) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::EmptyFileTest);
}

TEST_F(AppCacheUpdateJobTest, RetryRequest) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::RetryRequestTest);
}

TEST_F(AppCacheUpdateJobTest, RetryNoRetryAfter) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::RetryNoRetryAfterTest);
}

TEST_F(AppCacheUpdateJobTest, RetryNonzeroRetryAfter) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::RetryNonzeroRetryAfterTest);
}

TEST_F(AppCacheUpdateJobTest, RetrySuccess) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::RetrySuccessTest);
}

TEST_F(AppCacheUpdateJobTest, RetryUrl) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::RetryUrlTest);
}

TEST_F(AppCacheUpdateJobTest, FailStoreNewestCache) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::FailStoreNewestCacheTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryFailStoreNewestCacheTest) {
  RunTestOnIOThread(
      &AppCacheUpdateJobTest::MasterEntryFailStoreNewestCacheTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeFailStoreNewestCache) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeFailStoreNewestCacheTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeFailMakeGroupObsolete) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeFailMakeGroupObsoleteTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryFetchManifestFail) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::MasterEntryFetchManifestFailTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryBadManifest) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::MasterEntryBadManifestTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryManifestNotFound) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::MasterEntryManifestNotFoundTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryFailUrlFetch) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::MasterEntryFailUrlFetchTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryAllFail) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::MasterEntryAllFailTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeMasterEntryAllFail) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeMasterEntryAllFailTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntrySomeFail) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::MasterEntrySomeFailTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeMasterEntrySomeFail) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::UpgradeMasterEntrySomeFailTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryNoUpdate) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::MasterEntryNoUpdateTest);
}

TEST_F(AppCacheUpdateJobTest, StartUpdateMidCacheAttempt) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::StartUpdateMidCacheAttemptTest);
}

TEST_F(AppCacheUpdateJobTest, StartUpdateMidNoUpdate) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::StartUpdateMidNoUpdateTest);
}

TEST_F(AppCacheUpdateJobTest, StartUpdateMidDownload) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::StartUpdateMidDownloadTest);
}

TEST_F(AppCacheUpdateJobTest, QueueMasterEntry) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::QueueMasterEntryTest);
}

TEST_F(AppCacheUpdateJobTest, IfModifiedSince) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::IfModifiedSinceTest);
}

TEST_F(AppCacheUpdateJobTest, IfModifiedSinceUpgrade) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::IfModifiedSinceUpgradeTest);
}

TEST_F(AppCacheUpdateJobTest, IfNoneMatchUpgrade) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::IfNoneMatchUpgradeTest);
}

TEST_F(AppCacheUpdateJobTest, IfNoneMatchRefetch) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::IfNoneMatchRefetchTest);
}

TEST_F(AppCacheUpdateJobTest, MultipleHeadersRefetch) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::MultipleHeadersRefetchTest);
}

TEST_F(AppCacheUpdateJobTest, CrossOriginHttpsSuccess) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::CrossOriginHttpsSuccessTest);
}

TEST_F(AppCacheUpdateJobTest, CrossOriginHttpsDenied) {
  RunTestOnIOThread(&AppCacheUpdateJobTest::CrossOriginHttpsDeniedTest);
}

}  // namespace appcache

// AppCacheUpdateJobTest is expected to always live longer than the
// runnable methods.  This lets us call NewRunnableMethod on its instances.
DISABLE_RUNNABLE_METHOD_REFCOUNT(appcache::AppCacheUpdateJobTest);
