// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button.h"

#include <cmath>

#include "base/logging.h"
#import "base/memory/scoped_nsobject.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button_cell.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_folder_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "content/browser/user_metrics.h"

// The opacity of the bookmark button drag image.
static const CGFloat kDragImageOpacity = 0.7;


namespace bookmark_button {

NSString* const kPulseBookmarkButtonNotification =
    @"PulseBookmarkButtonNotification";
NSString* const kBookmarkKey = @"BookmarkKey";
NSString* const kBookmarkPulseFlagKey = @"BookmarkPulseFlagKey";

};

namespace {
// We need a class variable to track the current dragged button to enable
// proper live animated dragging behavior, and can't do it in the
// delegate/controller since you can drag a button from one domain to the
// other (from a "folder" menu, to the main bar, or vice versa).
BookmarkButton* gDraggedButton = nil; // Weak
};

@interface BookmarkButton(Private)

// Make a drag image for the button.
- (NSImage*)dragImage;

- (void)installCustomTrackingArea;

@end  // @interface BookmarkButton(Private)


@implementation BookmarkButton

@synthesize delegate = delegate_;
@synthesize acceptsTrackIn = acceptsTrackIn_;

- (id)initWithFrame:(NSRect)frameRect {
  // BookmarkButton's ViewID may be changed to VIEW_ID_OTHER_BOOKMARKS in
  // BookmarkBarController, so we can't just override -viewID method to return
  // it.
  if ((self = [super initWithFrame:frameRect])) {
    view_id_util::SetID(self, VIEW_ID_BOOKMARK_BAR_ELEMENT);
    [self installCustomTrackingArea];
  }
  return self;
}

- (void)dealloc {
  if ([[self cell] respondsToSelector:@selector(safelyStopPulsing)])
    [[self cell] safelyStopPulsing];
  view_id_util::UnsetID(self);

  if (area_) {
    [self removeTrackingArea:area_];
    [area_ release];
  }

  [super dealloc];
}

- (const BookmarkNode*)bookmarkNode {
  return [[self cell] bookmarkNode];
}

- (BOOL)isFolder {
  const BookmarkNode* node = [self bookmarkNode];
  return (node && node->is_folder());
}

- (BOOL)isEmpty {
  return [self bookmarkNode] ? NO : YES;
}

- (void)setIsContinuousPulsing:(BOOL)flag {
  [[self cell] setIsContinuousPulsing:flag];
}

- (BOOL)isContinuousPulsing {
  return [[self cell] isContinuousPulsing];
}

- (NSPoint)screenLocationForRemoveAnimation {
  NSPoint point;

  if (dragPending_) {
    // Use the position of the mouse in the drag image as the location.
    point = dragEndScreenLocation_;
    point.x += dragMouseOffset_.x;
    if ([self isFlipped]) {
      point.y += [self bounds].size.height - dragMouseOffset_.y;
    } else {
      point.y += dragMouseOffset_.y;
    }
  } else {
    // Use the middle of this button as the location.
    NSRect bounds = [self bounds];
    point = NSMakePoint(NSMidX(bounds), NSMidY(bounds));
    point = [self convertPoint:point toView:nil];
    point = [[self window] convertBaseToScreen:point];
  }

  return point;
}


- (void)updateTrackingAreas {
  [self installCustomTrackingArea];
  [super updateTrackingAreas];
}

- (DraggableButtonResult)deltaIndicatesDragStartWithXDelta:(float)xDelta
                                                    yDelta:(float)yDelta
                                               xHysteresis:(float)xHysteresis
                                               yHysteresis:(float)yHysteresis
                                                 indicates:(BOOL*)result {
  const float kDownProportion = 1.4142135f; // Square root of 2.

  // We want to show a folder menu when you drag down on folder buttons,
  // so don't classify this as a drag for that case.
  if ([self isFolder] &&
      (yDelta <= -yHysteresis) &&  // Bottom of hysteresis box was hit.
      (std::abs(yDelta) / std::abs(xDelta)) >= kDownProportion) {
    *result = NO;
    return kDraggableButtonMixinDidWork;
  }

  return kDraggableButtonImplUseBase;
}


// By default, NSButton ignores middle-clicks.
// But we want them.
- (void)otherMouseUp:(NSEvent*)event {
  [self performClick:self];
}

- (BOOL)acceptsTrackInFrom:(id)sender {
  return  [self isFolder] || [self acceptsTrackIn];
}


