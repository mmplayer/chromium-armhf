// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/scoped_nsobject.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#import "chrome/browser/ui/cocoa/animation_utils.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_constants.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_folder_window.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_unittest_helper.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_view.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button_cell.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#import "chrome/browser/ui/cocoa/view_resizer_pong.h"
#include "chrome/test/base/model_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "ui/base/test/cocoa_test_event_utils.h"
#include "ui/base/theme_provider.h"

// Unit tests don't need time-consuming asynchronous animations.
@interface BookmarkBarControllerTestable : BookmarkBarController {
}
@end
@implementation BookmarkBarControllerTestable
- (BOOL)animationEnabled {
  return NO;
}
@end

// Just like a BookmarkBarController but openURL: is stubbed out.
@interface BookmarkBarControllerNoOpen : BookmarkBarControllerTestable {
 @public
  std::vector<GURL> urls_;
  std::vector<WindowOpenDisposition> dispositions_;
}
@end

@implementation BookmarkBarControllerNoOpen
- (void)openURL:(GURL)url disposition:(WindowOpenDisposition)disposition {
  urls_.push_back(url);
  dispositions_.push_back(disposition);
}
- (void)clear {
  urls_.clear();
  dispositions_.clear();
}
@end


// NSCell that is pre-provided with a desired size that becomes the
// return value for -(NSSize)cellSize:.
@interface CellWithDesiredSize : NSCell {
 @private
  NSSize cellSize_;
}
@property (nonatomic, readonly) NSSize cellSize;
@end

@implementation CellWithDesiredSize

@synthesize cellSize = cellSize_;

- (id)initTextCell:(NSString*)string desiredSize:(NSSize)size {
  if ((self = [super initTextCell:string])) {
    cellSize_ = size;
  }
  return self;
}

@end

// Remember the number of times we've gotten a frameDidChange notification.
@interface BookmarkBarControllerTogglePong : BookmarkBarControllerNoOpen {
 @private
  int toggles_;
}
@property (nonatomic, readonly) int toggles;
@end

@implementation BookmarkBarControllerTogglePong

@synthesize toggles = toggles_;

- (void)frameDidChange {
  toggles_++;
}

@end

// Remembers if a notification callback was called.
@interface BookmarkBarControllerNotificationPong : BookmarkBarControllerNoOpen {
  BOOL windowWillCloseReceived_;
  BOOL windowDidResignKeyReceived_;
}
@property (nonatomic, readonly) BOOL windowWillCloseReceived;
@property (nonatomic, readonly) BOOL windowDidResignKeyReceived;
@end

@implementation BookmarkBarControllerNotificationPong
@synthesize windowWillCloseReceived = windowWillCloseReceived_;
@synthesize windowDidResignKeyReceived = windowDidResignKeyReceived_;

// Override NSNotificationCenter callback.
- (void)parentWindowWillClose:(NSNotification*)notification {
  windowWillCloseReceived_ = YES;
}

// NSNotificationCenter callback.
- (void)parentWindowDidResignKey:(NSNotification*)notification {
  windowDidResignKeyReceived_ = YES;
}
@end

// Remembers if and what kind of openAll was performed.
@interface BookmarkBarControllerOpenAllPong : BookmarkBarControllerNoOpen {
  WindowOpenDisposition dispositionDetected_;
}
@property (nonatomic) WindowOpenDisposition dispositionDetected;
@end

@implementation BookmarkBarControllerOpenAllPong
@synthesize dispositionDetected = dispositionDetected_;

// Intercede for the openAll:disposition: method.
- (void)openAll:(const BookmarkNode*)node
    disposition:(WindowOpenDisposition)disposition {
  [self setDispositionDetected:disposition];
}

@end

// Just like a BookmarkBarController but intercedes when providing
// pasteboard drag data.
@interface BookmarkBarControllerDragData : BookmarkBarControllerTestable {
  const BookmarkNode* dragDataNode_;  // Weak
}
- (void)setDragDataNode:(const BookmarkNode*)node;
@end

@implementation BookmarkBarControllerDragData

- (id)initWithBrowser:(Browser*)browser
         initialWidth:(CGFloat)initialWidth
             delegate:(id<BookmarkBarControllerDelegate>)delegate
       resizeDelegate:(id<ViewResizer>)resizeDelegate {
  if ((self = [super initWithBrowser:browser
                        initialWidth:initialWidth
                            delegate:delegate
                      resizeDelegate:resizeDelegate])) {
    dragDataNode_ = NULL;
  }
  return self;
}

- (void)setDragDataNode:(const BookmarkNode*)node {
  dragDataNode_ = node;
}

- (std::vector<const BookmarkNode*>)retrieveBookmarkNodeData {
  std::vector<const BookmarkNode*> dragDataNodes;
  if(dragDataNode_) {
    dragDataNodes.push_back(dragDataNode_);
  }
  return dragDataNodes;
}

@end


class FakeTheme : public ui::ThemeProvider {
 public:
  FakeTheme(NSColor* color) : color_(color) { }
  scoped_nsobject<NSColor> color_;

  virtual void Init(Profile* profile) { }
  virtual SkBitmap* GetBitmapNamed(int id) const { return nil; }
  virtual SkColor GetColor(int id) const { return SkColor(); }
  virtual bool GetDisplayProperty(int id, int* result) const { return false; }
  virtual bool ShouldUseNativeFrame() const { return false; }
  virtual bool HasCustomImage(int id) const { return false; }
  virtual RefCountedMemory* GetRawData(int id) const { return NULL; }
  virtual NSImage* GetNSImageNamed(int id, bool allow_default) const {
    return nil;
  }
  virtual NSColor* GetNSImageColorNamed(int id, bool allow_default) const {
    return nil;
  }
  virtual NSColor* GetNSColor(int id, bool allow_default) const {
    return color_.get();
  }
  virtual NSColor* GetNSColorTint(int id, bool allow_default) const {
    return nil;
  }
  virtual NSGradient* GetNSGradient(int id) const {
    return nil;
  }
};


@interface FakeDragInfo : NSObject {
 @public
  NSPoint dropLocation_;
  NSDragOperation sourceMask_;
}
@property (nonatomic, assign) NSPoint dropLocation;
- (void)setDraggingSourceOperationMask:(NSDragOperation)mask;
@end

@implementation FakeDragInfo

@synthesize dropLocation = dropLocation_;

- (id)init {
  if ((self = [super init])) {
    dropLocation_ = NSZeroPoint;
    sourceMask_ = NSDragOperationMove;
  }
  return self;
}

// NSDraggingInfo protocol functions.

- (id)draggingPasteboard {
  return self;
}

- (id)draggingSource {
  return self;
}

- (NSDragOperation)draggingSourceOperationMask {
  return sourceMask_;
}

- (NSPoint)draggingLocation {
  return dropLocation_;
}

// Other functions.

- (void)setDraggingSourceOperationMask:(NSDragOperation)mask {
  sourceMask_ = mask;
}

@end


namespace {

class BookmarkBarControllerTestBase : public CocoaProfileTest {
 public:
  scoped_nsobject<NSView> parent_view_;
  scoped_nsobject<ViewResizerPong> resizeDelegate_;

  virtual void SetUp() {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(profile());

    FilePath extension_dir;
    profile()->CreateExtensionService(CommandLine::ForCurrentProcess(),
                                      extension_dir, false);
    resizeDelegate_.reset([[ViewResizerPong alloc] init]);
    NSRect parent_frame = NSMakeRect(0, 0, 800, 50);
    parent_view_.reset([[NSView alloc] initWithFrame:parent_frame]);
    [parent_view_ setHidden:YES];
  }

  void InstallAndToggleBar(BookmarkBarController* bar) {
    // Force loading of the nib.
    [bar view];
    // Awkwardness to look like we've been installed.
    for (NSView* subView in [parent_view_ subviews])
      [subView removeFromSuperview];
    [parent_view_ addSubview:[bar view]];
    NSRect frame = [[[bar view] superview] frame];
    frame.origin.y = 100;
    [[[bar view] superview] setFrame:frame];

    // Make sure it's on in a window so viewDidMoveToWindow is called
    NSView* contentView = [test_window() contentView];
    if (![parent_view_ isDescendantOf:contentView])
      [contentView addSubview:parent_view_];

    // Make sure it's open so certain things aren't no-ops.
    [bar updateAndShowNormalBar:YES
                showDetachedBar:NO
                  withAnimation:NO];
  }
};

class BookmarkBarControllerTest : public BookmarkBarControllerTestBase {
 public:
  scoped_nsobject<BookmarkMenu> menu_;
  scoped_nsobject<NSMenuItem> menu_item_;
  scoped_nsobject<NSButtonCell> cell_;
  scoped_nsobject<BookmarkBarControllerNoOpen> bar_;

  virtual void SetUp() {
    BookmarkBarControllerTestBase::SetUp();
    ASSERT_TRUE(browser());

    bar_.reset(
      [[BookmarkBarControllerNoOpen alloc]
          initWithBrowser:browser()
             initialWidth:NSWidth([parent_view_ frame])
                 delegate:nil
           resizeDelegate:resizeDelegate_.get()]);

    InstallAndToggleBar(bar_.get());

    // Create a menu/item to act like a sender
    menu_.reset([[BookmarkMenu alloc] initWithTitle:@"I_dont_care"]);
    menu_item_.reset([[NSMenuItem alloc]
                       initWithTitle:@"still_dont_care"
                              action:NULL
                       keyEquivalent:@""]);
    cell_.reset([[NSButtonCell alloc] init]);
    [menu_item_ setMenu:menu_.get()];
  }

  // Return a menu item that points to the given URL.
  NSMenuItem* ItemForBookmarkBarMenu(GURL& gurl) {
    BookmarkModel* model = profile()->GetBookmarkModel();
    const BookmarkNode* parent = model->bookmark_bar_node();
    const BookmarkNode* node = model->AddURL(parent, parent->child_count(),
                                             ASCIIToUTF16("A title"), gurl);
    [menu_ setRepresentedObject:[NSNumber numberWithLongLong:node->id()]];
    return menu_item_;
  }

  // Does NOT take ownership of node.
  NSMenuItem* ItemForBookmarkBarMenu(const BookmarkNode* node) {
    [menu_ setRepresentedObject:[NSNumber numberWithLongLong:node->id()]];
    return menu_item_;
  }

