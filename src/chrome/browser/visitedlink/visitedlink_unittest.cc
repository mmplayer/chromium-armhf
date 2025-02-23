// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>
#include <string>
#include <cstdio>

#include "base/message_loop.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/shared_memory.h"
#include "base/string_util.h"
#include "chrome/browser/visitedlink/visitedlink_master.h"
#include "chrome/browser/visitedlink/visitedlink_event_listener.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/visitedlink_slave.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/browser_render_process_host.h"
#include "content/common/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// a nice long URL that we can append numbers to to get new URLs
const char g_test_prefix[] =
  "http://www.google.com/products/foo/index.html?id=45028640526508376&seq=";
const int g_test_count = 1000;

// Returns a test URL for index |i|
GURL TestURL(int i) {
  return GURL(StringPrintf("%s%d", g_test_prefix, i));
}

std::vector<VisitedLinkSlave*> g_slaves;

}  // namespace

class TrackingVisitedLinkEventListener : public VisitedLinkMaster::Listener {
 public:
  TrackingVisitedLinkEventListener()
      : reset_count_(0),
        add_count_(0) {}

  virtual void NewTable(base::SharedMemory* table) {
    if (table) {
      for (std::vector<VisitedLinkSlave>::size_type i = 0;
           i < g_slaves.size(); i++) {
        base::SharedMemoryHandle new_handle = base::SharedMemory::NULLHandle();
        table->ShareToProcess(base::GetCurrentProcessHandle(), &new_handle);
        g_slaves[i]->OnUpdateVisitedLinks(new_handle);
      }
    }
  }
  virtual void Add(VisitedLinkCommon::Fingerprint) { add_count_++; }
  virtual void Reset() { reset_count_++; }

  void SetUp() {
    reset_count_ = 0;
    add_count_ = 0;
  }

  int reset_count() const { return reset_count_; }
  int add_count() const { return add_count_; }

 private:
  int reset_count_;
  int add_count_;
};

class VisitedLinkTest : public testing::Test {
 protected:
  VisitedLinkTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_) {}
  // Initialize the history system. This should be called before InitVisited().
  bool InitHistory() {
    history_service_ = new HistoryService;
    return history_service_->Init(history_dir_, NULL);
  }

  // Initializes the visited link objects. Pass in the size that you want a
  // freshly created table to be. 0 means use the default.
  //
  // |suppress_rebuild| is set when we're not testing rebuilding, see
  // the VisitedLinkMaster constructor.
  bool InitVisited(int initial_size, bool suppress_rebuild) {
    // Initialize the visited link system.
    master_.reset(new VisitedLinkMaster(&listener_, history_service_,
                                        suppress_rebuild, visited_file_,
                                        initial_size));
    return master_->Init();
  }

  // May be called multiple times (some tests will do this to clear things,
  // and TearDown will do this to make sure eveything is shiny before quitting.
  void ClearDB() {
    if (master_.get())
      master_.reset(NULL);

    if (history_service_.get()) {
      history_service_->SetOnBackendDestroyTask(new MessageLoop::QuitTask);
      history_service_->Cleanup();
      history_service_ = NULL;

      // Wait for the backend class to terminate before deleting the files and
      // moving to the next test. Note: if this never terminates, somebody is
      // probably leaking a reference to the history backend, so it never calls
      // our destroy task.
      MessageLoop::current()->Run();
    }
  }

  // Loads the database from disk and makes sure that the same URLs are present
  // as were generated by TestIO_Create(). This also checks the URLs with a
  // slave to make sure it reads the data properly.
  void Reload() {
    // Clean up after our caller, who may have left the database open.
    ClearDB();

    ASSERT_TRUE(InitHistory());
    ASSERT_TRUE(InitVisited(0, true));
    master_->DebugValidate();

    // check that the table has the proper number of entries
    int used_count = master_->GetUsedCount();
    ASSERT_EQ(used_count, g_test_count);

    // Create a slave database.
    VisitedLinkSlave slave;
    base::SharedMemoryHandle new_handle = base::SharedMemory::NULLHandle();
    master_->shared_memory()->ShareToProcess(
        base::GetCurrentProcessHandle(), &new_handle);
    slave.OnUpdateVisitedLinks(new_handle);
    g_slaves.push_back(&slave);

    bool found;
    for (int i = 0; i < g_test_count; i++) {
      GURL cur = TestURL(i);
      found = master_->IsVisited(cur);
      EXPECT_TRUE(found) << "URL " << i << "not found in master.";

      found = slave.IsVisited(cur);
      EXPECT_TRUE(found) << "URL " << i << "not found in slave.";
    }

    // test some random URL so we know that it returns false sometimes too
    found = master_->IsVisited(GURL("http://unfound.site/"));
    ASSERT_FALSE(found);
    found = slave.IsVisited(GURL("http://unfound.site/"));
    ASSERT_FALSE(found);

    master_->DebugValidate();

    g_slaves.clear();
  }

  // testing::Test
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    history_dir_ = temp_dir_.path().AppendASCII("VisitedLinkTest");
    ASSERT_TRUE(file_util::CreateDirectory(history_dir_));

    visited_file_ = history_dir_.Append(FILE_PATH_LITERAL("VisitedLinks"));
    listener_.SetUp();
  }

  virtual void TearDown() {
    ClearDB();
  }

  ScopedTempDir temp_dir_;

  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;

  // Filenames for the services;
  FilePath history_dir_;
  FilePath visited_file_;

  scoped_ptr<VisitedLinkMaster> master_;
  scoped_refptr<HistoryService> history_service_;
  TrackingVisitedLinkEventListener listener_;
};

