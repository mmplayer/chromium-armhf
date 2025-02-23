// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/safe_browsing/browser_feature_extractor.h"
#include "chrome/browser/safe_browsing/client_side_detection_host.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents_wrapper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/common/safe_browsing/safebrowsing_messages.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;

namespace {
const bool kFalse = false;
const bool kTrue = true;
}

namespace safe_browsing {
namespace {
// This matcher verifies that the client computed verdict
// (ClientPhishingRequest) which is passed to SendClientReportPhishingRequest
// has the expected fields set.  Note: we can't simply compare the protocol
// buffer strings because the BrowserFeatureExtractor might add features to the
// verdict object before calling SendClientReportPhishingRequest.
MATCHER_P(PartiallyEqualVerdict, other, "") {
  return (other.url() == arg.url() &&
          other.client_score() == arg.client_score() &&
          other.is_phishing() == arg.is_phishing());
}

ACTION(QuitUIMessageLoop) {
  EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
  MessageLoopForUI::current()->Quit();
}

// It's kind of insane that InvokeArgument doesn't work with callbacks, but it
// doesn't seem like it.
ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_2_VALUE_PARAMS(p0, p1)) {
  ::std::tr1::get<k>(args)->Run(p0, p1);
  // This is an old callback, so it needs to be deleted explicitly.
  delete ::std::tr1::get<k>(args);
}

class MockClientSideDetectionService : public ClientSideDetectionService {
 public:
  MockClientSideDetectionService() : ClientSideDetectionService(NULL) {}
  virtual ~MockClientSideDetectionService() {};

  MOCK_METHOD2(SendClientReportPhishingRequest,
               void(ClientPhishingRequest*,
                    ClientReportPhishingRequestCallback*));
  MOCK_CONST_METHOD1(IsPrivateIPAddress, bool(const std::string&));
  MOCK_METHOD2(GetValidCachedResult, bool(const GURL&, bool*));
  MOCK_METHOD1(IsInCache, bool(const GURL&));
  MOCK_METHOD0(OverReportLimit, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientSideDetectionService);
};

class MockSafeBrowsingService : public SafeBrowsingService {
 public:
  MockSafeBrowsingService() {}
  virtual ~MockSafeBrowsingService() {}

  MOCK_METHOD1(DoDisplayBlockingPage, void(const UnsafeResource& resource));
  MOCK_METHOD1(MatchCsdWhitelistUrl, bool(const GURL&));

  // Helper function which calls OnBlockingPageComplete for this client
  // object.
  void InvokeOnBlockingPageComplete(SafeBrowsingService::Client* client) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    DCHECK(client);
    // Note: this will delete the client object in the case of the CsdClient
    // implementation.
    client->OnBlockingPageComplete(false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingService);
};

class MockTestingProfile : public TestingProfile {
 public:
  MockTestingProfile() {}
  virtual ~MockTestingProfile() {}

  MOCK_METHOD0(IsOffTheRecord, bool());
};

class MockBrowserFeatureExtractor : public BrowserFeatureExtractor {
 public:
  explicit MockBrowserFeatureExtractor(
      TabContents* tab,
      ClientSideDetectionService* service)
      : BrowserFeatureExtractor(tab, service) {}
  virtual ~MockBrowserFeatureExtractor() {}

  MOCK_METHOD3(ExtractFeatures, void(const BrowseInfo* info,
                                     ClientPhishingRequest*,
                                     BrowserFeatureExtractor::DoneCallback*));
};

// Helper function which quits the UI message loop from the IO message loop.
void QuitUIMessageLoopFromIO() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          new MessageLoop::QuitTask());
}
}  // namespace