  BookmarkBarControllerNoOpen* noOpenBar() {
    return (BookmarkBarControllerNoOpen*)bar_.get();
  }
};

TEST_F(BookmarkBarControllerTest, ShowWhenShowBookmarkBarTrue) {
  [bar_ updateAndShowNormalBar:YES
               showDetachedBar:NO
                 withAnimation:NO];
  EXPECT_TRUE([bar_ isInState:bookmarks::kShowingState]);
  EXPECT_FALSE([bar_ isInState:bookmarks::kDetachedState]);
  EXPECT_TRUE([bar_ isVisible]);
  EXPECT_FALSE([bar_ isAnimationRunning]);
  EXPECT_FALSE([[bar_ view] isHidden]);
  EXPECT_GT([resizeDelegate_ height], 0);
  EXPECT_GT([[bar_ view] frame].size.height, 0);
}

TEST_F(BookmarkBarControllerTest, HideWhenShowBookmarkBarFalse) {
  [bar_ updateAndShowNormalBar:NO
               showDetachedBar:NO
                 withAnimation:NO];
  EXPECT_FALSE([bar_ isInState:bookmarks::kShowingState]);
  EXPECT_FALSE([bar_ isInState:bookmarks::kDetachedState]);
  EXPECT_FALSE([bar_ isVisible]);
  EXPECT_FALSE([bar_ isAnimationRunning]);
  EXPECT_TRUE([[bar_ view] isHidden]);
  EXPECT_EQ(0, [resizeDelegate_ height]);
  EXPECT_EQ(0, [[bar_ view] frame].size.height);
}

TEST_F(BookmarkBarControllerTest, HideWhenShowBookmarkBarTrueButDisabled) {
  [bar_ setBookmarkBarEnabled:NO];
  [bar_ updateAndShowNormalBar:YES
               showDetachedBar:NO
                 withAnimation:NO];
  EXPECT_TRUE([bar_ isInState:bookmarks::kShowingState]);
  EXPECT_FALSE([bar_ isInState:bookmarks::kDetachedState]);
  EXPECT_FALSE([bar_ isVisible]);
  EXPECT_FALSE([bar_ isAnimationRunning]);
  EXPECT_TRUE([[bar_ view] isHidden]);
  EXPECT_EQ(0, [resizeDelegate_ height]);
  EXPECT_EQ(0, [[bar_ view] frame].size.height);
}

TEST_F(BookmarkBarControllerTest, ShowOnNewTabPage) {
  [bar_ updateAndShowNormalBar:NO
               showDetachedBar:YES
                 withAnimation:NO];
  EXPECT_FALSE([bar_ isInState:bookmarks::kShowingState]);
  EXPECT_TRUE([bar_ isInState:bookmarks::kDetachedState]);
  EXPECT_TRUE([bar_ isVisible]);
  EXPECT_FALSE([bar_ isAnimationRunning]);
  EXPECT_FALSE([[bar_ view] isHidden]);
  EXPECT_GT([resizeDelegate_ height], 0);
  EXPECT_GT([[bar_ view] frame].size.height, 0);

  // Make sure no buttons fall off the bar, either now or when resized
  // bigger or smaller.
  CGFloat sizes[] = { 300.0, -100.0, 200.0, -420.0 };
  CGFloat previousX = 0.0;
  for (unsigned x = 0; x < arraysize(sizes); x++) {
    // Confirm the buttons moved from the last check (which may be
    // init but that's fine).
    CGFloat newX = [[bar_ offTheSideButton] frame].origin.x;
    EXPECT_NE(previousX, newX);
    previousX = newX;

    // Confirm the buttons have a reasonable bounds. Recall that |-frame|
    // returns rectangles in the superview's coordinates.
    NSRect buttonViewFrame =
        [[bar_ buttonView] convertRect:[[bar_ buttonView] frame]
                              fromView:[[bar_ buttonView] superview]];
    EXPECT_EQ([bar_ buttonView], [[bar_ offTheSideButton] superview]);
    EXPECT_TRUE(NSContainsRect(buttonViewFrame,
                               [[bar_ offTheSideButton] frame]));
    EXPECT_EQ([bar_ buttonView], [[bar_ otherBookmarksButton] superview]);
    EXPECT_TRUE(NSContainsRect(buttonViewFrame,
                               [[bar_ otherBookmarksButton] frame]));

    // Now move them implicitly.
    // We confirm FrameChangeNotification works in the next unit test;
    // we simply assume it works here to resize or reposition the
    // buttons above.
    NSRect frame = [[bar_ view] frame];
    frame.size.width += sizes[x];
    [[bar_ view] setFrame:frame];
  }
}

// Test whether |-updateAndShowNormalBar:...| sets states as we expect. Make
// sure things don't crash.
TEST_F(BookmarkBarControllerTest, StateChanges) {
  // First, go in one-at-a-time cycle.
  [bar_ updateAndShowNormalBar:NO
               showDetachedBar:NO
                 withAnimation:NO];
  EXPECT_EQ(bookmarks::kHiddenState, [bar_ visualState]);
  EXPECT_FALSE([bar_ isVisible]);
  EXPECT_FALSE([bar_ isAnimationRunning]);
  [bar_ updateAndShowNormalBar:YES
               showDetachedBar:NO
                 withAnimation:NO];
  EXPECT_EQ(bookmarks::kShowingState, [bar_ visualState]);
  EXPECT_TRUE([bar_ isVisible]);
  EXPECT_FALSE([bar_ isAnimationRunning]);
  [bar_ updateAndShowNormalBar:YES
               showDetachedBar:YES
                 withAnimation:NO];
  EXPECT_EQ(bookmarks::kShowingState, [bar_ visualState]);
  EXPECT_TRUE([bar_ isVisible]);
  EXPECT_FALSE([bar_ isAnimationRunning]);
  [bar_ updateAndShowNormalBar:NO
               showDetachedBar:YES
                 withAnimation:NO];
  EXPECT_EQ(bookmarks::kDetachedState, [bar_ visualState]);
  EXPECT_TRUE([bar_ isVisible]);
  EXPECT_FALSE([bar_ isAnimationRunning]);

  // Now try some "jumps".
  for (int i = 0; i < 2; i++) {
    [bar_ updateAndShowNormalBar:NO
                 showDetachedBar:NO
                   withAnimation:NO];
    EXPECT_EQ(bookmarks::kHiddenState, [bar_ visualState]);
    EXPECT_FALSE([bar_ isVisible]);
    EXPECT_FALSE([bar_ isAnimationRunning]);
    [bar_ updateAndShowNormalBar:YES
                 showDetachedBar:YES
                   withAnimation:NO];
    EXPECT_EQ(bookmarks::kShowingState, [bar_ visualState]);
    EXPECT_TRUE([bar_ isVisible]);
    EXPECT_FALSE([bar_ isAnimationRunning]);
  }

  // Now try some "jumps".
  for (int i = 0; i < 2; i++) {
    [bar_ updateAndShowNormalBar:YES
                 showDetachedBar:NO
                   withAnimation:NO];
    EXPECT_EQ(bookmarks::kShowingState, [bar_ visualState]);
    EXPECT_TRUE([bar_ isVisible]);
    EXPECT_FALSE([bar_ isAnimationRunning]);
    [bar_ updateAndShowNormalBar:NO
                 showDetachedBar:YES
                   withAnimation:NO];
    EXPECT_EQ(bookmarks::kDetachedState, [bar_ visualState]);
    EXPECT_TRUE([bar_ isVisible]);
    EXPECT_FALSE([bar_ isAnimationRunning]);
  }
}

// Make sure we're watching for frame change notifications.
TEST_F(BookmarkBarControllerTest, FrameChangeNotification) {
  scoped_nsobject<BookmarkBarControllerTogglePong> bar;
  bar.reset(
    [[BookmarkBarControllerTogglePong alloc]
          initWithBrowser:browser()
             initialWidth:100  // arbitrary
                 delegate:nil
           resizeDelegate:resizeDelegate_.get()]);
  InstallAndToggleBar(bar.get());

  // Send a frame did change notification for the pong's view.
  [[NSNotificationCenter defaultCenter]
    postNotificationName:NSViewFrameDidChangeNotification
                  object:[bar view]];

  EXPECT_GT([bar toggles], 0);
}

// Confirm our "no items" container goes away when we add the 1st
// bookmark, and comes back when we delete the bookmark.
TEST_F(BookmarkBarControllerTest, NoItemContainerGoesAway) {
  BookmarkModel* model = profile()->GetBookmarkModel();
  const BookmarkNode* bar = model->bookmark_bar_node();

  [bar_ loaded:model];
  BookmarkBarView* view = [bar_ buttonView];
  DCHECK(view);
  NSView* noItemContainer = [view noItemContainer];
  DCHECK(noItemContainer);

  EXPECT_FALSE([noItemContainer isHidden]);
  const BookmarkNode* node = model->AddURL(bar, bar->child_count(),
                                           ASCIIToUTF16("title"),
                                           GURL("http://www.google.com"));
  EXPECT_TRUE([noItemContainer isHidden]);
  model->Remove(bar, bar->GetIndexOf(node));
  EXPECT_FALSE([noItemContainer isHidden]);

  // Now try it using a bookmark from the Other Bookmarks.
  const BookmarkNode* otherBookmarks = model->other_node();
  node = model->AddURL(otherBookmarks, otherBookmarks->child_count(),
                       ASCIIToUTF16("TheOther"),
                       GURL("http://www.other.com"));
  EXPECT_FALSE([noItemContainer isHidden]);
  // Move it from Other Bookmarks to the bar.
  model->Move(node, bar, 0);
  EXPECT_TRUE([noItemContainer isHidden]);
  // Move it back to Other Bookmarks from the bar.
  model->Move(node, otherBookmarks, 0);
  EXPECT_FALSE([noItemContainer isHidden]);
}

// Confirm off the side button only enabled when reasonable.
TEST_F(BookmarkBarControllerTest, OffTheSideButtonHidden) {
  BookmarkModel* model = profile()->GetBookmarkModel();
  [bar_ setIgnoreAnimations:YES];

  [bar_ loaded:model];
  EXPECT_TRUE([bar_ offTheSideButtonIsHidden]);

  for (int i = 0; i < 2; i++) {
    bookmark_utils::AddIfNotBookmarked(
        model, GURL("http://www.foo.com"), ASCIIToUTF16("small"));
    EXPECT_TRUE([bar_ offTheSideButtonIsHidden]);
  }

  const BookmarkNode* parent = model->bookmark_bar_node();
  for (int i = 0; i < 20; i++) {
    model->AddURL(parent, parent->child_count(),
                  ASCIIToUTF16("super duper wide title"),
                  GURL("http://superfriends.hall-of-justice.edu"));
  }
  EXPECT_FALSE([bar_ offTheSideButtonIsHidden]);

  // Open the "off the side" and start deleting nodes.  Make sure
  // deletion of the last node in "off the side" causes the folder to
  // close.
  EXPECT_FALSE([bar_ offTheSideButtonIsHidden]);
  NSButton* offTheSideButton = [bar_ offTheSideButton];
  // Open "off the side" menu.
  [bar_ openOffTheSideFolderFromButton:offTheSideButton];
  BookmarkBarFolderController* bbfc = [bar_ folderController];
  EXPECT_TRUE(bbfc);
  [bbfc setIgnoreAnimations:YES];
  while (!parent->empty()) {
    // We've completed the job so we're done.
    if ([bar_ offTheSideButtonIsHidden])
      break;
    // Delete the last button.
    model->Remove(parent, parent->child_count() - 1);
    // If last one make sure the menu is closed and the button is hidden.
    // Else make sure menu stays open.
    if ([bar_ offTheSideButtonIsHidden]) {
      EXPECT_FALSE([bar_ folderController]);
    } else {
      EXPECT_TRUE([bar_ folderController]);
    }
  }
}

// http://crbug.com/46175 is a crash when deleting bookmarks from the
// off-the-side menu while it is open.  This test tries to bang hard
// in this area to reproduce the crash.
TEST_F(BookmarkBarControllerTest, DeleteFromOffTheSideWhileItIsOpen) {
  BookmarkModel* model = profile()->GetBookmarkModel();
  [bar_ setIgnoreAnimations:YES];
  [bar_ loaded:model];

  // Add a lot of bookmarks (per the bug).
  const BookmarkNode* parent = model->bookmark_bar_node();
  for (int i = 0; i < 100; i++) {
    std::ostringstream title;
    title << "super duper wide title " << i;
    model->AddURL(parent, parent->child_count(), ASCIIToUTF16(title.str()),
                  GURL("http://superfriends.hall-of-justice.edu"));
  }
  EXPECT_FALSE([bar_ offTheSideButtonIsHidden]);

  // Open "off the side" menu.
  NSButton* offTheSideButton = [bar_ offTheSideButton];
  [bar_ openOffTheSideFolderFromButton:offTheSideButton];
  BookmarkBarFolderController* bbfc = [bar_ folderController];
  EXPECT_TRUE(bbfc);
  [bbfc setIgnoreAnimations:YES];

  // Start deleting items; try and delete randomish ones in case it
  // makes a difference.
  int indices[] = { 2, 4, 5, 1, 7, 9, 2, 0, 10, 9 };
  while (!parent->empty()) {
    for (unsigned int i = 0; i < arraysize(indices); i++) {
      if (indices[i] < parent->child_count()) {
        // First we mouse-enter the button to make things harder.
        NSArray* buttons = [bbfc buttons];
        for (BookmarkButton* button in buttons) {
          if ([button bookmarkNode] == parent->GetChild(indices[i])) {
            [bbfc mouseEnteredButton:button event:nil];
            break;
          }
        }
        // Then we remove the node.  This triggers the button to get
        // deleted.
        model->Remove(parent, indices[i]);
        // Force visual update which is otherwise delayed.
        [[bbfc window] displayIfNeeded];
      }
    }
  }
}

// Test whether |-dragShouldLockBarVisibility| returns NO iff the bar is
// detached.
TEST_F(BookmarkBarControllerTest, TestDragShouldLockBarVisibility) {
  [bar_ updateAndShowNormalBar:NO
               showDetachedBar:NO
                 withAnimation:NO];
  EXPECT_TRUE([bar_ dragShouldLockBarVisibility]);

  [bar_ updateAndShowNormalBar:YES
               showDetachedBar:NO
                 withAnimation:NO];
  EXPECT_TRUE([bar_ dragShouldLockBarVisibility]);

  [bar_ updateAndShowNormalBar:YES
               showDetachedBar:YES
                 withAnimation:NO];
  EXPECT_TRUE([bar_ dragShouldLockBarVisibility]);

  [bar_ updateAndShowNormalBar:NO
               showDetachedBar:YES
                 withAnimation:NO];
  EXPECT_FALSE([bar_ dragShouldLockBarVisibility]);
}

TEST_F(BookmarkBarControllerTest, TagMap) {
  int64 ids[] = { 1, 3, 4, 40, 400, 4000, 800000000, 2, 123456789 };
  std::vector<int32> tags;

  // Generate some tags
  for (unsigned int i = 0; i < arraysize(ids); i++) {
    tags.push_back([bar_ menuTagFromNodeId:ids[i]]);
  }

  // Confirm reverse mapping.
  for (unsigned int i = 0; i < arraysize(ids); i++) {
    EXPECT_EQ(ids[i], [bar_ nodeIdFromMenuTag:tags[i]]);
  }

  // Confirm uniqueness.
  std::sort(tags.begin(), tags.end());
  for (unsigned int i=0; i<(tags.size()-1); i++) {
    EXPECT_NE(tags[i], tags[i+1]);
  }
}

TEST_F(BookmarkBarControllerTest, MenuForFolderNode) {
  BookmarkModel* model = profile()->GetBookmarkModel();

  // First make sure something (e.g. "(empty)" string) is always present.
  NSMenu* menu = [bar_ menuForFolderNode:model->bookmark_bar_node()];
  EXPECT_GT([menu numberOfItems], 0);

  // Test two bookmarks.
  GURL gurl("http://www.foo.com");
  bookmark_utils::AddIfNotBookmarked(model, gurl, ASCIIToUTF16("small"));
  bookmark_utils::AddIfNotBookmarked(
      model, GURL("http://www.cnn.com"), ASCIIToUTF16("bigger title"));
  menu = [bar_ menuForFolderNode:model->bookmark_bar_node()];
  EXPECT_EQ([menu numberOfItems], 2);
  NSMenuItem *item = [menu itemWithTitle:@"bigger title"];
  EXPECT_TRUE(item);
  item = [menu itemWithTitle:@"small"];
  EXPECT_TRUE(item);
  if (item) {
    int64 tag = [bar_ nodeIdFromMenuTag:[item tag]];
    const BookmarkNode* node = model->GetNodeByID(tag);
    EXPECT_TRUE(node);
    EXPECT_EQ(gurl, node->url());
  }

  // Test with an actual folder as well
  const BookmarkNode* parent = model->bookmark_bar_node();
  const BookmarkNode* folder = model->AddFolder(parent,
                                                parent->child_count(),
                                                ASCIIToUTF16("folder"));
  model->AddURL(folder, folder->child_count(),
                ASCIIToUTF16("f1"), GURL("http://framma-lamma.com"));
  model->AddURL(folder, folder->child_count(),
                ASCIIToUTF16("f2"), GURL("http://framma-lamma-ding-dong.com"));
  menu = [bar_ menuForFolderNode:model->bookmark_bar_node()];
  EXPECT_EQ([menu numberOfItems], 3);

  item = [menu itemWithTitle:@"folder"];
  EXPECT_TRUE(item);
  EXPECT_TRUE([item hasSubmenu]);
  NSMenu *submenu = [item submenu];
  EXPECT_TRUE(submenu);
  EXPECT_EQ(2, [submenu numberOfItems]);
  EXPECT_TRUE([submenu itemWithTitle:@"f1"]);
  EXPECT_TRUE([submenu itemWithTitle:@"f2"]);
}

// Confirm openBookmark: forwards the request to the controller's delegate
TEST_F(BookmarkBarControllerTest, OpenBookmark) {
  GURL gurl("http://walla.walla.ding.dong.com");
  scoped_ptr<BookmarkNode> node(new BookmarkNode(gurl));

  scoped_nsobject<BookmarkButtonCell> cell([[BookmarkButtonCell alloc] init]);
  [cell setBookmarkNode:node.get()];
  scoped_nsobject<BookmarkButton> button([[BookmarkButton alloc] init]);
  [button setCell:cell.get()];
  [cell setRepresentedObject:[NSValue valueWithPointer:node.get()]];

  [bar_ openBookmark:button];
  EXPECT_EQ(noOpenBar()->urls_[0], node->url());
  EXPECT_EQ(noOpenBar()->dispositions_[0], CURRENT_TAB);
}

// Confirm opening of bookmarks works from the menus (different
// dispositions than clicking on the button).
TEST_F(BookmarkBarControllerTest, OpenBookmarkFromMenus) {
  const char* urls[] = { "http://walla.walla.ding.dong.com",
                         "http://i_dont_know.com",
                         "http://cee.enn.enn.dot.com" };
  SEL selectors[] = { @selector(openBookmarkInNewForegroundTab:),
                      @selector(openBookmarkInNewWindow:),
                      @selector(openBookmarkInIncognitoWindow:) };
  WindowOpenDisposition dispositions[] = { NEW_FOREGROUND_TAB,
                                           NEW_WINDOW,
                                           OFF_THE_RECORD };
  for (unsigned int i = 0; i < arraysize(dispositions); i++) {
    GURL gurl(urls[i]);
    [bar_ performSelector:selectors[i]
               withObject:ItemForBookmarkBarMenu(gurl)];
    EXPECT_EQ(noOpenBar()->urls_[0], gurl);
    EXPECT_EQ(noOpenBar()->dispositions_[0], dispositions[i]);
    [bar_ clear];
  }
}

TEST_F(BookmarkBarControllerTest, TestAddRemoveAndClear) {
  BookmarkModel* model = profile()->GetBookmarkModel();
  NSView* buttonView = [bar_ buttonView];
  EXPECT_EQ(0U, [[bar_ buttons] count]);
  unsigned int initial_subview_count = [[buttonView subviews] count];

  // Make sure a redundant call doesn't choke
  [bar_ clearBookmarkBar];
  EXPECT_EQ(0U, [[bar_ buttons] count]);
  EXPECT_EQ(initial_subview_count, [[buttonView subviews] count]);

  GURL gurl1("http://superfriends.hall-of-justice.edu");
  // Short titles increase the chances of this test succeeding if the view is
  // narrow.
  // TODO(viettrungluu): make the test independent of window/view size, font
  // metrics, button size and spacing, and everything else.
  string16 title1(ASCIIToUTF16("x"));
  bookmark_utils::AddIfNotBookmarked(model, gurl1, title1);
  EXPECT_EQ(1U, [[bar_ buttons] count]);
  EXPECT_EQ(1+initial_subview_count, [[buttonView subviews] count]);

  GURL gurl2("http://legion-of-doom.gov");
  string16 title2(ASCIIToUTF16("y"));
  bookmark_utils::AddIfNotBookmarked(model, gurl2, title2);
  EXPECT_EQ(2U, [[bar_ buttons] count]);
  EXPECT_EQ(2+initial_subview_count, [[buttonView subviews] count]);

  for (int i = 0; i < 3; i++) {
    bookmark_utils::RemoveAllBookmarks(model, gurl2);
    EXPECT_EQ(1U, [[bar_ buttons] count]);
    EXPECT_EQ(1+initial_subview_count, [[buttonView subviews] count]);

    // and bring it back
    bookmark_utils::AddIfNotBookmarked(model, gurl2, title2);
    EXPECT_EQ(2U, [[bar_ buttons] count]);
    EXPECT_EQ(2+initial_subview_count, [[buttonView subviews] count]);
  }

  [bar_ clearBookmarkBar];
  EXPECT_EQ(0U, [[bar_ buttons] count]);
  EXPECT_EQ(initial_subview_count, [[buttonView subviews] count]);

  // Explicit test of loaded: since this is a convenient spot
  [bar_ loaded:model];
  EXPECT_EQ(2U, [[bar_ buttons] count]);
  EXPECT_EQ(2+initial_subview_count, [[buttonView subviews] count]);
}

// Make sure we don't create too many buttons; we only really need
// ones that will be visible.
TEST_F(BookmarkBarControllerTest, TestButtonLimits) {
  BookmarkModel* model = profile()->GetBookmarkModel();
  EXPECT_EQ(0U, [[bar_ buttons] count]);
  // Add one; make sure we see it.
  const BookmarkNode* parent = model->bookmark_bar_node();
  model->AddURL(parent, parent->child_count(),
                ASCIIToUTF16("title"), GURL("http://www.google.com"));
  EXPECT_EQ(1U, [[bar_ buttons] count]);

  // Add 30 which we expect to be 'too many'.  Make sure we don't see
  // 30 buttons.
  model->Remove(parent, 0);
  EXPECT_EQ(0U, [[bar_ buttons] count]);
  for (int i=0; i<30; i++) {
    model->AddURL(parent, parent->child_count(),
                  ASCIIToUTF16("title"), GURL("http://www.google.com"));
  }
  int count = [[bar_ buttons] count];
  EXPECT_LT(count, 30L);

  // Add 10 more (to the front of the list so the on-screen buttons
  // would change) and make sure the count stays the same.
  for (int i=0; i<10; i++) {
    model->AddURL(parent, 0,  /* index is 0, so front, not end */
                  ASCIIToUTF16("title"), GURL("http://www.google.com"));
  }

  // Finally, grow the view and make sure the button count goes up.
  NSRect frame = [[bar_ view] frame];
  frame.size.width += 600;
  [[bar_ view] setFrame:frame];
  int finalcount = [[bar_ buttons] count];
  EXPECT_GT(finalcount, count);
}

// Make sure that each button we add marches to the right and does not
// overlap with the previous one.
TEST_F(BookmarkBarControllerTest, TestButtonMarch) {
  scoped_nsobject<NSMutableArray> cells([[NSMutableArray alloc] init]);

  CGFloat widths[] = { 10, 10, 100, 10, 500, 500, 80000, 60000, 1, 345 };
  for (unsigned int i = 0; i < arraysize(widths); i++) {
    NSCell* cell = [[CellWithDesiredSize alloc]
                     initTextCell:@"foo"
                      desiredSize:NSMakeSize(widths[i], 30)];
    [cells addObject:cell];
    [cell release];
  }

  int x_offset = 0;
  CGFloat x_end = x_offset;  // end of the previous button
  for (unsigned int i = 0; i < arraysize(widths); i++) {
    NSRect r = [bar_ frameForBookmarkButtonFromCell:[cells objectAtIndex:i]
                                            xOffset:&x_offset];
    EXPECT_GE(r.origin.x, x_end);
    x_end = NSMaxX(r);
  }
}

TEST_F(BookmarkBarControllerTest, CheckForGrowth) {
  WithNoAnimation at_all; // Turn off Cocoa auto animation in this scope.
  BookmarkModel* model = profile()->GetBookmarkModel();
  GURL gurl1("http://www.google.com");
  string16 title1(ASCIIToUTF16("x"));
  bookmark_utils::AddIfNotBookmarked(model, gurl1, title1);

  GURL gurl2("http://www.google.com/blah");
  string16 title2(ASCIIToUTF16("y"));
  bookmark_utils::AddIfNotBookmarked(model, gurl2, title2);

  EXPECT_EQ(2U, [[bar_ buttons] count]);
  CGFloat width_1 = [[[bar_ buttons] objectAtIndex:0] frame].size.width;
  CGFloat x_2 = [[[bar_ buttons] objectAtIndex:1] frame].origin.x;

  NSButton* first = [[bar_ buttons] objectAtIndex:0];
  [[first cell] setTitle:@"This is a really big title; watch out mom!"];
  [bar_ checkForBookmarkButtonGrowth:first];

  // Make sure the 1st button is now wider, the 2nd one is moved over,
  // and they don't overlap.
  NSRect frame_1 = [[[bar_ buttons] objectAtIndex:0] frame];
  NSRect frame_2 = [[[bar_ buttons] objectAtIndex:1] frame];
  EXPECT_GT(frame_1.size.width, width_1);
  EXPECT_GT(frame_2.origin.x, x_2);
  EXPECT_GE(frame_2.origin.x, frame_1.origin.x + frame_1.size.width);
}

TEST_F(BookmarkBarControllerTest, DeleteBookmark) {
  BookmarkModel* model = profile()->GetBookmarkModel();

  const char* urls[] = { "https://secret.url.com",
                         "http://super.duper.web.site.for.doodz.gov",
                         "http://www.foo-bar-baz.com/" };
  const BookmarkNode* parent = model->bookmark_bar_node();
  for (unsigned int i = 0; i < arraysize(urls); i++) {
    model->AddURL(parent, parent->child_count(),
                  ASCIIToUTF16("title"), GURL(urls[i]));
  }
  EXPECT_EQ(3, parent->child_count());
  const BookmarkNode* middle_node = parent->GetChild(1);

  NSMenuItem* item = ItemForBookmarkBarMenu(middle_node);
  [bar_ deleteBookmark:item];
  EXPECT_EQ(2, parent->child_count());
  EXPECT_EQ(parent->GetChild(0)->url(), GURL(urls[0]));
  // node 2 moved into spot 1
  EXPECT_EQ(parent->GetChild(1)->url(), GURL(urls[2]));
}

// TODO(jrg): write a test to confirm that nodeFaviconLoaded calls
// checkForBookmarkButtonGrowth:.

TEST_F(BookmarkBarControllerTest, Cell) {
  BookmarkModel* model = profile()->GetBookmarkModel();
  [bar_ loaded:model];

  const BookmarkNode* parent = model->bookmark_bar_node();
  model->AddURL(parent, parent->child_count(),
                ASCIIToUTF16("supertitle"),
                GURL("http://superfriends.hall-of-justice.edu"));
  const BookmarkNode* node = parent->GetChild(0);

  NSCell* cell = [bar_ cellForBookmarkNode:node];
  EXPECT_TRUE(cell);
  EXPECT_NSEQ(@"supertitle", [cell title]);
  EXPECT_EQ(node, [[cell representedObject] pointerValue]);
  EXPECT_TRUE([cell menu]);

  // Empty cells have no menu.
  cell = [bar_ cellForBookmarkNode:nil];
  EXPECT_FALSE([cell menu]);
  // Even empty cells have a title (of "(empty)")
  EXPECT_TRUE([cell title]);

  // cell is autoreleased; no need to release here
}

// Test drawing, mostly to ensure nothing leaks or crashes.
TEST_F(BookmarkBarControllerTest, Display) {
  [[bar_ view] display];
}

// Test that middle clicking on a bookmark button results in an open action.
TEST_F(BookmarkBarControllerTest, MiddleClick) {
  BookmarkModel* model = profile()->GetBookmarkModel();
  GURL gurl1("http://www.google.com/");
  string16 title1(ASCIIToUTF16("x"));
  bookmark_utils::AddIfNotBookmarked(model, gurl1, title1);

  EXPECT_EQ(1U, [[bar_ buttons] count]);
  NSButton* first = [[bar_ buttons] objectAtIndex:0];
  EXPECT_TRUE(first);

  [first otherMouseUp:
      cocoa_test_event_utils::MakeMouseEvent(NSOtherMouseUp, 0)];
  EXPECT_EQ(noOpenBar()->urls_.size(), 1U);
}

TEST_F(BookmarkBarControllerTest, DisplaysHelpMessageOnEmpty) {
  BookmarkModel* model = profile()->GetBookmarkModel();
  [bar_ loaded:model];
  EXPECT_FALSE([[[bar_ buttonView] noItemContainer] isHidden]);
}

TEST_F(BookmarkBarControllerTest, HidesHelpMessageWithBookmark) {
  BookmarkModel* model = profile()->GetBookmarkModel();

  const BookmarkNode* parent = model->bookmark_bar_node();
  model->AddURL(parent, parent->child_count(),
                ASCIIToUTF16("title"), GURL("http://one.com"));

  [bar_ loaded:model];
  EXPECT_TRUE([[[bar_ buttonView] noItemContainer] isHidden]);
}

TEST_F(BookmarkBarControllerTest, BookmarkButtonSizing) {
  BookmarkModel* model = profile()->GetBookmarkModel();

  const BookmarkNode* parent = model->bookmark_bar_node();
  model->AddURL(parent, parent->child_count(),
                ASCIIToUTF16("title"), GURL("http://one.com"));

  [bar_ loaded:model];

  // Make sure the internal bookmark button also is the correct height.
  NSArray* buttons = [bar_ buttons];
  EXPECT_GT([buttons count], 0u);
  for (NSButton* button in buttons) {
    EXPECT_FLOAT_EQ(
        (bookmarks::kBookmarkBarHeight + bookmarks::kVisualHeightOffset) - 2 *
                    bookmarks::kBookmarkVerticalPadding,
        [button frame].size.height);
  }
}

TEST_F(BookmarkBarControllerTest, DropBookmarks) {
  const char* urls[] = {
    "http://qwantz.com",
    "http://xkcd.com",
    "javascript:alert('lolwut')",
    "file://localhost/tmp/local-file.txt"  // As if dragged from the desktop.
  };
  const char* titles[] = {
    "Philosophoraptor",
    "Can't draw",
    "Inspiration",
    "Frum stuf"
  };
  EXPECT_EQ(arraysize(urls), arraysize(titles));

  NSMutableArray* nsurls = [NSMutableArray array];
  NSMutableArray* nstitles = [NSMutableArray array];
  for (size_t i = 0; i < arraysize(urls); ++i) {
    [nsurls addObject:base::SysUTF8ToNSString(urls[i])];
    [nstitles addObject:base::SysUTF8ToNSString(titles[i])];
  }

  BookmarkModel* model = profile()->GetBookmarkModel();
  const BookmarkNode* parent = model->bookmark_bar_node();
  [bar_ addURLs:nsurls withTitles:nstitles at:NSZeroPoint];
  EXPECT_EQ(4, parent->child_count());
  for (int i = 0; i < parent->child_count(); ++i) {
    GURL gurl = parent->GetChild(i)->url();
    if (gurl.scheme() == "http" ||
        gurl.scheme() == "javascript") {
      EXPECT_EQ(parent->GetChild(i)->url(), GURL(urls[i]));
    } else {
      // Be flexible if the scheme needed to be added.
      std::string gurl_string = gurl.spec();
      std::string my_string = parent->GetChild(i)->url().spec();
      EXPECT_NE(gurl_string.find(my_string), std::string::npos);
    }
    EXPECT_EQ(parent->GetChild(i)->GetTitle(), ASCIIToUTF16(titles[i]));
  }
}

TEST_F(BookmarkBarControllerTest, TestButtonOrBar) {
  BookmarkModel* model = profile()->GetBookmarkModel();
  GURL gurl1("http://www.google.com");
  string16 title1(ASCIIToUTF16("x"));
  bookmark_utils::AddIfNotBookmarked(model, gurl1, title1);

  GURL gurl2("http://www.google.com/gurl_power");
  string16 title2(ASCIIToUTF16("gurl power"));
  bookmark_utils::AddIfNotBookmarked(model, gurl2, title2);

  NSButton* first = [[bar_ buttons] objectAtIndex:0];
  NSButton* second = [[bar_ buttons] objectAtIndex:1];
  EXPECT_TRUE(first && second);

  NSMenuItem* menuItem = [[[first cell] menu] itemAtIndex:0];
  const BookmarkNode* node = [bar_ nodeFromMenuItem:menuItem];
  EXPECT_TRUE(node);
  EXPECT_EQ(node, model->bookmark_bar_node()->GetChild(0));

  menuItem = [[[second cell] menu] itemAtIndex:0];
  node = [bar_ nodeFromMenuItem:menuItem];
  EXPECT_TRUE(node);
  EXPECT_EQ(node, model->bookmark_bar_node()->GetChild(1));

  menuItem = [[[bar_ view] menu] itemAtIndex:0];
  node = [bar_ nodeFromMenuItem:menuItem];
  EXPECT_TRUE(node);
  EXPECT_EQ(node, model->bookmark_bar_node());
}

TEST_F(BookmarkBarControllerTest, TestMenuNodeAndDisable) {
  BookmarkModel* model = profile()->GetBookmarkModel();
  const BookmarkNode* parent = model->bookmark_bar_node();
  const BookmarkNode* folder = model->AddFolder(parent,
                                                parent->child_count(),
                                                ASCIIToUTF16("folder"));
  NSButton* button = [[bar_ buttons] objectAtIndex:0];
  EXPECT_TRUE(button);

  // Confirm the menu knows which node it is talking about
  BookmarkMenu* menu = static_cast<BookmarkMenu*>([[button cell] menu]);
  EXPECT_TRUE(menu);
  EXPECT_TRUE([menu isKindOfClass:[BookmarkMenu class]]);
  EXPECT_EQ(folder->id(), [menu id]);

  // Make sure "Open All" is disabled (nothing to open -- no children!)
  // (Assumes "Open All" is the 1st item)
  NSMenuItem* item = [menu itemAtIndex:0];
  EXPECT_FALSE([bar_ validateUserInterfaceItem:item]);

  // Now add a child and make sure the item would be enabled.
  model->AddURL(folder, folder->child_count(),
                ASCIIToUTF16("super duper wide title"),
                GURL("http://superfriends.hall-of-justice.edu"));
  EXPECT_TRUE([bar_ validateUserInterfaceItem:item]);
}

TEST_F(BookmarkBarControllerTest, TestDragButton) {
  WithNoAnimation at_all;
  BookmarkModel* model = profile()->GetBookmarkModel();

  GURL gurls[] = { GURL("http://www.google.com/a"),
                   GURL("http://www.google.com/b"),
                   GURL("http://www.google.com/c") };
  string16 titles[] = { ASCIIToUTF16("a"),
                        ASCIIToUTF16("b"),
                        ASCIIToUTF16("c") };
  for (unsigned i = 0; i < arraysize(titles); i++)
    bookmark_utils::AddIfNotBookmarked(model, gurls[i], titles[i]);

  EXPECT_EQ([[bar_ buttons] count], arraysize(titles));
  EXPECT_NSEQ(@"a", [[[bar_ buttons] objectAtIndex:0] title]);

  [bar_ dragButton:[[bar_ buttons] objectAtIndex:2]
                to:NSMakePoint(0, 0)
              copy:NO];
  EXPECT_NSEQ(@"c", [[[bar_ buttons] objectAtIndex:0] title]);
  // Make sure a 'copy' did not happen.
  EXPECT_EQ([[bar_ buttons] count], arraysize(titles));

  [bar_ dragButton:[[bar_ buttons] objectAtIndex:1]
                to:NSMakePoint(1000, 0)
              copy:NO];
  EXPECT_NSEQ(@"c", [[[bar_ buttons] objectAtIndex:0] title]);
  EXPECT_NSEQ(@"b", [[[bar_ buttons] objectAtIndex:1] title]);
  EXPECT_NSEQ(@"a", [[[bar_ buttons] objectAtIndex:2] title]);
  EXPECT_EQ([[bar_ buttons] count], arraysize(titles));

  // A drop of the 1st between the next 2.
  CGFloat x = NSMinX([[[bar_ buttons] objectAtIndex:2] frame]);
  x += [[bar_ view] frame].origin.x;
  [bar_ dragButton:[[bar_ buttons] objectAtIndex:0]
                to:NSMakePoint(x, 0)
              copy:NO];
  EXPECT_NSEQ(@"b", [[[bar_ buttons] objectAtIndex:0] title]);
  EXPECT_NSEQ(@"c", [[[bar_ buttons] objectAtIndex:1] title]);
  EXPECT_NSEQ(@"a", [[[bar_ buttons] objectAtIndex:2] title]);
  EXPECT_EQ([[bar_ buttons] count], arraysize(titles));

  // A drop on a non-folder button.  (Shouldn't try and go in it.)
  x = NSMidX([[[bar_ buttons] objectAtIndex:0] frame]);
  x += [[bar_ view] frame].origin.x;
  [bar_ dragButton:[[bar_ buttons] objectAtIndex:2]
                to:NSMakePoint(x, 0)
              copy:NO];
  EXPECT_EQ(arraysize(titles), [[bar_ buttons] count]);

  // A drop on a folder button.
  const BookmarkNode* folder = model->AddFolder(
      model->bookmark_bar_node(), 0, ASCIIToUTF16("awesome folder"));
  DCHECK(folder);
  model->AddURL(folder, 0, ASCIIToUTF16("already"),
                GURL("http://www.google.com"));
  EXPECT_EQ(arraysize(titles) + 1, [[bar_ buttons] count]);
  EXPECT_EQ(1, folder->child_count());
  x = NSMidX([[[bar_ buttons] objectAtIndex:0] frame]);
  x += [[bar_ view] frame].origin.x;
  string16 title = [[[bar_ buttons] objectAtIndex:2] bookmarkNode]->GetTitle();
  [bar_ dragButton:[[bar_ buttons] objectAtIndex:2]
                to:NSMakePoint(x, 0)
              copy:NO];
  // Gone from the bar
  EXPECT_EQ(arraysize(titles), [[bar_ buttons] count]);
  // In the folder
  EXPECT_EQ(2, folder->child_count());
  // At the end
  EXPECT_EQ(title, folder->GetChild(1)->GetTitle());
}

TEST_F(BookmarkBarControllerTest, TestCopyButton) {
  BookmarkModel* model = profile()->GetBookmarkModel();

  GURL gurls[] = { GURL("http://www.google.com/a"),
                   GURL("http://www.google.com/b"),
                   GURL("http://www.google.com/c") };
  string16 titles[] = { ASCIIToUTF16("a"),
                        ASCIIToUTF16("b"),
                        ASCIIToUTF16("c") };
  for (unsigned i = 0; i < arraysize(titles); i++)
    bookmark_utils::AddIfNotBookmarked(model, gurls[i], titles[i]);

  EXPECT_EQ([[bar_ buttons] count], arraysize(titles));
  EXPECT_NSEQ(@"a", [[[bar_ buttons] objectAtIndex:0] title]);

  // Drag 'a' between 'b' and 'c'.
  CGFloat x = NSMinX([[[bar_ buttons] objectAtIndex:2] frame]);
  x += [[bar_ view] frame].origin.x;
  [bar_ dragButton:[[bar_ buttons] objectAtIndex:0]
                to:NSMakePoint(x, 0)
              copy:YES];
  EXPECT_NSEQ(@"a", [[[bar_ buttons] objectAtIndex:0] title]);
  EXPECT_NSEQ(@"b", [[[bar_ buttons] objectAtIndex:1] title]);
  EXPECT_NSEQ(@"a", [[[bar_ buttons] objectAtIndex:2] title]);
  EXPECT_NSEQ(@"c", [[[bar_ buttons] objectAtIndex:3] title]);
  EXPECT_EQ([[bar_ buttons] count], 4U);
}

// Fake a theme with colored text.  Apply it and make sure bookmark
// buttons have the same colored text.  Repeat more than once.
TEST_F(BookmarkBarControllerTest, TestThemedButton) {
  BookmarkModel* model = profile()->GetBookmarkModel();
  bookmark_utils::AddIfNotBookmarked(
      model, GURL("http://www.foo.com"), ASCIIToUTF16("small"));
  BookmarkButton* button = [[bar_ buttons] objectAtIndex:0];
  EXPECT_TRUE(button);

  NSArray* colors = [NSArray arrayWithObjects:[NSColor redColor],
                                              [NSColor blueColor],
                                              nil];
  for (NSColor* color in colors) {
    FakeTheme theme(color);
    [bar_ updateTheme:&theme];
    NSAttributedString* astr = [button attributedTitle];
    EXPECT_TRUE(astr);
    EXPECT_NSEQ(@"small", [astr string]);
    // Pick a char in the middle to test (index 3)
    NSDictionary* attributes = [astr attributesAtIndex:3 effectiveRange:NULL];
    NSColor* newColor =
        [attributes objectForKey:NSForegroundColorAttributeName];
    EXPECT_NSEQ(newColor, color);
  }
}

// Test that delegates and targets of buttons are cleared on dealloc.
TEST_F(BookmarkBarControllerTest, TestClearOnDealloc) {
  // Make some bookmark buttons.
  BookmarkModel* model = profile()->GetBookmarkModel();
  GURL gurls[] = { GURL("http://www.foo.com/"),
                   GURL("http://www.bar.com/"),
                   GURL("http://www.baz.com/") };
  string16 titles[] = { ASCIIToUTF16("a"),
                        ASCIIToUTF16("b"),
                        ASCIIToUTF16("c") };
  for (size_t i = 0; i < arraysize(titles); i++)
    bookmark_utils::AddIfNotBookmarked(model, gurls[i], titles[i]);

  // Get and retain the buttons so we can examine them after dealloc.
  scoped_nsobject<NSArray> buttons([[bar_ buttons] retain]);
  EXPECT_EQ([buttons count], arraysize(titles));

  // Make sure that everything is set.
  for (BookmarkButton* button in buttons.get()) {
    ASSERT_TRUE([button isKindOfClass:[BookmarkButton class]]);
    EXPECT_TRUE([button delegate]);
    EXPECT_TRUE([button target]);
    EXPECT_TRUE([button action]);
  }

  // This will dealloc....
  bar_.reset();

  // Make sure that everything is cleared.
  for (BookmarkButton* button in buttons.get()) {
    EXPECT_FALSE([button delegate]);
    EXPECT_FALSE([button target]);
    EXPECT_FALSE([button action]);
  }
}

TEST_F(BookmarkBarControllerTest, TestFolders) {
  BookmarkModel* model = profile()->GetBookmarkModel();

  // Create some folder buttons.
  const BookmarkNode* parent = model->bookmark_bar_node();
  const BookmarkNode* folder = model->AddFolder(parent,
                                                parent->child_count(),
                                                ASCIIToUTF16("folder"));
  model->AddURL(folder, folder->child_count(),
                ASCIIToUTF16("f1"), GURL("http://framma-lamma.com"));
  folder = model->AddFolder(parent, parent->child_count(),
                            ASCIIToUTF16("empty"));

  EXPECT_EQ([[bar_ buttons] count], 2U);

  // First confirm mouseEntered does nothing if "menus" aren't active.
  NSEvent* event = cocoa_test_event_utils::MakeMouseEvent(NSOtherMouseUp, 0);
  [bar_ mouseEnteredButton:[[bar_ buttons] objectAtIndex:0] event:event];
  EXPECT_FALSE([bar_ folderController]);

  // Make one active.  Entering it is now a no-op.
  [bar_ openBookmarkFolderFromButton:[[bar_ buttons] objectAtIndex:0]];
  BookmarkBarFolderController* bbfc = [bar_ folderController];
  EXPECT_TRUE(bbfc);
  [bar_ mouseEnteredButton:[[bar_ buttons] objectAtIndex:0] event:event];
  EXPECT_EQ(bbfc, [bar_ folderController]);

  // Enter a different one; a new folderController is active.
  [bar_ mouseEnteredButton:[[bar_ buttons] objectAtIndex:1] event:event];
  EXPECT_NE(bbfc, [bar_ folderController]);

  // Confirm exited is a no-op.
  [bar_ mouseExitedButton:[[bar_ buttons] objectAtIndex:1] event:event];
  EXPECT_NE(bbfc, [bar_ folderController]);

  // Clean up.
  [bar_ closeBookmarkFolder:nil];
}

// Verify that the folder menu presentation properly tracks mouse movements
// over the bar. Until there is a click no folder menus should show. After a
// click on a folder folder menus should show until another click on a folder
// button, and a click outside the bar and its folder menus.
TEST_F(BookmarkBarControllerTest, TestFolderButtons) {
  BookmarkModel& model(*profile()->GetBookmarkModel());
  const BookmarkNode* root = model.bookmark_bar_node();
  const std::string model_string("1b 2f:[ 2f1b 2f2b ] 3b 4f:[ 4f1b 4f2b ] ");
  model_test_utils::AddNodesFromModelString(model, root, model_string);

  // Validate initial model and that we do not have a folder controller.
  std::string actualModelString = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ(model_string, actualModelString);
  EXPECT_FALSE([bar_ folderController]);

  // Add a real bookmark so we can click on it.
  const BookmarkNode* folder = root->GetChild(3);
  model.AddURL(folder, folder->child_count(), ASCIIToUTF16("CLICK ME"),
               GURL("http://www.google.com/"));

  // Click on a folder button.
  BookmarkButton* button = [bar_ buttonWithTitleEqualTo:@"4f"];
  EXPECT_TRUE(button);
  [bar_ openBookmarkFolderFromButton:button];
  BookmarkBarFolderController* bbfc = [bar_ folderController];
  EXPECT_TRUE(bbfc);

  // Make sure a 2nd click on the same button closes things.
  [bar_ openBookmarkFolderFromButton:button];
  EXPECT_FALSE([bar_ folderController]);

  // Next open is a different button.
  button = [bar_ buttonWithTitleEqualTo:@"2f"];
  EXPECT_TRUE(button);
  [bar_ openBookmarkFolderFromButton:button];
  EXPECT_TRUE([bar_ folderController]);

  // Mouse over a non-folder button and confirm controller has gone away.
  button = [bar_ buttonWithTitleEqualTo:@"1b"];
  EXPECT_TRUE(button);
  NSEvent* event = cocoa_test_event_utils::MouseEventAtPoint([button center],
                                                             NSMouseMoved, 0);
  [bar_ mouseEnteredButton:button event:event];
  EXPECT_FALSE([bar_ folderController]);

  // Mouse over the original folder and confirm a new controller.
  button = [bar_ buttonWithTitleEqualTo:@"2f"];
  EXPECT_TRUE(button);
  [bar_ mouseEnteredButton:button event:event];
  BookmarkBarFolderController* oldBBFC = [bar_ folderController];
  EXPECT_TRUE(oldBBFC);

  // 'Jump' over to a different folder and confirm a new controller.
  button = [bar_ buttonWithTitleEqualTo:@"4f"];
  EXPECT_TRUE(button);
  [bar_ mouseEnteredButton:button event:event];
  BookmarkBarFolderController* newBBFC = [bar_ folderController];
  EXPECT_TRUE(newBBFC);
  EXPECT_NE(oldBBFC, newBBFC);
}

// Make sure the "off the side" folder looks like a bookmark folder
// but only contains "off the side" items.
TEST_F(BookmarkBarControllerTest, OffTheSideFolder) {

  // It starts hidden.
  EXPECT_TRUE([bar_ offTheSideButtonIsHidden]);

  // Create some buttons.
  BookmarkModel* model = profile()->GetBookmarkModel();
  const BookmarkNode* parent = model->bookmark_bar_node();
  for (int x = 0; x < 30; x++) {
    model->AddURL(parent, parent->child_count(),
                  ASCIIToUTF16("medium-size-title"),
                  GURL("http://framma-lamma.com"));
  }
  // Add a couple more so we can delete one and make sure its button goes away.
  model->AddURL(parent, parent->child_count(),
                ASCIIToUTF16("DELETE_ME"), GURL("http://ashton-tate.com"));
  model->AddURL(parent, parent->child_count(),
                ASCIIToUTF16("medium-size-title"),
                GURL("http://framma-lamma.com"));

  // Should no longer be hidden.
  EXPECT_FALSE([bar_ offTheSideButtonIsHidden]);

  // Open it; make sure we have a folder controller.
  EXPECT_FALSE([bar_ folderController]);
  [bar_ openOffTheSideFolderFromButton:[bar_ offTheSideButton]];
  BookmarkBarFolderController* bbfc = [bar_ folderController];
  EXPECT_TRUE(bbfc);

  // Confirm the contents are only buttons which fell off the side by
  // making sure that none of the nodes in the off-the-side folder are
  // found in bar buttons.  Be careful since not all the bar buttons
  // may be currently displayed.
  NSArray* folderButtons = [bbfc buttons];
  NSArray* barButtons = [bar_ buttons];
  for (BookmarkButton* folderButton in folderButtons) {
    for (BookmarkButton* barButton in barButtons) {
      if ([barButton superview]) {
        EXPECT_NE([folderButton bookmarkNode], [barButton bookmarkNode]);
      }
    }
  }

  // Delete a bookmark in the off-the-side and verify it's gone.
  BookmarkButton* button = [bbfc buttonWithTitleEqualTo:@"DELETE_ME"];
  EXPECT_TRUE(button);
  model->Remove(parent, parent->child_count() - 2);
  button = [bbfc buttonWithTitleEqualTo:@"DELETE_ME"];
  EXPECT_FALSE(button);
}

TEST_F(BookmarkBarControllerTest, EventToExitCheck) {
  NSEvent* event = cocoa_test_event_utils::MakeMouseEvent(NSMouseMoved, 0);
  EXPECT_FALSE([bar_ isEventAnExitEvent:event]);

  BookmarkBarFolderWindow* folderWindow = [[[BookmarkBarFolderWindow alloc]
                                             init] autorelease];
  [[[bar_ view] window] addChildWindow:folderWindow
                               ordered:NSWindowAbove];
  event = cocoa_test_event_utils::LeftMouseDownAtPointInWindow(NSMakePoint(1,1),
                                                               folderWindow);
  EXPECT_FALSE([bar_ isEventAnExitEvent:event]);

  event = cocoa_test_event_utils::LeftMouseDownAtPointInWindow(
      NSMakePoint(100,100), test_window());
  EXPECT_TRUE([bar_ isEventAnExitEvent:event]);

  // Many components are arbitrary (e.g. location, keycode).
  event = [NSEvent keyEventWithType:NSKeyDown
                           location:NSMakePoint(1,1)
                      modifierFlags:0
                          timestamp:0
                       windowNumber:0
                            context:nil
                         characters:@"x"
        charactersIgnoringModifiers:@"x"
                          isARepeat:NO
                            keyCode:87];
  EXPECT_FALSE([bar_ isEventAnExitEvent:event]);

  [[[bar_ view] window] removeChildWindow:folderWindow];
}

TEST_F(BookmarkBarControllerTest, DropDestination) {
  // Make some buttons.
  BookmarkModel* model = profile()->GetBookmarkModel();
  const BookmarkNode* parent = model->bookmark_bar_node();
  model->AddFolder(parent, parent->child_count(), ASCIIToUTF16("folder 1"));
  model->AddFolder(parent, parent->child_count(), ASCIIToUTF16("folder 2"));
  EXPECT_EQ([[bar_ buttons] count], 2U);

  // Confirm "off to left" and "off to right" match nothing.
  NSPoint p = NSMakePoint(-1, 2);
  EXPECT_FALSE([bar_ buttonForDroppingOnAtPoint:p]);
  EXPECT_TRUE([bar_ shouldShowIndicatorShownForPoint:p]);
  p = NSMakePoint(50000, 10);
  EXPECT_FALSE([bar_ buttonForDroppingOnAtPoint:p]);
  EXPECT_TRUE([bar_ shouldShowIndicatorShownForPoint:p]);

  // Confirm "right in the center" (give or take a pixel) is a match,
  // and confirm "just barely in the button" is not.  Anything more
  // specific seems likely to be tweaked.
  CGFloat viewFrameXOffset = [[bar_ view] frame].origin.x;
  for (BookmarkButton* button in [bar_ buttons]) {
    CGFloat x = NSMidX([button frame]) + viewFrameXOffset;
    // Somewhere near the center: a match
    EXPECT_EQ(button,
              [bar_ buttonForDroppingOnAtPoint:NSMakePoint(x-1, 10)]);
    EXPECT_EQ(button,
              [bar_ buttonForDroppingOnAtPoint:NSMakePoint(x+1, 10)]);
    EXPECT_FALSE([bar_ shouldShowIndicatorShownForPoint:NSMakePoint(x, 10)]);;

    // On the very edges: NOT a match
    x = NSMinX([button frame]) + viewFrameXOffset;
    EXPECT_NE(button,
              [bar_ buttonForDroppingOnAtPoint:NSMakePoint(x, 9)]);
    x = NSMaxX([button frame]) + viewFrameXOffset;
    EXPECT_NE(button,
              [bar_ buttonForDroppingOnAtPoint:NSMakePoint(x, 11)]);
  }
}

TEST_F(BookmarkBarControllerTest, NodeDeletedWhileMenuIsOpen) {
  BookmarkModel* model = profile()->GetBookmarkModel();
  [bar_ loaded:model];

  const BookmarkNode* parent = model->bookmark_bar_node();
  const BookmarkNode* initialNode = model->AddURL(
      parent, parent->child_count(),
      ASCIIToUTF16("initial"),
      GURL("http://www.google.com"));

  NSMenuItem* item = ItemForBookmarkBarMenu(initialNode);
  EXPECT_EQ(0U, noOpenBar()->urls_.size());

  // Basic check of the menu item and an IBOutlet it can call.
  EXPECT_EQ(initialNode, [bar_ nodeFromMenuItem:item]);
  [bar_ openBookmarkInNewWindow:item];
  EXPECT_EQ(1U, noOpenBar()->urls_.size());
  [bar_ clear];

  // Now delete the node and make sure things are happy (no crash,
  // NULL node caught).
  model->Remove(parent, parent->GetIndexOf(initialNode));
  EXPECT_EQ(nil, [bar_ nodeFromMenuItem:item]);
  // Should not crash by referencing a deleted node.
  [bar_ openBookmarkInNewWindow:item];
  // Confirm the above did nothing in case it somehow didn't crash.
  EXPECT_EQ(0U, noOpenBar()->urls_.size());

  // Confirm some more non-crashes.
  [bar_ openBookmarkInNewForegroundTab:item];
  [bar_ openBookmarkInIncognitoWindow:item];
  [bar_ editBookmark:item];
  [bar_ copyBookmark:item];
  [bar_ deleteBookmark:item];
  [bar_ openAllBookmarks:item];
  [bar_ openAllBookmarksNewWindow:item];
  [bar_ openAllBookmarksIncognitoWindow:item];
}

TEST_F(BookmarkBarControllerTest, NodeDeletedWhileContextMenuIsOpen) {
  BookmarkModel* model = profile()->GetBookmarkModel();
  [bar_ loaded:model];

  const BookmarkNode* parent = model->bookmark_bar_node();
  const BookmarkNode* folder = model->AddFolder(parent,
                                                parent->child_count(),
                                                ASCIIToUTF16("folder"));
  const BookmarkNode* framma = model->AddURL(folder, folder->child_count(),
                                             ASCIIToUTF16("f1"),
                                             GURL("http://framma-lamma.com"));

  // Mock in a menu
  id origMenu = [bar_ buttonContextMenu];
  id fakeMenu = [OCMockObject partialMockForObject:origMenu];
  [[fakeMenu expect] cancelTracking];
  [bar_ setButtonContextMenu:fakeMenu];

  // Force a delete which should cancelTracking on the menu.
  model->Remove(framma->parent(), framma->parent()->GetIndexOf(framma));

  // Restore, then confirm cancelTracking was called.
  [bar_ setButtonContextMenu:origMenu];
  EXPECT_OCMOCK_VERIFY(fakeMenu);
}

TEST_F(BookmarkBarControllerTest, CloseFolderOnAnimate) {
  BookmarkModel* model = profile()->GetBookmarkModel();
  const BookmarkNode* parent = model->bookmark_bar_node();
  const BookmarkNode* folder = model->AddFolder(parent,
                                                parent->child_count(),
                                                ASCIIToUTF16("folder"));
  model->AddFolder(parent, parent->child_count(),
                  ASCIIToUTF16("sibbling folder"));
  model->AddURL(folder, folder->child_count(), ASCIIToUTF16("title a"),
                GURL("http://www.google.com/a"));
  model->AddURL(folder, folder->child_count(),
      ASCIIToUTF16("title super duper long long whoa momma title you betcha"),
      GURL("http://www.google.com/b"));
  BookmarkButton* button = [[bar_ buttons] objectAtIndex:0];
  EXPECT_FALSE([bar_ folderController]);
  [bar_ openBookmarkFolderFromButton:button];
  BookmarkBarFolderController* bbfc = [bar_ folderController];
  // The following tells us that the folder menu is showing. We want to make
  // sure the folder menu goes away if the bookmark bar is hidden.
  EXPECT_TRUE(bbfc);
  EXPECT_TRUE([bar_ isVisible]);

  // Hide the bookmark bar.
  [bar_ updateAndShowNormalBar:NO
               showDetachedBar:YES
                 withAnimation:YES];
  EXPECT_TRUE([bar_ isAnimationRunning]);

  // Now that we've closed the bookmark bar (with animation) the folder menu
  // should have been closed thus releasing the folderController.
  EXPECT_FALSE([bar_ folderController]);
}

TEST_F(BookmarkBarControllerTest, MoveRemoveAddButtons) {
  BookmarkModel& model(*profile()->GetBookmarkModel());
  const BookmarkNode* root = model.bookmark_bar_node();
  const std::string model_string("1b 2f:[ 2f1b 2f2b ] 3b ");
  model_test_utils::AddNodesFromModelString(model, root, model_string);

  // Validate initial model.
  std::string actualModelString = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ(model_string, actualModelString);

  // Remember how many buttons are showing.
  int oldDisplayedButtons = [bar_ displayedButtonCount];
  NSArray* buttons = [bar_ buttons];

  // Move a button around a bit.
  [bar_ moveButtonFromIndex:0 toIndex:2];
  EXPECT_NSEQ(@"2f", [[buttons objectAtIndex:0] title]);
  EXPECT_NSEQ(@"3b", [[buttons objectAtIndex:1] title]);
  EXPECT_NSEQ(@"1b", [[buttons objectAtIndex:2] title]);
  EXPECT_EQ(oldDisplayedButtons, [bar_ displayedButtonCount]);
  [bar_ moveButtonFromIndex:2 toIndex:0];
  EXPECT_NSEQ(@"1b", [[buttons objectAtIndex:0] title]);
  EXPECT_NSEQ(@"2f", [[buttons objectAtIndex:1] title]);
  EXPECT_NSEQ(@"3b", [[buttons objectAtIndex:2] title]);
  EXPECT_EQ(oldDisplayedButtons, [bar_ displayedButtonCount]);

  // Add a couple of buttons.
  const BookmarkNode* parent = root->GetChild(1); // Purloin an existing node.
  const BookmarkNode* node = parent->GetChild(0);
  [bar_ addButtonForNode:node atIndex:0];
  EXPECT_NSEQ(@"2f1b", [[buttons objectAtIndex:0] title]);
  EXPECT_NSEQ(@"1b", [[buttons objectAtIndex:1] title]);
  EXPECT_NSEQ(@"2f", [[buttons objectAtIndex:2] title]);
  EXPECT_NSEQ(@"3b", [[buttons objectAtIndex:3] title]);
  EXPECT_EQ(oldDisplayedButtons + 1, [bar_ displayedButtonCount]);
  node = parent->GetChild(1);
  [bar_ addButtonForNode:node atIndex:-1];
  EXPECT_NSEQ(@"2f1b", [[buttons objectAtIndex:0] title]);
  EXPECT_NSEQ(@"1b", [[buttons objectAtIndex:1] title]);
  EXPECT_NSEQ(@"2f", [[buttons objectAtIndex:2] title]);
  EXPECT_NSEQ(@"3b", [[buttons objectAtIndex:3] title]);
  EXPECT_NSEQ(@"2f2b", [[buttons objectAtIndex:4] title]);
  EXPECT_EQ(oldDisplayedButtons + 2, [bar_ displayedButtonCount]);

  // Remove a couple of buttons.
  [bar_ removeButton:4 animate:NO];
  [bar_ removeButton:1 animate:NO];
  EXPECT_NSEQ(@"2f1b", [[buttons objectAtIndex:0] title]);
  EXPECT_NSEQ(@"2f", [[buttons objectAtIndex:1] title]);
  EXPECT_NSEQ(@"3b", [[buttons objectAtIndex:2] title]);
  EXPECT_EQ(oldDisplayedButtons, [bar_ displayedButtonCount]);
}

TEST_F(BookmarkBarControllerTest, ShrinkOrHideView) {
  NSRect viewFrame = NSMakeRect(0.0, 0.0, 500.0, 50.0);
  NSView* view = [[[NSView alloc] initWithFrame:viewFrame] autorelease];
  EXPECT_FALSE([view isHidden]);
  [bar_ shrinkOrHideView:view forMaxX:500.0];
  EXPECT_EQ(500.0, NSWidth([view frame]));
  EXPECT_FALSE([view isHidden]);
  [bar_ shrinkOrHideView:view forMaxX:450.0];
  EXPECT_EQ(450.0, NSWidth([view frame]));
  EXPECT_FALSE([view isHidden]);
  [bar_ shrinkOrHideView:view forMaxX:40.0];
  EXPECT_EQ(40.0, NSWidth([view frame]));
  EXPECT_FALSE([view isHidden]);
  [bar_ shrinkOrHideView:view forMaxX:31.0];
  EXPECT_EQ(31.0, NSWidth([view frame]));
  EXPECT_FALSE([view isHidden]);
  [bar_ shrinkOrHideView:view forMaxX:29.0];
  EXPECT_TRUE([view isHidden]);
}

class BookmarkBarControllerOpenAllTest : public BookmarkBarControllerTest {
public:
  virtual void SetUp() {
    BookmarkBarControllerTest::SetUp();
    ASSERT_TRUE(profile());

    resizeDelegate_.reset([[ViewResizerPong alloc] init]);
    NSRect parent_frame = NSMakeRect(0, 0, 800, 50);
    bar_.reset(
               [[BookmarkBarControllerOpenAllPong alloc]
                initWithBrowser:browser()
                   initialWidth:NSWidth(parent_frame)
                       delegate:nil
                 resizeDelegate:resizeDelegate_.get()]);
    [bar_ view];
    // Awkwardness to look like we've been installed.
    [parent_view_ addSubview:[bar_ view]];
    NSRect frame = [[[bar_ view] superview] frame];
    frame.origin.y = 100;
    [[[bar_ view] superview] setFrame:frame];

    BookmarkModel* model = profile()->GetBookmarkModel();
    parent_ = model->bookmark_bar_node();
    // { one, { two-one, two-two }, three }
    model->AddURL(parent_, parent_->child_count(), ASCIIToUTF16("title"),
                  GURL("http://one.com"));
    folder_ = model->AddFolder(parent_, parent_->child_count(),
                               ASCIIToUTF16("folder"));
    model->AddURL(folder_, folder_->child_count(),
                  ASCIIToUTF16("title"), GURL("http://two-one.com"));
    model->AddURL(folder_, folder_->child_count(),
                  ASCIIToUTF16("title"), GURL("http://two-two.com"));
    model->AddURL(parent_, parent_->child_count(),
                  ASCIIToUTF16("title"), GURL("https://three.com"));
  }
  const BookmarkNode* parent_;  // Weak
  const BookmarkNode* folder_;  // Weak
};

TEST_F(BookmarkBarControllerOpenAllTest, OpenAllBookmarks) {
  // Our first OpenAll... is from the bar itself.
  [bar_ openAllBookmarks:ItemForBookmarkBarMenu(parent_)];
  BookmarkBarControllerOpenAllPong* specialBar =
      (BookmarkBarControllerOpenAllPong*)bar_.get();
  EXPECT_EQ([specialBar dispositionDetected], NEW_FOREGROUND_TAB);

  // Now try an OpenAll... from a folder node.
  [specialBar setDispositionDetected:IGNORE_ACTION]; // Reset
  [bar_ openAllBookmarks:ItemForBookmarkBarMenu(folder_)];
  EXPECT_EQ([specialBar dispositionDetected], NEW_FOREGROUND_TAB);
}

TEST_F(BookmarkBarControllerOpenAllTest, OpenAllNewWindow) {
  // Our first OpenAll... is from the bar itself.
  [bar_ openAllBookmarksNewWindow:ItemForBookmarkBarMenu(parent_)];
  BookmarkBarControllerOpenAllPong* specialBar =
      (BookmarkBarControllerOpenAllPong*)bar_.get();
  EXPECT_EQ([specialBar dispositionDetected], NEW_WINDOW);

  // Now try an OpenAll... from a folder node.
  [specialBar setDispositionDetected:IGNORE_ACTION]; // Reset
  [bar_ openAllBookmarksNewWindow:ItemForBookmarkBarMenu(folder_)];
  EXPECT_EQ([specialBar dispositionDetected], NEW_WINDOW);
}

TEST_F(BookmarkBarControllerOpenAllTest, OpenAllIncognito) {
  // Our first OpenAll... is from the bar itself.
  [bar_ openAllBookmarksIncognitoWindow:ItemForBookmarkBarMenu(parent_)];
  BookmarkBarControllerOpenAllPong* specialBar =
  (BookmarkBarControllerOpenAllPong*)bar_.get();
  EXPECT_EQ([specialBar dispositionDetected], OFF_THE_RECORD);

  // Now try an OpenAll... from a folder node.
  [specialBar setDispositionDetected:IGNORE_ACTION]; // Reset
  [bar_ openAllBookmarksIncognitoWindow:ItemForBookmarkBarMenu(folder_)];
  EXPECT_EQ([specialBar dispositionDetected], OFF_THE_RECORD);
}

// Command-click on a folder should open all the bookmarks in it.
TEST_F(BookmarkBarControllerOpenAllTest, CommandClickOnFolder) {
  NSButton* first = [[bar_ buttons] objectAtIndex:0];
  EXPECT_TRUE(first);

  // Create the right kind of event; mock NSApp so [NSApp
  // currentEvent] finds it.
  NSEvent* commandClick =
      cocoa_test_event_utils::MouseEventAtPoint(NSZeroPoint,
                                                NSLeftMouseDown,
                                                NSCommandKeyMask);
  id fakeApp = [OCMockObject partialMockForObject:NSApp];
  [[[fakeApp stub] andReturn:commandClick] currentEvent];
  id oldApp = NSApp;
  NSApp = fakeApp;
  size_t originalDispositionCount = noOpenBar()->dispositions_.size();

  // Click!
  [first performClick:first];

  size_t dispositionCount = noOpenBar()->dispositions_.size();
  EXPECT_EQ(originalDispositionCount+1, dispositionCount);
  EXPECT_EQ(noOpenBar()->dispositions_[dispositionCount-1], NEW_BACKGROUND_TAB);

  // Replace NSApp
  NSApp = oldApp;
}

class BookmarkBarControllerNotificationTest : public CocoaProfileTest {
 public:
  virtual void SetUp() {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    resizeDelegate_.reset([[ViewResizerPong alloc] init]);
    NSRect parent_frame = NSMakeRect(0, 0, 800, 50);
    parent_view_.reset([[NSView alloc] initWithFrame:parent_frame]);
    [parent_view_ setHidden:YES];
    bar_.reset(
      [[BookmarkBarControllerNotificationPong alloc]
          initWithBrowser:browser()
             initialWidth:NSWidth(parent_frame)
                 delegate:nil
           resizeDelegate:resizeDelegate_.get()]);

    // Force loading of the nib.
    [bar_ view];
    // Awkwardness to look like we've been installed.
    [parent_view_ addSubview:[bar_ view]];
    NSRect frame = [[[bar_ view] superview] frame];
    frame.origin.y = 100;
    [[[bar_ view] superview] setFrame:frame];

    // Do not add the bar to a window, yet.
  }