// This test creates and reads some databases to make sure the data is
// preserved throughout those operations.
TEST_F(VisitedLinkTest, DatabaseIO) {
  ASSERT_TRUE(InitHistory());
  ASSERT_TRUE(InitVisited(0, true));

  for (int i = 0; i < g_test_count; i++)
    master_->AddURL(TestURL(i));

  // Test that the database was written properly
  Reload();
}

// Checks that we can delete things properly when there are collisions.
TEST_F(VisitedLinkTest, Delete) {
  static const int32 kInitialSize = 17;
  ASSERT_TRUE(InitHistory());
  ASSERT_TRUE(InitVisited(kInitialSize, true));

  // Add a cluster from 14-17 wrapping around to 0. These will all hash to the
  // same value.
  const VisitedLinkCommon::Fingerprint kFingerprint0 = kInitialSize * 0 + 14;
  const VisitedLinkCommon::Fingerprint kFingerprint1 = kInitialSize * 1 + 14;
  const VisitedLinkCommon::Fingerprint kFingerprint2 = kInitialSize * 2 + 14;
  const VisitedLinkCommon::Fingerprint kFingerprint3 = kInitialSize * 3 + 14;
  const VisitedLinkCommon::Fingerprint kFingerprint4 = kInitialSize * 4 + 14;
  master_->AddFingerprint(kFingerprint0, false);  // @14
  master_->AddFingerprint(kFingerprint1, false);  // @15
  master_->AddFingerprint(kFingerprint2, false);  // @16
  master_->AddFingerprint(kFingerprint3, false);  // @0
  master_->AddFingerprint(kFingerprint4, false);  // @1

  // Deleting 14 should move the next value up one slot (we do not specify an
  // order).
  EXPECT_EQ(kFingerprint3, master_->hash_table_[0]);
  master_->DeleteFingerprint(kFingerprint3, false);
  VisitedLinkCommon::Fingerprint zero_fingerprint = 0;
  EXPECT_EQ(zero_fingerprint, master_->hash_table_[1]);
  EXPECT_NE(zero_fingerprint, master_->hash_table_[0]);

  // Deleting the other four should leave the table empty.
  master_->DeleteFingerprint(kFingerprint0, false);
  master_->DeleteFingerprint(kFingerprint1, false);
  master_->DeleteFingerprint(kFingerprint2, false);
  master_->DeleteFingerprint(kFingerprint4, false);

  EXPECT_EQ(0, master_->used_items_);
  for (int i = 0; i < kInitialSize; i++)
    EXPECT_EQ(zero_fingerprint, master_->hash_table_[i]) <<
        "Hash table has values in it.";
}