class ClientSideDetectionHostTest : public TabContentsWrapperTestHarness {
 public:
  virtual void SetUp() {
    // Set custom profile object so that we can mock calls to IsOffTheRecord.
    // This needs to happen before we call the parent SetUp() function.  We use
    // a nice mock because other parts of the code are calling IsOffTheRecord.
    mock_profile_ = new NiceMock<MockTestingProfile>();
    browser_context_.reset(mock_profile_);

    ui_thread_.reset(new BrowserThread(BrowserThread::UI, &message_loop_));
    // Note: we're starting a real IO thread to make sure our DCHECKs that
    // verify which thread is running are actually tested.
    io_thread_.reset(new BrowserThread(BrowserThread::IO));
    ASSERT_TRUE(io_thread_->Start());

    TabContentsWrapperTestHarness::SetUp();

    // Inject service classes.
    csd_service_.reset(new StrictMock<MockClientSideDetectionService>());
    sb_service_ = new StrictMock<MockSafeBrowsingService>();
    csd_host_.reset(safe_browsing::ClientSideDetectionHost::Create(
        contents_wrapper()->tab_contents()));
    csd_host_->set_client_side_detection_service(csd_service_.get());
    csd_host_->set_safe_browsing_service(sb_service_.get());
    // We need to create this here since we don't call
    // DidNavigateMainFramePostCommit in this test.
    csd_host_->browse_info_.reset(new BrowseInfo);
  }

  virtual void TearDown() {
    // Delete the host object on the UI thread.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        new DeleteTask<ClientSideDetectionHost>(csd_host_.release()));
    message_loop_.RunAllPending();
    TabContentsWrapperTestHarness::TearDown();
    io_thread_.reset();
    ui_thread_.reset();
  }

  void OnPhishingDetectionDone(const std::string& verdict_str) {
    csd_host_->OnPhishingDetectionDone(verdict_str);
  }

  void FlushIOMessageLoop() {
    // If there was a message posted on the IO thread to display the
    // interstitial page we know that it would have been posted before
    // we put the quit message there.
    BrowserThread::PostTask(BrowserThread::IO,
                            FROM_HERE,
                            NewRunnableFunction(&QuitUIMessageLoopFromIO));
    MessageLoop::current()->Run();
  }