// Overridden from DraggableButton.
- (void)beginDrag:(NSEvent*)event {
  // Don't allow a drag of the empty node.
  // The empty node is a placeholder for "(empty)", to be revisited.
  if ([self isEmpty])
    return;

  if (![self delegate]) {
    NOTREACHED();
    return;
  }

  if ([self isFolder]) {
    // Close the folder's drop-down menu if it's visible.
    [[self target] closeBookmarkFolder:self];
  }

  // At the moment, moving bookmarks causes their buttons (like me!)
  // to be destroyed and rebuilt.  Make sure we don't go away while on
  // the stack.
  [self retain];

  // Ask our delegate to fill the pasteboard for us.
  NSPasteboard* pboard = [NSPasteboard pasteboardWithName:NSDragPboard];
  [[self delegate] fillPasteboard:pboard forDragOfButton:self];

  // Lock bar visibility, forcing the overlay to stay visible if we are in
  // fullscreen mode.
  if ([[self delegate] dragShouldLockBarVisibility]) {
    DCHECK(!visibilityDelegate_);
    NSWindow* window = [[self delegate] browserWindow];
    visibilityDelegate_ =
        [BrowserWindowController browserWindowControllerForWindow:window];
    [visibilityDelegate_ lockBarVisibilityForOwner:self
                                     withAnimation:NO
                                             delay:NO];
  }
  const BookmarkNode* node = [self bookmarkNode];
  const BookmarkNode* parent = node ? node->parent() : NULL;
  if (parent && parent->type() == BookmarkNode::FOLDER) {
    UserMetrics::RecordAction(UserMetricsAction("BookmarkBarFolder_DragStart"));
  } else {
    UserMetrics::RecordAction(UserMetricsAction("BookmarkBar_DragStart"));
  }

  dragMouseOffset_ = [self convertPointFromBase:[event locationInWindow]];
  dragPending_ = YES;
  gDraggedButton = self;

  CGFloat yAt = [self bounds].size.height;
  NSSize dragOffset = NSMakeSize(0.0, 0.0);
  NSImage* image = [self dragImage];
  [self setHidden:YES];
  [self dragImage:image at:NSMakePoint(0, yAt) offset:dragOffset
            event:event pasteboard:pboard source:self slideBack:YES];
  [self setHidden:NO];

  // And we're done.
  dragPending_ = NO;
  gDraggedButton = nil;

  [self autorelease];
}

// Overridden to release bar visibility.
- (DraggableButtonResult)endDrag {
  gDraggedButton = nil;

  // visibilityDelegate_ can be nil if we're detached, and that's fine.
  [visibilityDelegate_ releaseBarVisibilityForOwner:self
                                      withAnimation:YES
                                              delay:YES];
  visibilityDelegate_ = nil;

  return kDraggableButtonImplUseBase;
}

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal {
  NSDragOperation operation = NSDragOperationCopy;
  if (isLocal) {
    operation |= NSDragOperationMove;
  }
  if ([delegate_ canDragBookmarkButtonToTrash:self]) {
    operation |= NSDragOperationDelete;
  }
  return operation;
}

- (void)draggedImage:(NSImage *)anImage
             endedAt:(NSPoint)aPoint
           operation:(NSDragOperation)operation {
  gDraggedButton = nil;
  // Inform delegate of drag source that we're finished dragging,
  // so it can close auto-opened bookmark folders etc.
  [delegate_ bookmarkDragDidEnd:self
                      operation:operation];
  // Tell delegate if it should delete us.
  if (operation & NSDragOperationDelete) {
    dragEndScreenLocation_ = aPoint;
    [delegate_ didDragBookmarkToTrash:self];
  }
}

- (DraggableButtonResult)performMouseDownAction:(NSEvent*)theEvent {
  int eventMask = NSLeftMouseUpMask | NSMouseEnteredMask | NSMouseExitedMask |
      NSLeftMouseDraggedMask;

  BOOL keepGoing = YES;
  [[self target] performSelector:[self action] withObject:self];
  self.draggableButton.actionHasFired = YES;

  DraggableButton* insideBtn = nil;

  while (keepGoing) {
    theEvent = [[self window] nextEventMatchingMask:eventMask];
    if (!theEvent)
      continue;

    NSPoint mouseLoc = [self convertPoint:[theEvent locationInWindow]
                                 fromView:nil];
    BOOL isInside = [self mouse:mouseLoc inRect:[self bounds]];

    switch ([theEvent type]) {
      case NSMouseEntered:
      case NSMouseExited: {
        NSView* trackedView = (NSView*)[[theEvent trackingArea] owner];
        if (trackedView && [trackedView isKindOfClass:[self class]]) {
          BookmarkButton* btn = static_cast<BookmarkButton*>(trackedView);
          if (![btn acceptsTrackInFrom:self])
            break;
          if ([theEvent type] == NSMouseEntered) {
            [[NSCursor arrowCursor] set];
            [[btn cell] mouseEntered:theEvent];
            insideBtn = btn;
          } else {
            [[btn cell] mouseExited:theEvent];
            if (insideBtn == btn)
              insideBtn = nil;
          }
        }
        break;
      }
      case NSLeftMouseDragged: {
        if (insideBtn)
          [insideBtn mouseDragged:theEvent];
        break;
      }
      case NSLeftMouseUp: {
        self.draggableButton.durationMouseWasDown =
            [theEvent timestamp] - self.draggableButton.whenMouseDown;
        if (!isInside && insideBtn && insideBtn != self) {
          // Has tracked onto another BookmarkButton menu item, and released,
          // so fire its action.
          [[insideBtn target] performSelector:[insideBtn action]
                                   withObject:insideBtn];

        } else {
          [self secondaryMouseUpAction:isInside];
          [[self cell] mouseExited:theEvent];
          [[insideBtn cell] mouseExited:theEvent];
        }
        keepGoing = NO;
        break;
      }
      default:
        /* Ignore any other kind of event. */
        break;
    }
  }
  return kDraggableButtonMixinDidWork;
}