// When we delete more than kBigDeleteThreshold we trigger different behavior
// where the entire file is rewritten.
TEST_F(VisitedLinkTest, BigDelete) {
  ASSERT_TRUE(InitHistory());
  ASSERT_TRUE(InitVisited(16381, true));

  // Add the base set of URLs that won't be deleted.
  // Reload() will test for these.
  for (int32 i = 0; i < g_test_count; i++)
    master_->AddURL(TestURL(i));

  // Add more URLs than necessary to trigger this case.
  const int kTestDeleteCount = VisitedLinkMaster::kBigDeleteThreshold + 2;
  std::set<GURL> urls_to_delete;
  for (int32 i = g_test_count; i < g_test_count + kTestDeleteCount; i++) {
    GURL url(TestURL(i));
    master_->AddURL(url);
    urls_to_delete.insert(url);
  }

  master_->DeleteURLs(urls_to_delete);
  master_->DebugValidate();

  Reload();
}

TEST_F(VisitedLinkTest, DeleteAll) {
  ASSERT_TRUE(InitHistory());
  ASSERT_TRUE(InitVisited(0, true));

  {
    VisitedLinkSlave slave;
    base::SharedMemoryHandle new_handle = base::SharedMemory::NULLHandle();
    master_->shared_memory()->ShareToProcess(
        base::GetCurrentProcessHandle(), &new_handle);
    slave.OnUpdateVisitedLinks(new_handle);
    g_slaves.push_back(&slave);

    // Add the test URLs.
    for (int i = 0; i < g_test_count; i++) {
      master_->AddURL(TestURL(i));
      ASSERT_EQ(i + 1, master_->GetUsedCount());
    }
    master_->DebugValidate();

    // Make sure the slave picked up the adds.
    for (int i = 0; i < g_test_count; i++)
      EXPECT_TRUE(slave.IsVisited(TestURL(i)));

    // Clear the table and make sure the slave picked it up.
    master_->DeleteAllURLs();
    EXPECT_EQ(0, master_->GetUsedCount());
    for (int i = 0; i < g_test_count; i++) {
      EXPECT_FALSE(master_->IsVisited(TestURL(i)));
      EXPECT_FALSE(slave.IsVisited(TestURL(i)));
    }

    // Close the database.
    g_slaves.clear();
    ClearDB();
  }

  // Reopen and validate.
  ASSERT_TRUE(InitHistory());
  ASSERT_TRUE(InitVisited(0, true));
  master_->DebugValidate();
  EXPECT_EQ(0, master_->GetUsedCount());
  for (int i = 0; i < g_test_count; i++)
    EXPECT_FALSE(master_->IsVisited(TestURL(i)));
}

// This tests that the master correctly resizes its tables when it gets too
// full, notifies its slaves of the change, and updates the disk.
TEST_F(VisitedLinkTest, Resizing) {
  // Create a very small database.
  const int32 initial_size = 17;
  ASSERT_TRUE(InitHistory());
  ASSERT_TRUE(InitVisited(initial_size, true));

  // ...and a slave
  VisitedLinkSlave slave;
  base::SharedMemoryHandle new_handle = base::SharedMemory::NULLHandle();
  master_->shared_memory()->ShareToProcess(
      base::GetCurrentProcessHandle(), &new_handle);
  slave.OnUpdateVisitedLinks(new_handle);
  g_slaves.push_back(&slave);

  int32 used_count = master_->GetUsedCount();
  ASSERT_EQ(used_count, 0);

  for (int i = 0; i < g_test_count; i++) {
    master_->AddURL(TestURL(i));
    used_count = master_->GetUsedCount();
    ASSERT_EQ(i + 1, used_count);
  }

  // Verify that the table got resized sufficiently.
  int32 table_size;
  VisitedLinkCommon::Fingerprint* table;
  master_->GetUsageStatistics(&table_size, &table);
  used_count = master_->GetUsedCount();
  ASSERT_GT(table_size, used_count);
  ASSERT_EQ(used_count, g_test_count) <<
                "table count doesn't match the # of things we added";

  // Verify that the slave got the resize message and has the same
  // table information.
  int32 child_table_size;
  VisitedLinkCommon::Fingerprint* child_table;
  slave.GetUsageStatistics(&child_table_size, &child_table);
  ASSERT_EQ(table_size, child_table_size);
  for (int32 i = 0; i < table_size; i++) {
    ASSERT_EQ(table[i], child_table[i]);
  }

  master_->DebugValidate();
  g_slaves.clear();

  // This tests that the file is written correctly by reading it in using
  // a new database.
  Reload();
}

