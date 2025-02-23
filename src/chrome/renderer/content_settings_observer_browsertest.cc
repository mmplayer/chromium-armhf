// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/content_settings.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/content_settings_observer.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_view.h"
#include "ipc/ipc_message_macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using testing::_;
using testing::DeleteArg;

namespace {

class MockContentSettingsObserver : public ContentSettingsObserver {
 public:
  explicit MockContentSettingsObserver(content::RenderView* render_view);

  virtual bool Send(IPC::Message* message);

  MOCK_METHOD2(OnContentBlocked,
               void(ContentSettingsType, const std::string&));

  MOCK_METHOD5(OnAllowDOMStorage,
               void(int, const GURL&, const GURL&, bool, IPC::Message*));
};

MockContentSettingsObserver::MockContentSettingsObserver(
    content::RenderView* render_view)
    : ContentSettingsObserver(render_view) {
}

bool MockContentSettingsObserver::Send(IPC::Message* message) {
  IPC_BEGIN_MESSAGE_MAP(MockContentSettingsObserver, *message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_ContentBlocked, OnContentBlocked)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ChromeViewHostMsg_AllowDOMStorage,
                                    OnAllowDOMStorage)
    IPC_MESSAGE_UNHANDLED(ADD_FAILURE())
  IPC_END_MESSAGE_MAP()

  // Our super class deletes the message.
  return RenderViewObserver::Send(message);
}

}  // namespace

TEST_F(ChromeRenderViewTest, DidBlockContentType) {
  MockContentSettingsObserver observer(view_);
  EXPECT_CALL(observer,
              OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES, std::string()));
  observer.DidBlockContentType(CONTENT_SETTINGS_TYPE_COOKIES, std::string());

  // Blocking the same content type a second time shouldn't send a notification.
  observer.DidBlockContentType(CONTENT_SETTINGS_TYPE_COOKIES, std::string());
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  // Blocking two different plugins should send two notifications.
  std::string kFooPlugin = "foo";
  std::string kBarPlugin = "bar";
  EXPECT_CALL(observer,
              OnContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS, kFooPlugin));
  EXPECT_CALL(observer,
              OnContentBlocked(CONTENT_SETTINGS_TYPE_PLUGINS, kBarPlugin));
  observer.DidBlockContentType(CONTENT_SETTINGS_TYPE_PLUGINS, kFooPlugin);
  observer.DidBlockContentType(CONTENT_SETTINGS_TYPE_PLUGINS, kBarPlugin);
}

// Tests that multiple invokations of AllowDOMStorage result in a single IPC.
TEST_F(ChromeRenderViewTest, AllowDOMStorage) {
  // Load some HTML, so we have a valid security origin.
  LoadHTML("<html></html>");
  MockContentSettingsObserver observer(view_);
  ON_CALL(observer,
          OnAllowDOMStorage(_, _, _, _, _)).WillByDefault(DeleteArg<4>());
  EXPECT_CALL(observer,
              OnAllowDOMStorage(_, _, _, _, _));
  observer.AllowStorage(view_->GetWebView()->focusedFrame(), true);

  // Accessing localStorage from the same origin again shouldn't result in a
  // new IPC.
  observer.AllowStorage(view_->GetWebView()->focusedFrame(), true);
  ::testing::Mock::VerifyAndClearExpectations(&observer);
}

// Regression test for http://crbug.com/35011
TEST_F(ChromeRenderViewTest, JSBlockSentAfterPageLoad) {
  // 1. Load page with JS.
  std::string html = "<html>"
                     "<head>"
                     "<script>document.createElement('div');</script>"
                     "</head>"
                     "<body>"
                     "</body>"
                     "</html>";
  render_thread_->sink().ClearMessages();
  LoadHTML(html.c_str());

  // 2. Block JavaScript.
  ContentSettings settings;
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i)
    settings.settings[i] = CONTENT_SETTING_ALLOW;
  settings.settings[CONTENT_SETTINGS_TYPE_JAVASCRIPT] = CONTENT_SETTING_BLOCK;
  ContentSettingsObserver* observer = ContentSettingsObserver::Get(view_);
  observer->SetContentSettings(settings);
  ContentSettingsObserver::SetDefaultContentSettings(settings);

  // Make sure no pending messages are in the queue.
  ProcessPendingMessages();
  render_thread_->sink().ClearMessages();

  // 3. Reload page.
  std::string url_str = "data:text/html;charset=utf-8,";
  url_str.append(html);
  GURL url(url_str);
  Reload(url);
  ProcessPendingMessages();

  // 4. Verify that the notification that javascript was blocked is sent after
  //    the navigation notifiction is sent.
  int navigation_index = -1;
  int block_index = -1;
  for (size_t i = 0; i < render_thread_->sink().message_count(); ++i) {
    const IPC::Message* msg = render_thread_->sink().GetMessageAt(i);
    if (msg->type() == GetNavigationIPCType())
      navigation_index = i;
    if (msg->type() == ChromeViewHostMsg_ContentBlocked::ID)
      block_index = i;
  }
  EXPECT_NE(-1, navigation_index);
  EXPECT_NE(-1, block_index);
  EXPECT_LT(navigation_index, block_index);
}

TEST_F(ChromeRenderViewTest, PluginsTemporarilyAllowed) {
  // Load some HTML.
  LoadHTML("<html>Foo</html>");

  // Block plugins.
  ContentSettings settings;
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i)
    settings.settings[i] = CONTENT_SETTING_ALLOW;
  settings.settings[CONTENT_SETTINGS_TYPE_PLUGINS] = CONTENT_SETTING_BLOCK;
  ContentSettingsObserver* observer = ContentSettingsObserver::Get(view_);
  observer->SetContentSettings(settings);
  ContentSettingsObserver::SetDefaultContentSettings(settings);
  EXPECT_FALSE(observer->plugins_temporarily_allowed());

  // Temporarily allow plugins.
  OnMessageReceived(ChromeViewMsg_LoadBlockedPlugins(MSG_ROUTING_NONE));
  EXPECT_TRUE(observer->plugins_temporarily_allowed());

  // Simulate a navigation within the page.
  DidNavigateWithinPage(GetMainFrame(), true);
  EXPECT_TRUE(observer->plugins_temporarily_allowed());

  // Navigate to a different page.
  LoadHTML("<html>Bar</html>");
  EXPECT_FALSE(observer->plugins_temporarily_allowed());
}