  void ExpectPreClassificationChecks(const GURL& url,
                                     const bool* is_private,
                                     const bool* is_incognito,
                                     const bool* match_csd_whitelist,
                                     const bool* get_valid_cached_result,
                                     const bool* is_in_cache,
                                     const bool* over_report_limit) {
    if (is_private) {
      EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_))
          .WillOnce(Return(*is_private));
    }
    if (is_incognito) {
      EXPECT_CALL(*mock_profile_, IsOffTheRecord())
          .WillRepeatedly(Return(*is_incognito));
    }
    if (match_csd_whitelist) {
      EXPECT_CALL(*sb_service_, MatchCsdWhitelistUrl(url))
          .WillOnce(Return(*match_csd_whitelist));
    }
    if (get_valid_cached_result) {
      EXPECT_CALL(*csd_service_, GetValidCachedResult(url, NotNull()))
          .WillOnce(DoAll(SetArgumentPointee<1>(true),
                          Return(*get_valid_cached_result)));
    }
    if (is_in_cache) {
      EXPECT_CALL(*csd_service_, IsInCache(url)).WillOnce(Return(*is_in_cache));
    }
    if (over_report_limit) {
      EXPECT_CALL(*csd_service_, OverReportLimit())
          .WillOnce(Return(*over_report_limit));
    }
  }

  void WaitAndCheckPreClassificationChecks() {
    // Wait for CheckCsdWhitelist to be called if at all.
    FlushIOMessageLoop();
    // Checks for CheckCache() to be called if at all.
    MessageLoop::current()->RunAllPending();
    EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
    EXPECT_TRUE(Mock::VerifyAndClear(sb_service_.get()));
    EXPECT_TRUE(Mock::VerifyAndClear(mock_profile_));
  }

  void SetFeatureExtractor(BrowserFeatureExtractor* extractor) {
    csd_host_->feature_extractor_.reset(extractor);
  }

  void SetRedirectChain(const std::vector<GURL>& redirect_chain) {
    csd_host_->browse_info_->url_redirects = redirect_chain;
  }

  void SetUnsafeResourceToCurrent() {
    SafeBrowsingService::UnsafeResource resource;
    resource.url = GURL("http://www.malware.com/");
    resource.original_url = contents()->GetURL();
    resource.is_subresource = true;
    resource.threat_type = SafeBrowsingService::URL_MALWARE;
    // Bogus client class.  We just need this class to check that the client
    // field of the UnsafeResource gets cleared before we stored it in the
    // host object.
    class BogusClient : public SafeBrowsingService::Client {
     public:
      BogusClient() {}
      virtual ~BogusClient() {}
    };
    resource.client = new BogusClient();
    resource.render_process_host_id = contents()->GetRenderProcessHost()->id();
    resource.render_view_id = contents()->render_view_host()->routing_id();
    csd_host_->OnSafeBrowsingHit(resource);
    delete resource.client;
    ASSERT_TRUE(csd_host_->DidShowSBInterstitial());
    ASSERT_TRUE(csd_host_->unsafe_resource_.get());
    // Test that the resource above was copied.
    EXPECT_EQ(resource.url, csd_host_->unsafe_resource_->url);
    EXPECT_EQ(resource.original_url, csd_host_->unsafe_resource_->original_url);
    EXPECT_EQ(resource.is_subresource,
              csd_host_->unsafe_resource_->is_subresource);
    EXPECT_EQ(resource.threat_type, csd_host_->unsafe_resource_->threat_type);
    EXPECT_EQ(NULL, csd_host_->unsafe_resource_->client);
    EXPECT_EQ(resource.render_process_host_id,
              csd_host_->unsafe_resource_->render_process_host_id);
    EXPECT_EQ(resource.render_view_id,
              csd_host_->unsafe_resource_->render_view_id);
  }

 protected:
  scoped_ptr<ClientSideDetectionHost> csd_host_;
  scoped_ptr<StrictMock<MockClientSideDetectionService> > csd_service_;
  scoped_refptr<StrictMock<MockSafeBrowsingService> > sb_service_;
  MockTestingProfile* mock_profile_;  // We don't own this object

 private:
  scoped_ptr<BrowserThread> ui_thread_;
  scoped_ptr<BrowserThread> io_thread_;
};

TEST_F(ClientSideDetectionHostTest, OnPhishingDetectionDoneInvalidVerdict) {
  // Case 0: renderer sends an invalid verdict string that we're unable to
  // parse.
  MockBrowserFeatureExtractor* mock_extractor = new MockBrowserFeatureExtractor(
      contents(),
      csd_service_.get());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.
  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _)).Times(0);
  OnPhishingDetectionDone("Invalid Protocol Buffer");
  EXPECT_TRUE(Mock::VerifyAndClear(mock_extractor));
}

TEST_F(ClientSideDetectionHostTest, OnPhishingDetectionDoneNotPhishing) {
  // Case 1: client thinks the page is phishing.  The server does not agree.
  // No interstitial is shown.
  MockBrowserFeatureExtractor* mock_extractor = new MockBrowserFeatureExtractor(
      contents(),
      csd_service_.get());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.

  ClientSideDetectionService::ClientReportPhishingRequestCallback* cb;
  ClientPhishingRequest verdict;
  verdict.set_url("http://phishingurl.com/");
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _))
      .WillOnce(DoAll(DeleteArg<1>(),
                      InvokeCallbackArgument<2>(true, &verdict)));
  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(
                  Pointee(PartiallyEqualVerdict(verdict)), _))
      .WillOnce(SaveArg<1>(&cb));
  OnPhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_TRUE(cb);

  // Make sure DoDisplayBlockingPage is not going to be called.
  EXPECT_CALL(*sb_service_, DoDisplayBlockingPage(_)).Times(0);
  cb->Run(GURL(verdict.url()), false);
  delete cb;
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(Mock::VerifyAndClear(sb_service_.get()));
}

