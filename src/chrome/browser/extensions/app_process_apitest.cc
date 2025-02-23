// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "net/base/mock_host_resolver.h"

class AppApiTest : public ExtensionApiTest {
 protected:
  // Gets the base URL for files for a specific test, making sure that it uses
  // "localhost" as the hostname, since that is what the extent is declared
  // as in the test apps manifests.
  GURL GetTestBaseURL(std::string test_directory) {
    GURL::Replacements replace_host;
    std::string host_str("localhost");  // must stay in scope with replace_host
    replace_host.SetHostStr(host_str);
    GURL base_url = test_server()->GetURL(
        "files/extensions/api_test/" + test_directory + "/");
    return base_url.ReplaceComponents(replace_host);
  }
};

// Simulates a page calling window.open on an URL, and waits for the navigation.
static void WindowOpenHelper(Browser* browser,
                             RenderViewHost* opener_host,
                             const GURL& url,
                             bool newtab_process_should_equal_opener) {
  ui_test_utils::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP, NotificationService::AllSources());
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(
      opener_host, L"", L"window.open('" + UTF8ToWide(url.spec()) + L"');"));

  // The above window.open call is not user-initiated, it will create
  // a popup window instead of a new tab in current window.
  // Now the active tab in last active window should be the new tab.
  Browser* last_active_browser = BrowserList::GetLastActive();
  EXPECT_TRUE(last_active_browser);
  TabContents* newtab = last_active_browser->GetSelectedTabContents();
  EXPECT_TRUE(newtab);
  observer.Wait();
  EXPECT_EQ(url, newtab->controller().GetLastCommittedEntry()->url());
  if (newtab_process_should_equal_opener)
    EXPECT_EQ(opener_host->process(), newtab->render_view_host()->process());
  else
    EXPECT_NE(opener_host->process(), newtab->render_view_host()->process());
}

// Simulates a page navigating itself to an URL, and waits for the navigation.
static void NavigateTabHelper(TabContents* contents, const GURL& url) {
  bool result = false;
  ui_test_utils::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      NotificationService::AllSources());
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      contents->render_view_host(), L"",
      L"window.addEventListener('unload', function() {"
      L"    window.domAutomationController.send(true);"
      L"}, false);"
      L"window.location = '" + UTF8ToWide(url.spec()) + L"';",
      &result));
  ASSERT_TRUE(result);
  observer.Wait();
  EXPECT_EQ(url, contents->controller().GetLastCommittedEntry()->url());
}

#if defined(OS_WIN)
// AppProcess sometimes hangs on Windows
// http://crbug.com/88316
#define MAYBE_AppProcess DISABLED_AppProcess
#else
#define MAYBE_AppProcess AppProcess
#endif

IN_PROC_BROWSER_TEST_F(AppApiTest, MAYBE_AppProcess) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);

  ExtensionProcessManager* extension_process_manager =
      browser()->profile()->GetExtensionProcessManager();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app_process")));

  // Open two tabs in the app, one outside it.
  GURL base_url = GetTestBaseURL("app_process");

  // Test both opening a URL in a new tab, and opening a tab and then navigating
  // it.  Either way, app tabs should be considered extension processes, but
  // they have no elevated privileges and thus should not have WebUI bindings.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("path1/empty.html"), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_TRUE(extension_process_manager->IsExtensionProcess(
      browser()->GetTabContentsAt(1)->render_view_host()->process()->id()));
  EXPECT_FALSE(browser()->GetTabContentsAt(1)->web_ui());
  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path2/empty.html"));
  EXPECT_TRUE(extension_process_manager->IsExtensionProcess(
      browser()->GetTabContentsAt(2)->render_view_host()->process()->id()));
  EXPECT_FALSE(browser()->GetTabContentsAt(2)->web_ui());
  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path3/empty.html"));
  EXPECT_FALSE(extension_process_manager->IsExtensionProcess(
      browser()->GetTabContentsAt(3)->render_view_host()->process()->id()));
  EXPECT_FALSE(browser()->GetTabContentsAt(3)->web_ui());

  // We should have opened 3 new extension tabs. Including the original blank
  // tab, we now have 4 tabs. Because the app_process app has the background
  // permission, all of its instances are in the same process.  Thus two tabs
  // should be part of the extension app and grouped in the same process.
  ASSERT_EQ(4, browser()->tab_count());
  RenderViewHost* host = browser()->GetTabContentsAt(1)->render_view_host();

  EXPECT_EQ(host->process(),
            browser()->GetTabContentsAt(2)->render_view_host()->process());
  EXPECT_NE(host->process(),
            browser()->GetTabContentsAt(3)->render_view_host()->process());

  // Now let's do the same using window.open. The same should happen.
  ASSERT_EQ(1u, BrowserList::GetBrowserCount(browser()->profile()));
  WindowOpenHelper(browser(), host,
                   base_url.Resolve("path1/empty.html"), true);
  WindowOpenHelper(browser(), host,
                   base_url.Resolve("path2/empty.html"), true);
  // TODO(creis): This should open in a new process (i.e., false for the last
  // argument), but we temporarily avoid swapping processes away from an app
  // until we're able to support cross-process postMessage calls.
  // See crbug.com/59285.
  WindowOpenHelper(browser(), host,
                   base_url.Resolve("path3/empty.html"), true);

  // Now let's have these pages navigate, into or out of the extension web
  // extent. They should switch processes.
  const GURL& app_url(base_url.Resolve("path1/empty.html"));
  const GURL& non_app_url(base_url.Resolve("path3/empty.html"));
  NavigateTabHelper(browser()->GetTabContentsAt(2), non_app_url);
  NavigateTabHelper(browser()->GetTabContentsAt(3), app_url);
  // TODO(creis): This should swap out of the app's process (i.e., EXPECT_NE),
  // but we temporarily avoid swapping away from an app in case the window
  // tries to send a postMessage to the app.  See crbug.com/59285.
  EXPECT_EQ(host->process(),
            browser()->GetTabContentsAt(2)->render_view_host()->process());
  EXPECT_EQ(host->process(),
            browser()->GetTabContentsAt(3)->render_view_host()->process());

  // If one of the popup tabs navigates back to the app, window.opener should
  // be valid.
  NavigateTabHelper(browser()->GetTabContentsAt(6), app_url);
  EXPECT_EQ(host->process(),
            browser()->GetTabContentsAt(6)->render_view_host()->process());
  bool windowOpenerValid = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetTabContentsAt(6)->render_view_host(), L"",
      L"window.domAutomationController.send(window.opener != null)",
      &windowOpenerValid));
  ASSERT_TRUE(windowOpenerValid);
}