  scoped_nsobject<NSView> parent_view_;
  scoped_nsobject<ViewResizerPong> resizeDelegate_;
  scoped_nsobject<BookmarkBarControllerNotificationPong> bar_;
};

TEST_F(BookmarkBarControllerNotificationTest, DeregistersForNotifications) {
  NSWindow* window = [[CocoaTestHelperWindow alloc] init];
  [window setReleasedWhenClosed:YES];

  // First add the bookmark bar to the temp window, then to another window.
  [[window contentView] addSubview:parent_view_];
  [[test_window() contentView] addSubview:parent_view_];

  // Post a fake windowDidResignKey notification for the temp window and make
  // sure the bookmark bar controller wasn't listening.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidResignKeyNotification
                    object:window];
  EXPECT_FALSE([bar_ windowDidResignKeyReceived]);

  // Close the temp window and make sure no notification was received.
  [window close];
  EXPECT_FALSE([bar_ windowWillCloseReceived]);
}


// TODO(jrg): draggingEntered: and draggingExited: trigger timers so
// they are hard to test.  Factor out "fire timers" into routines
// which can be overridden to fire immediately to make behavior
// confirmable.

// TODO(jrg): add unit test to make sure "Other Bookmarks" responds
// properly to a hover open.

// TODO(viettrungluu): figure out how to test animations.