TEST_F(ClientSideDetectionHostTest, OnPhishingDetectionDoneDisabled) {
  // Case 2: client thinks the page is phishing and so does the server but
  // showing the interstitial is disabled => no interstitial is shown.
  MockBrowserFeatureExtractor* mock_extractor = new MockBrowserFeatureExtractor(
      contents(),
      csd_service_.get());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.

  ClientSideDetectionService::ClientReportPhishingRequestCallback* cb;
  ClientPhishingRequest verdict;
  verdict.set_url("http://phishingurl.com/");
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _))
      .WillOnce(DoAll(DeleteArg<1>(),
                      InvokeCallbackArgument<2>(true, &verdict)));
  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(
                  Pointee(PartiallyEqualVerdict(verdict)), _))
      .WillOnce(SaveArg<1>(&cb));
  OnPhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_TRUE(cb);

  // Make sure DoDisplayBlockingPage is not going to be called.
  EXPECT_CALL(*sb_service_, DoDisplayBlockingPage(_)).Times(0);
  cb->Run(GURL(verdict.url()), false);
  delete cb;
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(Mock::VerifyAndClear(sb_service_.get()));
}

TEST_F(ClientSideDetectionHostTest, OnPhishingDetectionDoneShowInterstitial) {
  // Case 3: client thinks the page is phishing and so does the server.
  // We show an interstitial.
  MockBrowserFeatureExtractor* mock_extractor = new MockBrowserFeatureExtractor(
      contents(),
      csd_service_.get());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.

  ClientSideDetectionService::ClientReportPhishingRequestCallback* cb;
  GURL phishing_url("http://phishingurl.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(phishing_url.spec());
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _))
      .WillOnce(DoAll(DeleteArg<1>(),
                      InvokeCallbackArgument<2>(true, &verdict)));
  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(
                  Pointee(PartiallyEqualVerdict(verdict)), _))
      .WillOnce(SaveArg<1>(&cb));
  OnPhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_TRUE(cb);

  SafeBrowsingService::UnsafeResource resource;
  EXPECT_CALL(*sb_service_, DoDisplayBlockingPage(_))
      .WillOnce(SaveArg<0>(&resource));
  cb->Run(phishing_url, true);
  delete cb;

  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(Mock::VerifyAndClear(sb_service_.get()));
  EXPECT_EQ(phishing_url, resource.url);
  EXPECT_EQ(phishing_url, resource.original_url);
  EXPECT_FALSE(resource.is_subresource);
  EXPECT_EQ(SafeBrowsingService::CLIENT_SIDE_PHISHING_URL,
            resource.threat_type);
  EXPECT_EQ(contents()->GetRenderProcessHost()->id(),
            resource.render_process_host_id);
  EXPECT_EQ(contents()->render_view_host()->routing_id(),
            resource.render_view_id);

  // Make sure the client object will be deleted.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      NewRunnableMethod(
          sb_service_.get(),
          &MockSafeBrowsingService::InvokeOnBlockingPageComplete,
          resource.client));
  // Since the CsdClient object will be deleted on the UI thread I need
  // to run the UI message loop.  Post a task to stop the UI message loop
  // after the client object destructor is called.
  FlushIOMessageLoop();
}

