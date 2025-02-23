// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/performance/sync_timing_helper.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

using sessions_helper::GetLocalSession;
using sessions_helper::GetSessionData;
using sessions_helper::OpenMultipleTabs;
using sessions_helper::SyncedSessionVector;
using sessions_helper::SessionWindowMap;
using sessions_helper::WaitForTabsToLoad;

static const int kNumTabs = 150;

class SessionsSyncPerfTest: public SyncTest {
 public:
  SessionsSyncPerfTest() : SyncTest(TWO_CLIENT), url_number_(0) {}

  // Opens |num_tabs| new tabs on |profile|.
  void AddTabs(int profile, int num_tabs);

  // Update all tabs in |profile| by visiting a new URL.
  void UpdateTabs(int profile);

  // Close all tabs in |profile|.
  void RemoveTabs(int profile);

  // Returns the number of open tabs in all sessions (local + foreign) for
  // |profile|.  Returns -1 on failure.
  int GetTabCount(int profile);

 private:
  // Returns a new unique URL.
  GURL NextURL();

  // Returns a unique URL according to the integer |n|.
  GURL IntToURL(int n);

  int url_number_;
  DISALLOW_COPY_AND_ASSIGN(SessionsSyncPerfTest);
};

void SessionsSyncPerfTest::AddTabs(int profile, int num_tabs) {
  std::vector<GURL> urls;
  for (int i = 0; i < num_tabs; ++i) {
    urls.push_back(NextURL());
  }
  OpenMultipleTabs(profile, urls);
}

void SessionsSyncPerfTest::UpdateTabs(int profile) {
  Browser* browser = GetBrowser(profile);
  GURL url;
  std::vector<GURL> urls;
  for (int i = 0; i < browser->tab_count(); ++i) {
    browser->SelectNumberedTab(i);
    url = NextURL();
    browser->OpenURL(
        OpenURLParams(url, GURL("http://localhost"), CURRENT_TAB,
        content::PageTransitionFromInt(0), false));
    urls.push_back(url);
  }
  WaitForTabsToLoad(profile, urls);
}

void SessionsSyncPerfTest::RemoveTabs(int profile) {
  GetBrowser(profile)->CloseAllTabs();
}

int SessionsSyncPerfTest::GetTabCount(int profile) {
  int tab_count = 0;
  const browser_sync::SyncedSession* local_session;
  SyncedSessionVector sessions;

  if (!GetLocalSession(profile, &local_session)) {
    VLOG(1) << "GetLocalSession returned false";
    return -1;
  }

  if (!GetSessionData(profile, &sessions)) {
    // Foreign session data may be empty.  In this case we only count tabs in
    // the local session.
    VLOG(1) << "GetSessionData returned false";
  }

  sessions.push_back(local_session);
  for (SyncedSessionVector::const_iterator it = sessions.begin();
       it != sessions.end(); ++it) {
    for (SessionWindowMap::const_iterator win_it = (*it)->windows.begin();
         win_it != (*it)->windows.end();
         ++win_it) {
      tab_count += win_it->second->tabs.size();
    }
  }
  return tab_count;
}

GURL SessionsSyncPerfTest::NextURL() {
  return IntToURL(url_number_++);
}

GURL SessionsSyncPerfTest::IntToURL(int n) {
  return GURL(StringPrintf("http://localhost/%d", n));
}

// TODO(lipalani): Re-enable after crbug.com/96921 is fixed.
IN_PROC_BROWSER_TEST_F(SessionsSyncPerfTest, DISABLED_P0) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  AddTabs(0, kNumTabs);
  base::TimeDelta dt =
      SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumTabs, GetTabCount(0));
  ASSERT_EQ(kNumTabs, GetTabCount(1));
  SyncTimingHelper::PrintResult("tabs", "add_tabs", dt);

  UpdateTabs(0);
  dt = SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumTabs, GetTabCount(0));
  ASSERT_EQ(kNumTabs, GetTabCount(1));
  SyncTimingHelper::PrintResult("tabs", "update_tabs", dt);

  RemoveTabs(0);
  dt = SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  // New tab page remains open on profile 0 after closing all tabs.
  ASSERT_EQ(1, GetTabCount(0));
  ASSERT_EQ(0, GetTabCount(1));
  SyncTimingHelper::PrintResult("tabs", "delete_tabs", dt);
}