#if defined(OS_WIN)
// Seems to timeout sometimes on Windows: http://crbug.com/89766
#define MAYBE_AppProcessInstances FLAKY_AppProcessInstances
#else
#define MAYBE_AppProcessInstances AppProcessInstances
#endif

// Test that hosted apps without the background permission use a process per app
// instance model, such that separate instances are in separate processes.
IN_PROC_BROWSER_TEST_F(AppApiTest, MAYBE_AppProcessInstances) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);

  ExtensionProcessManager* extension_process_manager =
      browser()->profile()->GetExtensionProcessManager();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("app_process_instances")));

  // Open two tabs in the app, one outside it.
  GURL base_url = GetTestBaseURL("app_process_instances");

  // Test both opening a URL in a new tab, and opening a tab and then navigating
  // it.  Either way, app tabs should be considered extension processes, but
  // they have no elevated privileges and thus should not have WebUI bindings.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), base_url.Resolve("path1/empty.html"), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_TRUE(extension_process_manager->IsExtensionProcess(
      browser()->GetTabContentsAt(1)->render_view_host()->process()->id()));
  EXPECT_FALSE(browser()->GetTabContentsAt(1)->web_ui());
  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path2/empty.html"));
  EXPECT_TRUE(extension_process_manager->IsExtensionProcess(
      browser()->GetTabContentsAt(2)->render_view_host()->process()->id()));
  EXPECT_FALSE(browser()->GetTabContentsAt(2)->web_ui());

  // We should have opened 2 new extension tabs. Including the original blank
  // tab, we now have 3 tabs. The two app tabs should not be in the same
  // process, since they do not have the background permission.  (Thus, we want
  // to separate them to improve responsiveness.)
  ASSERT_EQ(3, browser()->tab_count());
  RenderViewHost* host1 = browser()->GetTabContentsAt(1)->render_view_host();
  RenderViewHost* host2 = browser()->GetTabContentsAt(2)->render_view_host();
  EXPECT_NE(host1->process(), host2->process());

  // Opening tabs with window.open should keep the page in the opener's process.
  ASSERT_EQ(1u, BrowserList::GetBrowserCount(browser()->profile()));
  WindowOpenHelper(browser(), host1,
                   base_url.Resolve("path1/empty.html"), true);
  WindowOpenHelper(browser(), host2,
                   base_url.Resolve("path2/empty.html"), true);
}