TEST_F(ClientSideDetectionHostTest, OnPhishingDetectionDoneMultiplePings) {
  // Case 4 & 5: client thinks a page is phishing then navigates to
  // another page which is also considered phishing by the client
  // before the server responds with a verdict.  After a while the
  // server responds for both requests with a phishing verdict.  Only
  // a single interstitial is shown for the second URL.
  MockBrowserFeatureExtractor* mock_extractor = new MockBrowserFeatureExtractor(
      contents(),
      csd_service_.get());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.

  ClientSideDetectionService::ClientReportPhishingRequestCallback* cb;
  GURL phishing_url("http://phishingurl.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(phishing_url.spec());
  verdict.set_client_score(1.0f);
  verdict.set_is_phishing(true);

  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _))
      .WillOnce(DoAll(DeleteArg<1>(),
                      InvokeCallbackArgument<2>(true, &verdict)));
  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(
                  Pointee(PartiallyEqualVerdict(verdict)), _))
      .WillOnce(SaveArg<1>(&cb));
  OnPhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_TRUE(cb);

  // Set this back to a normal browser feature extractor since we're using
  // NavigateAndCommit() and it's easier to use the real thing than setting up
  // mock expectations.
  SetFeatureExtractor(new BrowserFeatureExtractor(contents(),
                                                  csd_service_.get()));
  GURL other_phishing_url("http://other_phishing_url.com/bla");
  ExpectPreClassificationChecks(other_phishing_url, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse, &kFalse);
  // We navigate away.  The callback cb should be revoked.
  NavigateAndCommit(other_phishing_url);
  // Wait for the pre-classification checks to finish for other_phishing_url.
  WaitAndCheckPreClassificationChecks();

  ClientSideDetectionService::ClientReportPhishingRequestCallback* cb_other;
  verdict.set_url(other_phishing_url.spec());
  verdict.set_client_score(0.8f);
  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(
                  Pointee(PartiallyEqualVerdict(verdict)), _))
      .WillOnce(DoAll(DeleteArg<0>(),
                      SaveArg<1>(&cb_other),
                      QuitUIMessageLoop()));
  std::vector<GURL> redirect_chain;
  redirect_chain.push_back(other_phishing_url);
  SetRedirectChain(redirect_chain);
  OnPhishingDetectionDone(verdict.SerializeAsString());
  MessageLoop::current()->Run();
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  ASSERT_TRUE(cb_other);

  // We expect that the interstitial is shown for the second phishing URL and
  // not for the first phishing URL.
  SafeBrowsingService::UnsafeResource resource;
  EXPECT_CALL(*sb_service_, DoDisplayBlockingPage(_))
      .WillOnce(SaveArg<0>(&resource));

  cb->Run(phishing_url, true);  // Should have no effect.
  delete cb;
  cb_other->Run(other_phishing_url, true);  // Should show interstitial.
  delete cb_other;

  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(Mock::VerifyAndClear(sb_service_.get()));
  EXPECT_EQ(other_phishing_url, resource.url);
  EXPECT_EQ(other_phishing_url, resource.original_url);
  EXPECT_FALSE(resource.is_subresource);
  EXPECT_EQ(SafeBrowsingService::CLIENT_SIDE_PHISHING_URL,
            resource.threat_type);
  EXPECT_EQ(contents()->GetRenderProcessHost()->id(),
            resource.render_process_host_id);
  EXPECT_EQ(contents()->render_view_host()->routing_id(),
            resource.render_view_id);

  // Make sure the client object will be deleted.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      NewRunnableMethod(
          sb_service_.get(),
          &MockSafeBrowsingService::InvokeOnBlockingPageComplete,
          resource.client));
  // Since the CsdClient object will be deleted on the UI thread I need
  // to run the UI message loop.  Post a task to stop the UI message loop
  // after the client object destructor is called.
  FlushIOMessageLoop();
}

TEST_F(ClientSideDetectionHostTest, OnPhishingDetectionDoneVerdictNotPhishing) {
  // Case 6: renderer sends a verdict string that isn't phishing.
  MockBrowserFeatureExtractor* mock_extractor = new MockBrowserFeatureExtractor(
      contents(),
      csd_service_.get());
  SetFeatureExtractor(mock_extractor);  // The host class takes ownership.

  ClientPhishingRequest verdict;
  verdict.set_url("http://not-phishing.com/");
  verdict.set_client_score(0.1f);
  verdict.set_is_phishing(false);

  EXPECT_CALL(*mock_extractor, ExtractFeatures(_, _, _)).Times(0);
  OnPhishingDetectionDone(verdict.SerializeAsString());
  EXPECT_TRUE(Mock::VerifyAndClear(mock_extractor));
}

