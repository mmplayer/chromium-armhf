// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/extensions_helper.h"
#include "chrome/browser/sync/test/integration/performance/sync_timing_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

using extensions_helper::AllProfilesHaveSameExtensions;
using extensions_helper::AllProfilesHaveSameExtensionsAsVerifier;
using extensions_helper::DisableExtension;
using extensions_helper::EnableExtension;
using extensions_helper::GetInstalledExtensions;
using extensions_helper::InstallExtension;
using extensions_helper::InstallExtensionsPendingForSync;
using extensions_helper::IsExtensionEnabled;
using extensions_helper::UninstallExtension;

// TODO(braffert): Replicate these tests for apps.

static const int kNumExtensions = 150;

class ExtensionsSyncPerfTest : public SyncTest {
 public:
  ExtensionsSyncPerfTest()
      : SyncTest(TWO_CLIENT),
        extension_number_(0) {}

  // Adds |num_extensions| new unique extensions to |profile|.
  void AddExtensions(int profile, int num_extensions);

  // Updates the enabled/disabled state for all extensions in |profile|.
  void UpdateExtensions(int profile);

  // Uninstalls all currently installed extensions from |profile|.
  void RemoveExtensions(int profile);

  // Returns the number of currently installed extensions for |profile|.
  int GetExtensionCount(int profile);

 private:
  int extension_number_;
  DISALLOW_COPY_AND_ASSIGN(ExtensionsSyncPerfTest);
};

void ExtensionsSyncPerfTest::AddExtensions(int profile, int num_extensions) {
  for (int i = 0; i < num_extensions; ++i) {
    InstallExtension(GetProfile(profile), extension_number_++);
  }
}

void ExtensionsSyncPerfTest::UpdateExtensions(int profile) {
  std::vector<int> extensions = GetInstalledExtensions(GetProfile(profile));
  for (std::vector<int>::iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    if (IsExtensionEnabled(GetProfile(profile), *it)) {
      DisableExtension(GetProfile(profile), *it);
    } else {
      EnableExtension(GetProfile(profile), *it);
    }
  }
}

int ExtensionsSyncPerfTest::GetExtensionCount(int profile) {
  return GetInstalledExtensions(GetProfile(profile)).size();
}

void ExtensionsSyncPerfTest::RemoveExtensions(int profile) {
  std::vector<int> extensions = GetInstalledExtensions(GetProfile(profile));
  for (std::vector<int>::iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    UninstallExtension(GetProfile(profile), *it);
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionsSyncPerfTest, P0) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  int num_default_extensions = GetExtensionCount(0);
  int expected_extension_count = num_default_extensions + kNumExtensions;

  // TCM ID - 7563874.
  AddExtensions(0, kNumExtensions);
  base::TimeDelta dt =
      SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  InstallExtensionsPendingForSync(GetProfile(1));
  ASSERT_EQ(expected_extension_count, GetExtensionCount(1));
  SyncTimingHelper::PrintResult("extensions", "add_extensions", dt);

  // TCM ID - 7655397.
  UpdateExtensions(0);
  dt = SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(expected_extension_count, GetExtensionCount(1));
  SyncTimingHelper::PrintResult("extensions", "update_extensions", dt);

  // TCM ID - 7567721.
  RemoveExtensions(0);
  dt = SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(num_default_extensions, GetExtensionCount(1));
  SyncTimingHelper::PrintResult("extensions", "delete_extensions", dt);
}