// Tests that if the database doesn't exist, it will be rebuilt from history.
TEST_F(VisitedLinkTest, Rebuild) {
  ASSERT_TRUE(InitHistory());

  // Add half of our URLs to history. This needs to be done before we
  // initialize the visited link DB.
  int history_count = g_test_count / 2;
  for (int i = 0; i < history_count; i++)
    history_service_->AddPage(TestURL(i), history::SOURCE_BROWSED);

  // Initialize the visited link DB. Since the visited links file doesn't exist
  // and we don't suppress history rebuilding, this will load from history.
  ASSERT_TRUE(InitVisited(0, false));

  // While the table is rebuilding, add the rest of the URLs to the visited
  // link system. This isn't guaranteed to happen during the rebuild, so we
  // can't be 100% sure we're testing the right thing, but in practice is.
  // All the adds above will generally take some time queuing up on the
  // history thread, and it will take a while to catch up to actually
  // processing the rebuild that has queued behind it. We will generally
  // finish adding all of the URLs before it has even found the first URL.
  for (int i = history_count; i < g_test_count; i++)
    master_->AddURL(TestURL(i));

  // Add one more and then delete it.
  master_->AddURL(TestURL(g_test_count));
  std::set<GURL> deleted_urls;
  deleted_urls.insert(TestURL(g_test_count));
  master_->DeleteURLs(deleted_urls);

  // Wait for the rebuild to complete. The task will terminate the message
  // loop when the rebuild is done. There's no chance that the rebuild will
  // complete before we set the task because the rebuild completion message
  // is posted to the message loop; until we Run() it, rebuild can not
  // complete.
  master_->set_rebuild_complete_task(new MessageLoop::QuitTask);
  MessageLoop::current()->Run();

  // Test that all URLs were written to the database properly.
  Reload();

  // Make sure the extra one was *not* written (Reload won't test this).
  EXPECT_FALSE(master_->IsVisited(TestURL(g_test_count)));
}

// Test that importing a large number of URLs will work
TEST_F(VisitedLinkTest, BigImport) {
  ASSERT_TRUE(InitHistory());
  ASSERT_TRUE(InitVisited(0, false));

  // Before the table rebuilds, add a large number of URLs
  int total_count = VisitedLinkMaster::kDefaultTableSize + 10;
  for (int i = 0; i < total_count; i++)
    master_->AddURL(TestURL(i));

  // Wait for the rebuild to complete.
  master_->set_rebuild_complete_task(new MessageLoop::QuitTask);
  MessageLoop::current()->Run();

  // Ensure that the right number of URLs are present
  int used_count = master_->GetUsedCount();
  ASSERT_EQ(used_count, total_count);
}

TEST_F(VisitedLinkTest, Listener) {
  ASSERT_TRUE(InitHistory());
  ASSERT_TRUE(InitVisited(0, true));

  // Add test URLs.
  for (int i = 0; i < g_test_count; i++) {
    master_->AddURL(TestURL(i));
    ASSERT_EQ(i + 1, master_->GetUsedCount());
  }

  std::set<GURL> deleted_urls;
  deleted_urls.insert(TestURL(0));
  // Delete an URL.
  master_->DeleteURLs(deleted_urls);
  // ... and all of the remaining ones.
  master_->DeleteAllURLs();

  // Verify that VisitedLinkMaster::Listener::Add was called for each added URL.
  EXPECT_EQ(g_test_count, listener_.add_count());
  // Verify that VisitedLinkMaster::Listener::Reset was called both when one and
  // all URLs are deleted.
  EXPECT_EQ(2, listener_.reset_count());
}

class VisitCountingProfile : public TestingProfile {
 public:
  VisitCountingProfile()
      : add_count_(0),
        add_event_count_(0),
        reset_event_count_(0),
        event_listener_(ALLOW_THIS_IN_INITIALIZER_LIST(
            new VisitedLinkEventListener(this))) {}

  virtual VisitedLinkMaster* GetVisitedLinkMaster() {
    if (!visited_link_master_.get()) {
      visited_link_master_.reset(
          new VisitedLinkMaster(event_listener_.get(), this));
      visited_link_master_->Init();
    }
    return visited_link_master_.get();
  }