TEST_F(ClientSideDetectionHostTest,
       OnPhishingDetectionDoneVerdictNotPhishingButSBMatch) {
  // Case 7: renderer sends a verdict string that isn't phishing but the URL
  // was on the regular phishing or malware lists.
  GURL url("http://not-phishing.com/");
  ClientPhishingRequest verdict;
  verdict.set_url(url.spec());
  verdict.set_client_score(0.1f);
  verdict.set_is_phishing(false);

  // First we have to navigate to the URL to set the unique page ID.
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  SetUnsafeResourceToCurrent();

  EXPECT_CALL(*csd_service_,
              SendClientReportPhishingRequest(
                  Pointee(PartiallyEqualVerdict(verdict)), IsNull()))
      .WillOnce(DoAll(DeleteArg<0>(), QuitUIMessageLoop()));
  std::vector<GURL> redirect_chain;
  redirect_chain.push_back(url);
  SetRedirectChain(redirect_chain);
  OnPhishingDetectionDone(verdict.SerializeAsString());
  MessageLoop::current()->Run();
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
}

TEST_F(ClientSideDetectionHostTest, NavigationCancelsShouldClassifyUrl) {
  // Test that canceling pending should classify requests works as expected.

  GURL first_url("http://first.phishy.url.com");
  // The first few checks are done synchronously so check that they have been
  // done for the first URL.
  ExpectPreClassificationChecks(first_url, &kFalse, &kFalse, &kFalse, NULL,
                                NULL, NULL);
  NavigateAndCommit(first_url);

  // Don't flush the message loop, as we want to navigate to a different
  // url before the final pre-classification checks are run.
  GURL second_url("http://second.url.com/");
  ExpectPreClassificationChecks(second_url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  NavigateAndCommit(second_url);
  WaitAndCheckPreClassificationChecks();
}

TEST_F(ClientSideDetectionHostTest, ShouldClassifyUrl) {
  // Navigate the tab to a page.  We should see a StartPhishingDetection IPC.
  GURL url("http://host.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  const IPC::Message* msg = process()->sink().GetFirstMessageMatching(
      SafeBrowsingMsg_StartPhishingDetection::ID);
  ASSERT_TRUE(msg);
  Tuple1<GURL> actual_url;
  SafeBrowsingMsg_StartPhishingDetection::Read(msg, &actual_url);
  EXPECT_EQ(url, actual_url.a);
  EXPECT_EQ(rvh()->routing_id(), msg->routing_id());
  process()->sink().ClearMessages();

  // Now try an in-page navigation.  This should not trigger an IPC.
  EXPECT_CALL(*csd_service_, IsPrivateIPAddress(_)).Times(0);
  url = GURL("http://host.com/#foo");
  ExpectPreClassificationChecks(url, NULL, NULL, NULL, NULL, NULL, NULL);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();

  msg = process()->sink().GetFirstMessageMatching(
      SafeBrowsingMsg_StartPhishingDetection::ID);
  ASSERT_FALSE(msg);

  // Check that XHTML is supported, in addition to the default HTML type.
  // Note: for this test to work correctly, the new URL must be on the
  // same domain as the previous URL, otherwise it will create a new
  // RenderViewHost that won't have the mime type set.
  url = GURL("http://host.com/xhtml");
  rvh()->set_contents_mime_type("application/xhtml+xml");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  msg = process()->sink().GetFirstMessageMatching(
      SafeBrowsingMsg_StartPhishingDetection::ID);
  ASSERT_TRUE(msg);
  SafeBrowsingMsg_StartPhishingDetection::Read(msg, &actual_url);
  EXPECT_EQ(url, actual_url.a);
  EXPECT_EQ(rvh()->routing_id(), msg->routing_id());
  process()->sink().ClearMessages();

  // Navigate to a new host, which should cause another IPC.
  url = GURL("http://host2.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kFalse);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  msg = process()->sink().GetFirstMessageMatching(
      SafeBrowsingMsg_StartPhishingDetection::ID);
  ASSERT_TRUE(msg);
  SafeBrowsingMsg_StartPhishingDetection::Read(msg, &actual_url);
  EXPECT_EQ(url, actual_url.a);
  EXPECT_EQ(rvh()->routing_id(), msg->routing_id());
  process()->sink().ClearMessages();

  // If the mime type is not one that we support, no IPC should be triggered.
  // Note: for this test to work correctly, the new URL must be on the
  // same domain as the previous URL, otherwise it will create a new
  // RenderViewHost that won't have the mime type set.
  url = GURL("http://host2.com/image.jpg");
  rvh()->set_contents_mime_type("image/jpeg");
  ExpectPreClassificationChecks(url, NULL, NULL, NULL, NULL, NULL, NULL);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  msg = process()->sink().GetFirstMessageMatching(
      SafeBrowsingMsg_StartPhishingDetection::ID);
  ASSERT_FALSE(msg);

  // If IsPrivateIPAddress returns true, no IPC should be triggered.
  url = GURL("http://host3.com/");
  ExpectPreClassificationChecks(url, &kTrue, NULL, NULL, NULL, NULL, NULL);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  msg = process()->sink().GetFirstMessageMatching(
      SafeBrowsingMsg_StartPhishingDetection::ID);
  ASSERT_FALSE(msg);

  // If the tab is incognito there should be no IPC.  Also, we shouldn't
  // even check the csd-whitelist.
  url = GURL("http://host4.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kTrue, NULL, NULL, NULL, NULL);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  msg = process()->sink().GetFirstMessageMatching(
      SafeBrowsingMsg_StartPhishingDetection::ID);
  ASSERT_FALSE(msg);

  // If the URL is on the csd whitelist, no IPC should be triggered.
  url = GURL("http://host5.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kTrue, NULL, NULL,
                                NULL);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  msg = process()->sink().GetFirstMessageMatching(
      SafeBrowsingMsg_StartPhishingDetection::ID);
  ASSERT_FALSE(msg);

  // If item is in the cache but it isn't valid, we will classify regardless
  // of whether we are over the reporting limit.
  url = GURL("http://host6.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse, &kTrue,
                                NULL);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  msg = process()->sink().GetFirstMessageMatching(
      SafeBrowsingMsg_StartPhishingDetection::ID);
  ASSERT_TRUE(msg);
  SafeBrowsingMsg_StartPhishingDetection::Read(msg, &actual_url);
  EXPECT_EQ(url, actual_url.a);
  EXPECT_EQ(rvh()->routing_id(), msg->routing_id());
  process()->sink().ClearMessages();

  // If the url isn't in the cache and we are over the reporting limit, we
  // don't do classification.
  url = GURL("http://host7.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kFalse,
                                &kFalse, &kTrue);
  NavigateAndCommit(url);
  WaitAndCheckPreClassificationChecks();
  msg = process()->sink().GetFirstMessageMatching(
      SafeBrowsingMsg_StartPhishingDetection::ID);
  ASSERT_FALSE(msg);

  // If result is cached, we will try and display the blocking page directly
  // with no start classification message.
  url = GURL("http://host8.com/");
  ExpectPreClassificationChecks(url, &kFalse, &kFalse, &kFalse, &kTrue, NULL,
                                NULL);

  SafeBrowsingService::UnsafeResource resource;
  EXPECT_CALL(*sb_service_, DoDisplayBlockingPage(_))
      .WillOnce(SaveArg<0>(&resource));

  NavigateAndCommit(url);
  // Wait for CheckCsdWhitelist to be called on the IO thread.
  FlushIOMessageLoop();
  // Wait for CheckCache() to be called on the UI thread.
  MessageLoop::current()->RunAllPending();
  // Now we check that all expected functions were indeed called on the two
  // service objects.
  EXPECT_TRUE(Mock::VerifyAndClear(csd_service_.get()));
  EXPECT_TRUE(Mock::VerifyAndClear(sb_service_.get()));
  EXPECT_EQ(url, resource.url);
  EXPECT_EQ(url, resource.original_url);
  delete resource.client;
  msg = process()->sink().GetFirstMessageMatching(
      SafeBrowsingMsg_StartPhishingDetection::ID);
  ASSERT_FALSE(msg);
}

}  // namespace safe_browsing