// Tests that app process switching works properly in the following scenario:
// 1. navigate to a page1 in the app
// 2. page1 redirects to a page2 outside the app extent (ie, "/server-redirect")
// 3. page2 redirects back to a page in the app
// The final navigation should end up in the app process.
// See http://crbug.com/61757
IN_PROC_BROWSER_TEST_F(AppApiTest, AppProcessRedirectBack) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app_process")));

  // Open two tabs in the app.
  GURL base_url = GetTestBaseURL("app_process");

  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path1/empty.html"));
  browser()->NewTab();
  // Wait until the second tab finishes its redirect train (2 hops).
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), base_url.Resolve("path1/redirect.html"), 2);

  // 3 tabs, including the initial about:blank. The last 2 should be the same
  // process.
  ASSERT_EQ(3, browser()->tab_count());
  EXPECT_EQ("/files/extensions/api_test/app_process/path1/empty.html",
            browser()->GetTabContentsAt(2)->controller().
                GetLastCommittedEntry()->url().path());
  RenderViewHost* host = browser()->GetTabContentsAt(1)->render_view_host();
  EXPECT_EQ(host->process(),
            browser()->GetTabContentsAt(2)->render_view_host()->process());
}

// Ensure that reloading a URL after installing or uninstalling it as an app
// correctly swaps the process.  (http://crbug.com/80621)
// Disabled until we get a correct fix for 80621.  See http://crbug.com/102408.
IN_PROC_BROWSER_TEST_F(AppApiTest, DISABLED_ReloadIntoAppProcess) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);

  ExtensionProcessManager* extension_process_manager =
      browser()->profile()->GetExtensionProcessManager();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = GetTestBaseURL("app_process");

  // Load an app URL before loading the app.
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path1/empty.html"));
  TabContents* contents = browser()->GetTabContentsAt(0);
  EXPECT_FALSE(extension_process_manager->IsExtensionProcess(
      contents->render_view_host()->process()->id()));

  // Load app and navigate to the page.
  const Extension* app =
      LoadExtension(test_data_dir_.AppendASCII("app_process"));
  ASSERT_TRUE(app);
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path1/empty.html"));
  EXPECT_TRUE(extension_process_manager->IsExtensionProcess(
      contents->render_view_host()->process()->id()));

  // Disable app and navigate to the page.
  DisableExtension(app->id());
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path1/empty.html"));
  EXPECT_FALSE(extension_process_manager->IsExtensionProcess(
      contents->render_view_host()->process()->id()));

  // Enable app and reload the page.
  EnableExtension(app->id());
  ui_test_utils::WindowedNotificationObserver reload_observer(
      content::NOTIFICATION_LOAD_STOP,
      Source<NavigationController>(
          &browser()->GetSelectedTabContentsWrapper()->controller()));
  browser()->Reload(CURRENT_TAB);
  reload_observer.Wait();
  EXPECT_TRUE(extension_process_manager->IsExtensionProcess(
      contents->render_view_host()->process()->id()));

  // Disable app and reload the page.
  DisableExtension(app->id());
  ui_test_utils::WindowedNotificationObserver reload_observer2(
      content::NOTIFICATION_LOAD_STOP,
      Source<NavigationController>(
          &browser()->GetSelectedTabContentsWrapper()->controller()));
  browser()->Reload(CURRENT_TAB);
  reload_observer2.Wait();
  EXPECT_FALSE(extension_process_manager->IsExtensionProcess(
      contents->render_view_host()->process()->id()));

  // Enable app and reload via JavaScript.
  EnableExtension(app->id());
  ui_test_utils::WindowedNotificationObserver js_reload_observer(
      content::NOTIFICATION_LOAD_STOP,
      Source<NavigationController>(
          &browser()->GetSelectedTabContentsWrapper()->controller()));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(contents->render_view_host(),
                                               L"", L"location.reload();"));
  js_reload_observer.Wait();
  EXPECT_TRUE(extension_process_manager->IsExtensionProcess(
      contents->render_view_host()->process()->id()));

  // Disable app and reload via JavaScript.
  DisableExtension(app->id());
  ui_test_utils::WindowedNotificationObserver js_reload_observer2(
      content::NOTIFICATION_LOAD_STOP,
      Source<NavigationController>(
          &browser()->GetSelectedTabContentsWrapper()->controller()));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(contents->render_view_host(),
                                               L"", L"location.reload();"));
  js_reload_observer2.Wait();
  EXPECT_FALSE(extension_process_manager->IsExtensionProcess(
      contents->render_view_host()->process()->id()));
}