class BookmarkBarControllerDragDropTest : public BookmarkBarControllerTestBase {
 public:
  scoped_nsobject<BookmarkBarControllerDragData> bar_;

  virtual void SetUp() {
    BookmarkBarControllerTestBase::SetUp();
    ASSERT_TRUE(browser());

    bar_.reset(
               [[BookmarkBarControllerDragData alloc]
                initWithBrowser:browser()
                   initialWidth:NSWidth([parent_view_ frame])
                       delegate:nil
                 resizeDelegate:resizeDelegate_.get()]);
    InstallAndToggleBar(bar_.get());
  }
};

TEST_F(BookmarkBarControllerDragDropTest, DragMoveBarBookmarkToOffTheSide) {
  BookmarkModel& model(*profile()->GetBookmarkModel());
  const BookmarkNode* root = model.bookmark_bar_node();
  const std::string model_string("1bWithLongName 2fWithLongName:[ "
      "2f1bWithLongName 2f2fWithLongName:[ 2f2f1bWithLongName "
      "2f2f2bWithLongName 2f2f3bWithLongName 2f4b ] 2f3bWithLongName ] "
      "3bWithLongName 4bWithLongName 5bWithLongName 6bWithLongName "
      "7bWithLongName 8bWithLongName 9bWithLongName 10bWithLongName "
      "11bWithLongName 12bWithLongName 13b ");
  model_test_utils::AddNodesFromModelString(model, root, model_string);

  // Validate initial model.
  std::string actualModelString = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ(model_string, actualModelString);

  // Insure that the off-the-side is not showing.
  ASSERT_FALSE([bar_ offTheSideButtonIsHidden]);

  // Remember how many buttons are showing and are available.
  int oldDisplayedButtons = [bar_ displayedButtonCount];
  int oldChildCount = root->child_count();

  // Pop up the off-the-side menu.
  BookmarkButton* otsButton = (BookmarkButton*)[bar_ offTheSideButton];
  ASSERT_TRUE(otsButton);
  [[otsButton target] performSelector:@selector(openOffTheSideFolderFromButton:)
                           withObject:otsButton];
  BookmarkBarFolderController* otsController = [bar_ folderController];
  EXPECT_TRUE(otsController);
  NSWindow* toWindow = [otsController window];
  EXPECT_TRUE(toWindow);
  BookmarkButton* draggedButton =
      [bar_ buttonWithTitleEqualTo:@"3bWithLongName"];
  ASSERT_TRUE(draggedButton);
  int oldOTSCount = (int)[[otsController buttons] count];
  EXPECT_EQ(oldOTSCount, oldChildCount - oldDisplayedButtons);
  BookmarkButton* targetButton = [[otsController buttons] objectAtIndex:0];
  ASSERT_TRUE(targetButton);
  [otsController dragButton:draggedButton
                         to:[targetButton center]
                       copy:YES];
  // There should still be the same number of buttons in the bar
  // and off-the-side should have one more.
  int newDisplayedButtons = [bar_ displayedButtonCount];
  int newChildCount = root->child_count();
  int newOTSCount = (int)[[otsController buttons] count];
  EXPECT_EQ(oldDisplayedButtons, newDisplayedButtons);
  EXPECT_EQ(oldChildCount + 1, newChildCount);
  EXPECT_EQ(oldOTSCount + 1, newOTSCount);
  EXPECT_EQ(newOTSCount, newChildCount - newDisplayedButtons);
}

