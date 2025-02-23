// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen_exit_bubble_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/site_instance.h"
#include "testing/gtest_mac.h"
#include "ui/base/models/accelerator_cocoa.h"

@interface FullscreenExitBubbleController(JustForTesting)
// Already defined.
+ (NSString*)keyCommandString;
+ (NSString*)keyCombinationForAccelerator:(const ui::AcceleratorCocoa&)item;
@end

@interface FullscreenExitBubbleController(ExposedForTesting)
- (NSTextField*)exitLabelPlaceholder;
- (NSTextView*)exitLabel;
@end

@implementation FullscreenExitBubbleController(ExposedForTesting)
- (NSTextField*)exitLabelPlaceholder {
  return exitLabelPlaceholder_;
}

- (NSTextView*)exitLabel {
  return exitLabel_;
}
@end

class FullscreenExitBubbleControllerTest : public CocoaProfileTest {
 public:
  virtual void SetUp() {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(profile());

    site_instance_ = SiteInstance::CreateSiteInstance(profile());
    controller_.reset(
        [[FullscreenExitBubbleController alloc] initWithOwner:nil
                                                      browser:browser()
                                                          url:GURL()
                      bubbleType:FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION]);
    EXPECT_TRUE([controller_ window]);
  }

  virtual void TearDown() {
    [controller_ close];
    controller_.reset();
    CocoaProfileTest::TearDown();
  }

  void AppendTabToStrip() {
    TabContentsWrapper* tab_contents = Browser::TabContentsFactory(
        profile(), site_instance_, MSG_ROUTING_NONE,
        NULL, NULL);
    browser()->tabstrip_model()->AppendTabContents(
        tab_contents, /*foreground=*/true);
  }

  scoped_refptr<SiteInstance> site_instance_;
  scoped_nsobject<FullscreenExitBubbleController> controller_;
};

TEST_F(FullscreenExitBubbleControllerTest, DenyExitsFullscreen) {
  // This test goes with r107841, which was merged from trunk. However, it
  // doesn't seem to compile on 912. As such, commenting it out, since it's not
  // important that this be run on the branch (as long as it's passing on
  // trunk).
#if 0
  CreateBrowserWindow();
  AppendTabToStrip();
  TabContents* fullscreen_tab = browser()->GetSelectedTabContents();
  {
    base::mac::ScopedNSAutoreleasePool pool;
    ui_test_utils::WindowedNotificationObserver fullscreen_observer(
        chrome::NOTIFICATION_FULLSCREEN_CHANGED,
        content::NotificationService::AllSources());
    browser()->ToggleFullscreenModeForTab(fullscreen_tab, true);
    fullscreen_observer.Wait();
    ASSERT_TRUE(browser()->window()->IsFullscreen());
  }

  NSWindow* window = browser()->window()->GetNativeHandle();
  BrowserWindowController* bwc = [BrowserWindowController
      browserWindowControllerForWindow:window];
  FullscreenExitBubbleController* bubble = [bwc fullscreenExitBubbleController];
  ASSERT_TRUE(bubble);
  {
    ui_test_utils::WindowedNotificationObserver fullscreen_observer(
        chrome::NOTIFICATION_FULLSCREEN_CHANGED,
        content::NotificationService::AllSources());
    [bubble deny:nil];
    fullscreen_observer.Wait();
  }
  EXPECT_FALSE([bwc fullscreenExitBubbleController]);
  EXPECT_FALSE(browser()->window()->IsFullscreen());
  CloseBrowserWindow();
#endif
}

TEST_F(FullscreenExitBubbleControllerTest, LabelWasReplaced) {
  EXPECT_FALSE([controller_ exitLabelPlaceholder]);
  EXPECT_TRUE([controller_ exitLabel]);
}

TEST_F(FullscreenExitBubbleControllerTest, LabelContainsShortcut) {
  NSString* shortcut = [FullscreenExitBubbleController keyCommandString];
  EXPECT_GT([shortcut length], 0U);

  NSString* message = [[[controller_ exitLabel] textStorage] string];

  NSRange range = [message rangeOfString:shortcut];
  EXPECT_NE(NSNotFound, range.location);
}

TEST_F(FullscreenExitBubbleControllerTest, ShortcutText) {
  ui::AcceleratorCocoa cmd_F(@"F", NSCommandKeyMask);
  ui::AcceleratorCocoa cmd_shift_f(@"f", NSCommandKeyMask|NSShiftKeyMask);
  NSString* cmd_F_text = [FullscreenExitBubbleController
      keyCombinationForAccelerator:cmd_F];
  NSString* cmd_shift_f_text = [FullscreenExitBubbleController
      keyCombinationForAccelerator:cmd_shift_f];
  EXPECT_NSEQ(cmd_shift_f_text, cmd_F_text);
  EXPECT_NSEQ(@"\u2318\u21E7F", cmd_shift_f_text);
}
