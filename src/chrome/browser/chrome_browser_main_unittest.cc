// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/browser/content_browser_client.h"
#include "content/common/main_function_params.h"
#include "content/common/sandbox_init_wrapper.h"
#include "net/socket/client_socket_pool_base.h"
#include "testing/gtest/include/gtest/gtest.h"

class BrowserMainTest : public testing::Test {
 public:
  BrowserMainTest() : command_line_(CommandLine::NO_PROGRAM) {}
 protected:
  virtual void SetUp() {
    sandbox_init_wrapper_.reset(new SandboxInitWrapper());
  }

  scoped_ptr<SandboxInitWrapper> sandbox_init_wrapper_;
  TestingPrefService pref_service_;
  CommandLine command_line_;
};

TEST_F(BrowserMainTest, WarmConnectionFieldTrial_WarmestSocket) {
  command_line_.AppendSwitchASCII(switches::kSocketReusePolicy, "0");

  scoped_ptr<MainFunctionParams> params(
      new MainFunctionParams(command_line_, *sandbox_init_wrapper_, NULL));
  scoped_ptr<content::BrowserMainParts> bw(
      content::GetContentClient()->browser()->CreateBrowserMainParts(
          *params));

  ChromeBrowserMainParts* cbw = static_cast<ChromeBrowserMainParts*>(bw.get());
  cbw->WarmConnectionFieldTrial();

  EXPECT_EQ(0, net::GetSocketReusePolicy());
}

TEST_F(BrowserMainTest, WarmConnectionFieldTrial_Random) {
  scoped_ptr<MainFunctionParams> params(
      new MainFunctionParams(command_line_, *sandbox_init_wrapper_, NULL));
  scoped_ptr<content::BrowserMainParts> bw(
      content::GetContentClient()->browser()->CreateBrowserMainParts(
          *params));
  ChromeBrowserMainParts* cbw = static_cast<ChromeBrowserMainParts*>(bw.get());

  const int kNumRuns = 1000;
  for (int i = 0; i < kNumRuns; i++) {
    cbw->WarmConnectionFieldTrial();
    int val = net::GetSocketReusePolicy();
    EXPECT_LE(val, 2);
    EXPECT_GE(val, 0);
  }
}

TEST_F(BrowserMainTest, WarmConnectionFieldTrial_Invalid) {
  command_line_.AppendSwitchASCII(switches::kSocketReusePolicy, "100");

  scoped_ptr<MainFunctionParams> params(
      new MainFunctionParams(command_line_, *sandbox_init_wrapper_, NULL));
  // This test ends up launching a new process, and that doesn't initialize the
  // ContentClient interfaces.
  scoped_ptr<content::BrowserMainParts> bw;
  if (content::GetContentClient()) {
    bw.reset(content::GetContentClient()->browser()->CreateBrowserMainParts(
        *params));
  } else {
    chrome::ChromeContentBrowserClient ccbc;
    bw.reset(ccbc.CreateBrowserMainParts(*params));
  }
  ChromeBrowserMainParts* cbw = static_cast<ChromeBrowserMainParts*>(bw.get());
#if defined(NDEBUG) && defined(DCHECK_ALWAYS_ON)
  EXPECT_DEATH(cbw->WarmConnectionFieldTrial(),
                     "Not a valid socket reuse policy group");
#else
  EXPECT_DEBUG_DEATH(cbw->WarmConnectionFieldTrial(),
                     "Not a valid socket reuse policy group");
#endif
}
