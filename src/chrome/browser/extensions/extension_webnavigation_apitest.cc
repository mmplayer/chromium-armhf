// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_webnavigation_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "net/base/mock_host_resolver.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "webkit/glue/context_menu.h"

namespace {

class TestRenderViewContextMenu : public RenderViewContextMenu {
 public:
  TestRenderViewContextMenu(TabContents* tab_contents,
                            const ContextMenuParams& params)
      : RenderViewContextMenu(tab_contents, params) {
  }
  virtual ~TestRenderViewContextMenu() {}

 private:
  virtual void PlatformInit() {}
  virtual bool GetAcceleratorForCommandId(int, ui::Accelerator*) {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(TestRenderViewContextMenu);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigation) {
  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_api.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationGetFrame) {
  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_getFrame.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationClientRedirect) {
  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_clientRedirect.html"))
          << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationServerRedirect) {
  FrameNavigationState::set_allow_extension_scheme(true);
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_serverRedirect.html"))
          << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationForwardBack) {
  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_forwardBack.html"))
          << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationIFrame) {
  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_iframe.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationOpenTab) {
  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_openTab.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationReferenceFragment) {
  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_referenceFragment.html"))
          << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationSimpleLoad) {
  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_simpleLoad.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationFailures) {
  FrameNavigationState::set_allow_extension_scheme(true);

  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_failures.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationUserAction) {
  FrameNavigationState::set_allow_extension_scheme(true);

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(
      RunExtensionSubtest("webnavigation", "test_userAction.html")) << message_;

  TabContents* tab = browser()->GetSelectedTabContents();
  ui_test_utils::WaitForLoadStop(tab);

  ResultCatcher catcher;

  ExtensionService* service = browser()->profile()->GetExtensionService();
  const Extension* extension =
      service->GetExtensionById(last_loaded_extension_id_, false);
  GURL url = extension->GetResourceURL("userAction/a.html");

  ui_test_utils::NavigateToURL(browser(), url);

  // This corresponds to "Open link in new tab".
  ContextMenuParams params;
  params.is_editable = false;
  params.media_type = WebKit::WebContextMenuData::MediaTypeNone;
  params.page_url = url;
  params.frame_id =
      ExtensionWebNavigationTabObserver::Get(tab)->
          frame_navigation_state().GetMainFrameID();
  params.link_url = extension->GetResourceURL("userAction/b.html");

  TestRenderViewContextMenu menu(tab, params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebNavigationRequestOpenTab) {
  FrameNavigationState::set_allow_extension_scheme(true);

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionSubtest("webnavigation", "test_requestOpenTab.html"))
      << message_;

  TabContents* tab = browser()->GetSelectedTabContents();
  ui_test_utils::WaitForLoadStop(tab);

  ResultCatcher catcher;

  ExtensionService* service = browser()->profile()->GetExtensionService();
  const Extension* extension =
      service->GetExtensionById(last_loaded_extension_id_, false);
  GURL url = extension->GetResourceURL("requestOpenTab/a.html");

  ui_test_utils::NavigateToURL(browser(), url);

  // There's a link on a.html. Middle-click on it to open it in a new tab.
  WebKit::WebMouseEvent mouse_event;
  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.button = WebKit::WebMouseEvent::ButtonMiddle;
  mouse_event.x = 7;
  mouse_event.y = 7;
  mouse_event.clickCount = 1;
  tab->render_view_host()->ForwardMouseEvent(mouse_event);
  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  tab->render_view_host()->ForwardMouseEvent(mouse_event);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}