TEST_F(BookmarkBarControllerDragDropTest, DragOffTheSideToOther) {
  BookmarkModel& model(*profile()->GetBookmarkModel());
  const BookmarkNode* root = model.bookmark_bar_node();
  const std::string model_string("1bWithLongName 2bWithLongName "
      "3bWithLongName 4bWithLongName 5bWithLongName 6bWithLongName "
      "7bWithLongName 8bWithLongName 9bWithLongName 10bWithLongName "
      "11bWithLongName 12bWithLongName 13bWithLongName 14bWithLongName "
      "15bWithLongName 16bWithLongName 17bWithLongName 18bWithLongName "
      "19bWithLongName 20bWithLongName ");
  model_test_utils::AddNodesFromModelString(model, root, model_string);

  const BookmarkNode* other = model.other_node();
  const std::string other_string("1other 2other 3other ");
  model_test_utils::AddNodesFromModelString(model, other, other_string);

  // Validate initial model.
  std::string actualModelString = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ(model_string, actualModelString);
  std::string actualOtherString = model_test_utils::ModelStringFromNode(other);
  EXPECT_EQ(other_string, actualOtherString);

  // Insure that the off-the-side is showing.
  ASSERT_FALSE([bar_ offTheSideButtonIsHidden]);

  // Remember how many buttons are showing and are available.
  int oldDisplayedButtons = [bar_ displayedButtonCount];
  int oldRootCount = root->child_count();
  int oldOtherCount = other->child_count();

  // Pop up the off-the-side menu.
  BookmarkButton* otsButton = (BookmarkButton*)[bar_ offTheSideButton];
  ASSERT_TRUE(otsButton);
  [[otsButton target] performSelector:@selector(openOffTheSideFolderFromButton:)
                           withObject:otsButton];
  BookmarkBarFolderController* otsController = [bar_ folderController];
  EXPECT_TRUE(otsController);
  int oldOTSCount = (int)[[otsController buttons] count];
  EXPECT_EQ(oldOTSCount, oldRootCount - oldDisplayedButtons);

  // Pick an off-the-side button and drag it to the other bookmarks.
  BookmarkButton* draggedButton =
      [otsController buttonWithTitleEqualTo:@"20bWithLongName"];
  ASSERT_TRUE(draggedButton);
  BookmarkButton* targetButton = [bar_ otherBookmarksButton];
  ASSERT_TRUE(targetButton);
  [bar_ dragButton:draggedButton to:[targetButton center] copy:NO];

  // There should one less button in the bar, one less in off-the-side,
  // and one more in other bookmarks.
  int newRootCount = root->child_count();
  int newOTSCount = (int)[[otsController buttons] count];
  int newOtherCount = other->child_count();
  EXPECT_EQ(oldRootCount - 1, newRootCount);
  EXPECT_EQ(oldOTSCount - 1, newOTSCount);
  EXPECT_EQ(oldOtherCount + 1, newOtherCount);
}