  void CountAddEvent(int by) {
    add_count_ += by;
    add_event_count_++;
  }

  void CountResetEvent() {
    reset_event_count_++;
  }

  VisitedLinkMaster* master() const { return visited_link_master_.get(); }
  int add_count() const { return add_count_; }
  int add_event_count() const { return add_event_count_; }
  int reset_event_count() const { return reset_event_count_; }

 private:
  int add_count_;
  int add_event_count_;
  int reset_event_count_;
  scoped_ptr<VisitedLinkEventListener> event_listener_;
  scoped_ptr<VisitedLinkMaster> visited_link_master_;
};

// Stub out as little as possible, borrowing from BrowserRenderProcessHost.
class VisitRelayingRenderProcessHost : public BrowserRenderProcessHost {
 public:
  explicit VisitRelayingRenderProcessHost(
      content::BrowserContext* browser_context)
          : BrowserRenderProcessHost(browser_context) {
    NotificationService::current()->Notify(
        content::NOTIFICATION_RENDERER_PROCESS_CREATED,
        Source<RenderProcessHost>(this), NotificationService::NoDetails());
  }
  virtual ~VisitRelayingRenderProcessHost() {
    NotificationService::current()->Notify(
        content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
        Source<RenderProcessHost>(this), NotificationService::NoDetails());
  }

  virtual bool Init(bool is_accessibility_enabled) {
    return true;
  }

  virtual void CancelResourceRequests(int render_widget_id) {
  }

  virtual void CrossSiteSwapOutACK(const ViewMsg_SwapOut_Params& params) {
  }

  virtual bool WaitForPaintMsg(int render_widget_id,
                               const base::TimeDelta& max_delay,
                               IPC::Message* msg) {
    return false;
  }

  virtual bool Send(IPC::Message* msg) {
    VisitCountingProfile* counting_profile =
        static_cast<VisitCountingProfile*>(
            Profile::FromBrowserContext(browser_context()));

    if (msg->type() == ChromeViewMsg_VisitedLink_Add::ID) {
      void* iter = NULL;
      std::vector<uint64> fingerprints;
      CHECK(IPC::ReadParam(msg, &iter, &fingerprints));
      counting_profile->CountAddEvent(fingerprints.size());
    } else if (msg->type() == ChromeViewMsg_VisitedLink_Reset::ID) {
      counting_profile->CountResetEvent();
    }

    delete msg;
    return true;
  }

  virtual void SetBackgrounded(bool backgrounded) {
    backgrounded_ = backgrounded;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VisitRelayingRenderProcessHost);
};

class VisitedLinkRenderProcessHostFactory
    : public RenderProcessHostFactory {
 public:
  VisitedLinkRenderProcessHostFactory()
      : RenderProcessHostFactory() {}
  virtual RenderProcessHost* CreateRenderProcessHost(
      content::BrowserContext* browser_context) const OVERRIDE {
    return new VisitRelayingRenderProcessHost(browser_context);
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(VisitedLinkRenderProcessHostFactory);
};

class VisitedLinkEventsTest : public ChromeRenderViewHostTestHarness {
 public:
  VisitedLinkEventsTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_) {}
  virtual ~VisitedLinkEventsTest() {}
  virtual void SetFactoryMode() {}
  virtual void SetUp() {
    SetFactoryMode();
    rvh_factory_.set_render_process_host_factory(&vc_rph_factory_);
    browser_context_.reset(new VisitCountingProfile());
    ChromeRenderViewHostTestHarness::SetUp();
  }

  VisitCountingProfile* profile() const {
    return static_cast<VisitCountingProfile*>(browser_context_.get());
  }

  void WaitForCoalescense() {
    // Let the timer fire.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                            new MessageLoop::QuitTask(), 110);
    MessageLoop::current()->Run();
  }

 protected:
  VisitedLinkRenderProcessHostFactory vc_rph_factory_;

 private:
  BrowserThread ui_thread_;
  BrowserThread file_thread_;

  DISALLOW_COPY_AND_ASSIGN(VisitedLinkEventsTest);
};

