// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

class ExtensionClipboardApiTest : public ExtensionApiTest {
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalExtensionApis);

    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionClipboardApiTest, DISABLED_WriteAndReadTest) {
#if defined(OS_WIN)
  ASSERT_TRUE(RunExtensionTest("clipboard_api/extension_win")) << message_;
#else  // !defined(OS_WIN)
#if defined(OS_MACOSX)
  ASSERT_TRUE(RunExtensionTest("clipboard_api/extension_mac")) << message_;
#else  // !defined(OS_WIN) && !defiend(OS_MACOSX)
  ASSERT_TRUE(RunExtensionTest("clipboard_api/extension"))  << message_;
#endif  // defined(OS_MAOSX)
#endif  // defiend(OS_WIN)
};

IN_PROC_BROWSER_TEST_F(ExtensionClipboardApiTest, NoPermission) {
  ASSERT_FALSE(RunExtensionTest("clipboard_api/extension_no_permission"))
      << message_;
};