TEST_F(BookmarkBarControllerDragDropTest, DragBookmarkData) {
  BookmarkModel& model(*profile()->GetBookmarkModel());
  const BookmarkNode* root = model.bookmark_bar_node();
  const std::string model_string("1b 2f:[ 2f1b 2f2f:[ 2f2f1b 2f2f2b 2f2f3b ] "
                                  "2f3b ] 3b 4b ");
  model_test_utils::AddNodesFromModelString(model, root, model_string);
  const BookmarkNode* other = model.other_node();
  const std::string other_string("O1b O2b O3f:[ O3f1b O3f2f ] "
                                 "O4f:[ O4f1b O4f2f ] 05b ");
  model_test_utils::AddNodesFromModelString(model, other, other_string);

  // Validate initial model.
  std::string actual = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ(model_string, actual);
  actual = model_test_utils::ModelStringFromNode(other);
  EXPECT_EQ(other_string, actual);

  // Remember the little ones.
  int oldChildCount = root->child_count();

  BookmarkButton* targetButton = [bar_ buttonWithTitleEqualTo:@"3b"];
  ASSERT_TRUE(targetButton);

  // Gen up some dragging data.
  const BookmarkNode* newNode = other->GetChild(2);
  [bar_ setDragDataNode:newNode];
  scoped_nsobject<FakeDragInfo> dragInfo([[FakeDragInfo alloc] init]);
  [dragInfo setDropLocation:[targetButton center]];
  [bar_ dragBookmarkData:(id<NSDraggingInfo>)dragInfo.get()];

  // There should one more button in the bar.
  int newChildCount = root->child_count();
  EXPECT_EQ(oldChildCount + 1, newChildCount);
  // Verify the model.
  const std::string expected("1b 2f:[ 2f1b 2f2f:[ 2f2f1b 2f2f2b 2f2f3b ] "
                             "2f3b ] O3f:[ O3f1b O3f2f ] 3b 4b ");
  actual = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ(expected, actual);
  oldChildCount = newChildCount;

  // Now do it over a folder button.
  targetButton = [bar_ buttonWithTitleEqualTo:@"2f"];
  ASSERT_TRUE(targetButton);
  NSPoint targetPoint = [targetButton center];
  newNode = other->GetChild(2);  // Should be O4f.
  EXPECT_EQ(newNode->GetTitle(), ASCIIToUTF16("O4f"));
  [bar_ setDragDataNode:newNode];
  [dragInfo setDropLocation:targetPoint];
  [bar_ dragBookmarkData:(id<NSDraggingInfo>)dragInfo.get()];

  newChildCount = root->child_count();
  EXPECT_EQ(oldChildCount, newChildCount);
  // Verify the model.
  const std::string expected1("1b 2f:[ 2f1b 2f2f:[ 2f2f1b 2f2f2b 2f2f3b ] "
                              "2f3b O4f:[ O4f1b O4f2f ] ] O3f:[ O3f1b O3f2f ] "
                              "3b 4b ");
  actual = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ(expected1, actual);
}