// mouseEntered: and mouseExited: are called from our
// BookmarkButtonCell.  We redirect this information to our delegate.
// The controller can then perform menu-like actions (e.g. "hover over
// to open menu").
- (void)mouseEntered:(NSEvent*)event {
  [delegate_ mouseEnteredButton:self event:event];
}

// See comments above mouseEntered:.
- (void)mouseExited:(NSEvent*)event {
  [delegate_ mouseExitedButton:self event:event];
}

- (void)mouseMoved:(NSEvent*)theEvent {
  if ([delegate_ respondsToSelector:@selector(mouseMoved:)])
    [id(delegate_) mouseMoved:theEvent];
}

- (void)mouseDragged:(NSEvent*)theEvent {
  if ([delegate_ respondsToSelector:@selector(mouseDragged:)])
    [id(delegate_) mouseDragged:theEvent];
}

+ (BookmarkButton*)draggedButton {
  return gDraggedButton;
}

- (BOOL)canBecomeKeyView {
  // If button is an item in a folder menu, don't become key.
  return ![[self cell] isFolderButtonCell];
}

// This only gets called after a click that wasn't a drag, and only on folders.
- (DraggableButtonResult)secondaryMouseUpAction:(BOOL)wasInside {
  const NSTimeInterval kShortClickLength = 0.5;
  // Long clicks that end over the folder button result in the menu hiding.
  if (wasInside &&
      self.draggableButton.durationMouseWasDown > kShortClickLength) {
    [[self target] performSelector:[self action] withObject:self];
  } else {
    // Mouse tracked out of button during menu track. Hide menus.
    if (!wasInside)
      [delegate_ bookmarkDragDidEnd:self
                          operation:NSDragOperationNone];
  }
  return kDraggableButtonMixinDidWork;
}

@end

@implementation BookmarkButton(Private)


- (void)installCustomTrackingArea {
  const NSTrackingAreaOptions options =
      NSTrackingActiveAlways |
      NSTrackingMouseEnteredAndExited |
      NSTrackingEnabledDuringMouseDrag;

  if (area_) {
    [self removeTrackingArea:area_];
    [area_ release];
  }

  area_ = [[NSTrackingArea alloc] initWithRect:[self bounds]
                                       options:options
                                         owner:self
                                      userInfo:nil];
  [self addTrackingArea:area_];
}


- (NSImage*)dragImage {
  NSRect bounds = [self bounds];

  // Grab the image from the screen and put it in an |NSImage|. We can't use
  // this directly since we need to clip it and set its opacity. This won't work
  // if the source view is clipped. Fortunately, we don't display clipped
  // bookmark buttons.
  [self lockFocus];
  scoped_nsobject<NSBitmapImageRep>
      bitmap([[NSBitmapImageRep alloc] initWithFocusedViewRect:bounds]);
  [self unlockFocus];
  scoped_nsobject<NSImage> image([[NSImage alloc] initWithSize:[bitmap size]]);
  [image addRepresentation:bitmap];

  // Make an autoreleased |NSImage|, which will be returned, and draw into it.
  // By default, the |NSImage| will be completely transparent.
  NSImage* dragImage =
      [[[NSImage alloc] initWithSize:[bitmap size]] autorelease];
  [dragImage lockFocus];

  // Draw the image with the appropriate opacity, clipping it tightly.
  GradientButtonCell* cell = static_cast<GradientButtonCell*>([self cell]);
  DCHECK([cell isKindOfClass:[GradientButtonCell class]]);
  [[cell clipPathForFrame:bounds inView:self] setClip];
  [image drawAtPoint:NSMakePoint(0, 0)
            fromRect:NSMakeRect(0, 0, NSWidth(bounds), NSHeight(bounds))
           operation:NSCompositeSourceOver
            fraction:kDragImageOpacity];

  [dragImage unlockFocus];
  return dragImage;
}

@end  // @implementation BookmarkButton(Private)