TEST_F(VisitedLinkEventsTest, Coalescense) {
  // add some URLs to master.
  VisitedLinkMaster* master = profile()->GetVisitedLinkMaster();
  // Add a few URLs.
  master->AddURL(GURL("http://acidtests.org/"));
  master->AddURL(GURL("http://google.com/"));
  master->AddURL(GURL("http://chromium.org/"));
  // Just for kicks, add a duplicate URL. This shouldn't increase the resulting
  master->AddURL(GURL("http://acidtests.org/"));

  // Make sure that coalescing actually occurs. There should be no links or
  // events received by the renderer.
  EXPECT_EQ(0, profile()->add_count());
  EXPECT_EQ(0, profile()->add_event_count());

  WaitForCoalescense();

  // We now should have 3 entries added in 1 event.
  EXPECT_EQ(3, profile()->add_count());
  EXPECT_EQ(1, profile()->add_event_count());

  // Test whether the coalescing continues by adding a few more URLs.
  master->AddURL(GURL("http://google.com/chrome/"));
  master->AddURL(GURL("http://webkit.org/"));
  master->AddURL(GURL("http://acid3.acidtests.org/"));

  WaitForCoalescense();

  // We should have 6 entries added in 2 events.
  EXPECT_EQ(6, profile()->add_count());
  EXPECT_EQ(2, profile()->add_event_count());

  // Test whether duplicate entries produce add events.
  master->AddURL(GURL("http://acidtests.org/"));

  WaitForCoalescense();

  // We should have no change in results.
  EXPECT_EQ(6, profile()->add_count());
  EXPECT_EQ(2, profile()->add_event_count());

  // Ensure that the coalescing does not resume after resetting.
  master->AddURL(GURL("http://build.chromium.org/"));
  master->DeleteAllURLs();

  WaitForCoalescense();

  // We should have no change in results except for one new reset event.
  EXPECT_EQ(6, profile()->add_count());
  EXPECT_EQ(2, profile()->add_event_count());
  EXPECT_EQ(1, profile()->reset_event_count());
}

TEST_F(VisitedLinkEventsTest, Basics) {
  VisitedLinkMaster* master = profile()->GetVisitedLinkMaster();
  rvh()->CreateRenderView(string16());

  // Add a few URLs.
  master->AddURL(GURL("http://acidtests.org/"));
  master->AddURL(GURL("http://google.com/"));
  master->AddURL(GURL("http://chromium.org/"));

  WaitForCoalescense();

  // We now should have 1 add event.
  EXPECT_EQ(1, profile()->add_event_count());
  EXPECT_EQ(0, profile()->reset_event_count());

  master->DeleteAllURLs();

  WaitForCoalescense();

  // We should have no change in add results, plus one new reset event.
  EXPECT_EQ(1, profile()->add_event_count());
  EXPECT_EQ(1, profile()->reset_event_count());
}

TEST_F(VisitedLinkEventsTest, TabVisibility) {
  VisitedLinkMaster* master = profile()->GetVisitedLinkMaster();
  rvh()->CreateRenderView(string16());

  // Simulate tab becoming inactive.
  rvh()->WasHidden();

  // Add a few URLs.
  master->AddURL(GURL("http://acidtests.org/"));
  master->AddURL(GURL("http://google.com/"));
  master->AddURL(GURL("http://chromium.org/"));

  WaitForCoalescense();

  // We shouldn't have any events.
  EXPECT_EQ(0, profile()->add_event_count());
  EXPECT_EQ(0, profile()->reset_event_count());

  // Simulate the tab becoming active.
  rvh()->WasRestored();

  // We should now have 3 add events, still no reset events.
  EXPECT_EQ(1, profile()->add_event_count());
  EXPECT_EQ(0, profile()->reset_event_count());

  // Deactivate the tab again.
  rvh()->WasHidden();

  // Add a bunch of URLs (over 50) to exhaust the link event buffer.
  for (int i = 0; i < 100; i++)
    master->AddURL(TestURL(i));

  WaitForCoalescense();

  // Again, no change in events until tab is active.
  EXPECT_EQ(1, profile()->add_event_count());
  EXPECT_EQ(0, profile()->reset_event_count());

  // Activate the tab.
  rvh()->WasRestored();

  // We should have only one more reset event.
  EXPECT_EQ(1, profile()->add_event_count());
  EXPECT_EQ(1, profile()->reset_event_count());
}