TEST_F(BookmarkBarControllerDragDropTest, AddURLs) {
  BookmarkModel& model(*profile()->GetBookmarkModel());
  const BookmarkNode* root = model.bookmark_bar_node();
  const std::string model_string("1b 2f:[ 2f1b 2f2f:[ 2f2f1b 2f2f2b 2f2f3b ] "
                                 "2f3b ] 3b 4b ");
  model_test_utils::AddNodesFromModelString(model, root, model_string);

  // Validate initial model.
  std::string actual = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ(model_string, actual);

  // Remember the children.
  int oldChildCount = root->child_count();

  BookmarkButton* targetButton = [bar_ buttonWithTitleEqualTo:@"3b"];
  ASSERT_TRUE(targetButton);

  NSArray* urls = [NSArray arrayWithObjects: @"http://www.a.com/",
                   @"http://www.b.com/", nil];
  NSArray* titles = [NSArray arrayWithObjects: @"SiteA", @"SiteB", nil];
  [bar_ addURLs:urls withTitles:titles at:[targetButton center]];

  // There should two more nodes in the bar.
  int newChildCount = root->child_count();
  EXPECT_EQ(oldChildCount + 2, newChildCount);
  // Verify the model.
  const std::string expected("1b 2f:[ 2f1b 2f2f:[ 2f2f1b 2f2f2b 2f2f3b ] "
                             "2f3b ] SiteA SiteB 3b 4b ");
  actual = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ(expected, actual);
}

