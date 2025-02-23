// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/site_instance.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// make the test a PlatformTest to setup autorelease pools properly on mac
class ExtensionProcessManagerTest : public testing::Test {
 public:
  static void SetUpTestCase() {
    ExtensionErrorReporter::Init(false);  // no noisy errors
  }

  virtual void SetUp() {
    ExtensionErrorReporter::GetInstance()->ClearErrors();
  }
};

}  // namespace

// Test that extensions get grouped in the right SiteInstance (and therefore
// process) based on their URLs.
TEST_F(ExtensionProcessManagerTest, ProcessGrouping) {
  // Extensions in different profiles should always be different SiteInstances.
  // Note: we don't initialize these, since we're not testing that
  // functionality.  This means we can get away with a NULL UserScriptMaster.
  TestingProfile profile1;
  scoped_ptr<ExtensionProcessManager> manager1(
      ExtensionProcessManager::Create(&profile1));

  TestingProfile profile2;
  scoped_ptr<ExtensionProcessManager> manager2(
      ExtensionProcessManager::Create(&profile2));

  // Extensions with common origins ("scheme://id/") should be grouped in the
  // same SiteInstance.
  GURL ext1_url1("chrome-extension://ext1_id/index.html");
  GURL ext1_url2("chrome-extension://ext1_id/monkey/monkey.html");
  GURL ext2_url1("chrome-extension://ext2_id/index.html");

  scoped_refptr<SiteInstance> site11 =
      manager1->GetSiteInstanceForURL(ext1_url1);
  scoped_refptr<SiteInstance> site12 =
      manager1->GetSiteInstanceForURL(ext1_url2);
  EXPECT_EQ(site11, site12);

  scoped_refptr<SiteInstance> site21 =
      manager1->GetSiteInstanceForURL(ext2_url1);
  EXPECT_NE(site11, site21);

  scoped_refptr<SiteInstance> other_profile_site =
      manager2->GetSiteInstanceForURL(ext1_url1);
  EXPECT_NE(site11, other_profile_site);
}
