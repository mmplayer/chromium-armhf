// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_webrequest_api.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "net/base/mock_host_resolver.h"

namespace {

class CancelLoginDialog : public NotificationObserver {
 public:
  CancelLoginDialog() {
    registrar_.Add(this,
                   chrome::NOTIFICATION_AUTH_NEEDED,
                   NotificationService::AllSources());
  }

  virtual ~CancelLoginDialog() {}

  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    LoginHandler* handler =
        Details<LoginNotificationDetails>(details).ptr()->handler();
    handler->CancelAuth();
  }

 private:
  NotificationRegistrar registrar_;

 DISALLOW_COPY_AND_ASSIGN(CancelLoginDialog);
};

}  // namespace

class ExtensionWebRequestApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpInProcessBrowserTestFixture() {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartTestServer());
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, WebRequestApi) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_api.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, WebRequestSimple) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_simple.html")) <<
      message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, WebRequestComplex) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_complex.html")) <<
      message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, WebRequestAuthRequired) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  CancelLoginDialog login_dialog_helper;

  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_auth_required.html")) <<
      message_;
}

// Hangs flakily: http://crbug.com/91715
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       DISABLED_WebRequestBlocking) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_blocking.html")) <<
      message_;
}