TEST_F(BookmarkBarControllerDragDropTest, ControllerForNode) {
  BookmarkModel& model(*profile()->GetBookmarkModel());
  const BookmarkNode* root = model.bookmark_bar_node();
  const std::string model_string("1b 2f:[ 2f1b 2f2b ] 3b ");
  model_test_utils::AddNodesFromModelString(model, root, model_string);

  // Validate initial model.
  std::string actualModelString = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ(model_string, actualModelString);

  // Find the main bar controller.
  const void* expectedController = bar_;
  const void* actualController = [bar_ controllerForNode:root];
  EXPECT_EQ(expectedController, actualController);
}

TEST_F(BookmarkBarControllerDragDropTest, DropPositionIndicator) {
  BookmarkModel& model(*profile()->GetBookmarkModel());
  const BookmarkNode* root = model.bookmark_bar_node();
  const std::string model_string("1b 2f:[ 2f1b 2f2b 2f3b ] 3b 4b ");
  model_test_utils::AddNodesFromModelString(model, root, model_string);

  // Validate initial model.
  std::string actualModel = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ(model_string, actualModel);

  // Test a series of points starting at the right edge of the bar.
  BookmarkButton* targetButton = [bar_ buttonWithTitleEqualTo:@"1b"];
  ASSERT_TRUE(targetButton);
  NSPoint targetPoint = [targetButton left];
  const CGFloat xDelta = 0.5 * bookmarks::kBookmarkHorizontalPadding;
  const CGFloat baseOffset = targetPoint.x;
  CGFloat expected = xDelta;
  CGFloat actual = [bar_ indicatorPosForDragToPoint:targetPoint];
  EXPECT_CGFLOAT_EQ(expected, actual);
  targetButton = [bar_ buttonWithTitleEqualTo:@"2f"];
  actual = [bar_ indicatorPosForDragToPoint:[targetButton right]];
  targetButton = [bar_ buttonWithTitleEqualTo:@"3b"];
  expected = [targetButton left].x - baseOffset + xDelta;
  EXPECT_CGFLOAT_EQ(expected, actual);
  targetButton = [bar_ buttonWithTitleEqualTo:@"4b"];
  targetPoint = [targetButton right];
  targetPoint.x += 100;  // Somewhere off to the right.
  expected = NSMaxX([targetButton frame]) + xDelta;
  actual = [bar_ indicatorPosForDragToPoint:targetPoint];
  EXPECT_CGFLOAT_EQ(expected, actual);
}

TEST_F(BookmarkBarControllerDragDropTest, PulseButton) {
  BookmarkModel* model = profile()->GetBookmarkModel();
  const BookmarkNode* root = model->bookmark_bar_node();
  GURL gurl("http://www.google.com");
  const BookmarkNode* node = model->AddURL(root, root->child_count(),
                                           ASCIIToUTF16("title"), gurl);

  BookmarkButton* button = [[bar_ buttons] objectAtIndex:0];
  EXPECT_FALSE([button isContinuousPulsing]);

  NSValue *value = [NSValue valueWithPointer:node];
  NSDictionary *dict = [NSDictionary
                         dictionaryWithObjectsAndKeys:value,
                         bookmark_button::kBookmarkKey,
                         [NSNumber numberWithBool:YES],
                         bookmark_button::kBookmarkPulseFlagKey,
                         nil];
  [[NSNotificationCenter defaultCenter]
        postNotificationName:bookmark_button::kPulseBookmarkButtonNotification
                      object:nil
                    userInfo:dict];
  EXPECT_TRUE([button isContinuousPulsing]);

  dict = [NSDictionary dictionaryWithObjectsAndKeys:value,
                       bookmark_button::kBookmarkKey,
                       [NSNumber numberWithBool:NO],
                       bookmark_button::kBookmarkPulseFlagKey,
                       nil];
  [[NSNotificationCenter defaultCenter]
        postNotificationName:bookmark_button::kPulseBookmarkButtonNotification
                      object:nil
                    userInfo:dict];
  EXPECT_FALSE([button isContinuousPulsing]);
}

TEST_F(BookmarkBarControllerDragDropTest, DragBookmarkDataToTrash) {
  BookmarkModel& model(*profile()->GetBookmarkModel());
  const BookmarkNode* root = model.bookmark_bar_node();
  const std::string model_string("1b 2f:[ 2f1b 2f2f:[ 2f2f1b 2f2f2b 2f2f3b ] "
                                  "2f3b ] 3b 4b ");
  model_test_utils::AddNodesFromModelString(model, root, model_string);

  // Validate initial model.
  std::string actual = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ(model_string, actual);

  int oldChildCount = root->child_count();

  // Drag a button to the trash.
  BookmarkButton* buttonToDelete = [bar_ buttonWithTitleEqualTo:@"3b"];
  ASSERT_TRUE(buttonToDelete);
  EXPECT_TRUE([bar_ canDragBookmarkButtonToTrash:buttonToDelete]);
  [bar_ didDragBookmarkToTrash:buttonToDelete];

  // There should be one less button in the bar.
  int newChildCount = root->child_count();
  EXPECT_EQ(oldChildCount - 1, newChildCount);
  // Verify the model.
  const std::string expected("1b 2f:[ 2f1b 2f2f:[ 2f2f1b 2f2f2b 2f2f3b ] "
                             "2f3b ] 4b ");
  actual = model_test_utils::ModelStringFromNode(root);
  EXPECT_EQ(expected, actual);

  // Verify that the other bookmark folder can't be deleted.
  BookmarkButton *otherButton = [bar_ otherBookmarksButton];
  EXPECT_FALSE([bar_ canDragBookmarkButtonToTrash:otherButton]);
}

}  // namespace
