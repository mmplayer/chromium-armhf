// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_text_field.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#import "content/browser/find_pasteboard.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// Expose private variables to make testing easier.
@interface FindBarCocoaController(Testing)
- (NSView*)findBarView;
- (NSString*)findText;
- (FindBarTextField*)findTextField;
@end

@implementation FindBarCocoaController(Testing)
- (NSView*)findBarView {
  return findBarView_;
}

- (NSString*)findText {
  return [findText_ stringValue];
}

- (FindBarTextField*)findTextField {
  return findText_;
}

- (NSButton*)nextButton {
  return nextButton_;
}

- (NSButton*)previousButton {
  return previousButton_;
}
@end

namespace {

class FindBarCocoaControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    controller_.reset([[FindBarCocoaController alloc] init]);
    [[test_window() contentView] addSubview:[controller_ view]];
  }

  virtual void TearDown() {
    CocoaTest::TearDown();
    [controller_ stopAnimation];
  }

 protected:
  scoped_nsobject<FindBarCocoaController> controller_;
};

TEST_VIEW(FindBarCocoaControllerTest, [controller_ view])

TEST_F(FindBarCocoaControllerTest, ImagesLoadedProperly) {
  EXPECT_TRUE([[[controller_ nextButton] image] isValid]);
  EXPECT_TRUE([[[controller_ previousButton] image] isValid]);
}

TEST_F(FindBarCocoaControllerTest, ShowAndHide) {
  NSView* findBarView = [controller_ findBarView];

  ASSERT_GT([findBarView frame].origin.y, 0);
  ASSERT_FALSE([controller_ isFindBarVisible]);

  [controller_ showFindBar:NO];
  EXPECT_EQ([findBarView frame].origin.y, 0);
  EXPECT_TRUE([controller_ isFindBarVisible]);

  [controller_ hideFindBar:NO];
  EXPECT_GT([findBarView frame].origin.y, 0);
  EXPECT_FALSE([controller_ isFindBarVisible]);
}

TEST_F(FindBarCocoaControllerTest, SetFindText) {
  NSTextField* findTextField = [controller_ findTextField];

  // Start by making the find bar visible.
  [controller_ showFindBar:NO];
  EXPECT_TRUE([controller_ isFindBarVisible]);

  // Set the find text.
  NSString* const kFindText = @"Google";
  [controller_ setFindText:kFindText];
  EXPECT_EQ(
      NSOrderedSame,
      [[findTextField stringValue] compare:kFindText]);

  // Call clearResults, which doesn't actually clear the find text but
  // simply sets it back to what it was before.  This is silly, but
  // matches the behavior on other platforms.  |details| isn't used by
  // our implementation of clearResults, so it's ok to pass in an
  // empty |details|.
  FindNotificationDetails details;
  [controller_ clearResults:details];
  EXPECT_EQ(
      NSOrderedSame,
      [[findTextField stringValue] compare:kFindText]);
}

TEST_F(FindBarCocoaControllerTest, ResultLabelUpdatesCorrectly) {
  // TODO(rohitrao): Test this.  It may involve creating some dummy
  // FindNotificationDetails objects.
}

TEST_F(FindBarCocoaControllerTest, FindTextIsGlobal) {
  scoped_nsobject<FindBarCocoaController> otherController(
      [[FindBarCocoaController alloc] init]);
  [[test_window() contentView] addSubview:[otherController view]];

  // Setting the text in one controller should update the other controller's
  // text as well.
  NSString* const kFindText = @"Respect to the man in the ice cream van";
  [controller_ setFindText:kFindText];
  EXPECT_EQ(
      NSOrderedSame,
      [[controller_ findText] compare:kFindText]);
  EXPECT_EQ(
      NSOrderedSame,
      [[otherController.get() findText] compare:kFindText]);
}

TEST_F(FindBarCocoaControllerTest, SettingFindTextUpdatesFindPboard) {
  NSString* const kFindText =
      @"It's not a bird, it's not a plane, it must be Dave who's on the train";
  [controller_ setFindText:kFindText];
  EXPECT_EQ(
      NSOrderedSame,
      [[[FindPasteboard sharedInstance] findText] compare:kFindText]);
}

}  // namespace