// Tests that if we have a non-app process (path3/container.html) that has an
// iframe with  a URL in the app's extent (path1/iframe.html), then opening a
// link from that iframe to a new window to a URL in the app's extent (path1/
// empty.html) results in the new window being in an app process. See
// http://crbug.com/89272 for more details.
IN_PROC_BROWSER_TEST_F(AppApiTest, OpenAppFromIframe) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);

  ExtensionProcessManager* extension_process_manager =
      browser()->profile()->GetExtensionProcessManager();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  GURL base_url = GetTestBaseURL("app_process");

  // Load app and start URL (not in the app).
  const Extension* app =
      LoadExtension(test_data_dir_.AppendASCII("app_process"));
  ASSERT_TRUE(app);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      base_url.Resolve("path3/container.html"),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  EXPECT_FALSE(extension_process_manager->IsExtensionProcess(
      browser()->GetTabContentsAt(0)->render_view_host()->process()->id()));

  // Wait for popup window to appear.
  GURL app_url = base_url.Resolve("path1/empty.html");
  Browser* last_active_browser = BrowserList::GetLastActive();
  EXPECT_TRUE(last_active_browser);
  ASSERT_NE(browser(), last_active_browser);
  TabContents* newtab = last_active_browser->GetSelectedTabContents();
  EXPECT_TRUE(newtab);
  if (!newtab->controller().GetLastCommittedEntry() ||
      newtab->controller().GetLastCommittedEntry()->url() != app_url)
    ui_test_utils::WaitForNavigation(&newtab->controller());

  // Popup window should be in the app's process.
  EXPECT_TRUE(extension_process_manager->IsExtensionProcess(
      last_active_browser->GetTabContentsAt(0)->render_view_host()->process()->
          id()));
}

// Tests that if we have an app process (path1/container.html) with a non-app
// iframe (path3/iframe.html), then opening a link from that iframe to a new
// window to a same-origin non-app URL (path3/empty.html) should keep the window
// in the app process.
// This is in contrast to OpenAppFromIframe, since here the popup will not be
// missing special permissions and should be scriptable from the iframe.
// See http://crbug.com/92669 for more details.
IN_PROC_BROWSER_TEST_F(AppApiTest, OpenWebPopupFromWebIframe) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);

  ExtensionProcessManager* extension_process_manager =
      browser()->profile()->GetExtensionProcessManager();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  GURL base_url = GetTestBaseURL("app_process");

  // Load app and start URL (in the app).
  const Extension* app =
      LoadExtension(test_data_dir_.AppendASCII("app_process"));
  ASSERT_TRUE(app);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      base_url.Resolve("path1/container.html"),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  RenderProcessHost* process =
      browser()->GetTabContentsAt(0)->render_view_host()->process();
  EXPECT_TRUE(extension_process_manager->IsExtensionProcess(
      process->id()));

  // Wait for popup window to appear.  The new Browser may not have been
  // added with SetLastActive, in which case we need to show it first.
  // This is necessary for popup windows without a cross-site transition.
  if (browser() == BrowserList::GetLastActive()) {
    // Grab the second window and show it.
    ASSERT_TRUE(BrowserList::size() == 2);
    Browser* popup_browser = *(++BrowserList::begin());
    popup_browser->window()->Show();
  }
  Browser* last_active_browser = BrowserList::GetLastActive();
  EXPECT_TRUE(last_active_browser);
  ASSERT_NE(browser(), last_active_browser);
  TabContents* newtab = last_active_browser->GetSelectedTabContents();
  EXPECT_TRUE(newtab);
  GURL non_app_url = base_url.Resolve("path3/empty.html");
  if (!newtab->controller().GetLastCommittedEntry() ||
      newtab->controller().GetLastCommittedEntry()->url() != non_app_url)
    ui_test_utils::WaitForNavigation(&newtab->controller());

  // Popup window should be in the app's process.
  RenderProcessHost* popup_process =
      last_active_browser->GetTabContentsAt(0)->render_view_host()->process();
  EXPECT_EQ(process, popup_process);
}

IN_PROC_BROWSER_TEST_F(AppApiTest, ReloadAppAfterCrash) {
  ExtensionProcessManager* extension_process_manager =
      browser()->profile()->GetExtensionProcessManager();
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app_process")));

  GURL base_url = GetTestBaseURL("app_process");

  // Load the app, chrome.app.isInstalled should be true.
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve("path1/empty.html"));
  TabContents* contents = browser()->GetTabContentsAt(0);
  EXPECT_TRUE(extension_process_manager->IsExtensionProcess(
      contents->render_view_host()->process()->id()));
  bool is_installed = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      contents->render_view_host(), L"",
      L"window.domAutomationController.send(chrome.app.isInstalled)",
      &is_installed));
  ASSERT_TRUE(is_installed);

  // Crash the tab and reload it, chrome.app.isInstalled should still be true.
  ui_test_utils::CrashTab(browser()->GetSelectedTabContents());
  ui_test_utils::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      Source<NavigationController>(
          &browser()->GetSelectedTabContentsWrapper()->controller()));
  browser()->Reload(CURRENT_TAB);
  observer.Wait();
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      contents->render_view_host(), L"",
      L"window.domAutomationController.send(chrome.app.isInstalled)",
      &is_installed));
  ASSERT_TRUE(is_installed);
}
