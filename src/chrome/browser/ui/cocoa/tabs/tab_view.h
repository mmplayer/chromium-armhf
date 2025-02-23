// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TABS_TAB_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_TABS_TAB_VIEW_H_
#pragma once

#import <Cocoa/Cocoa.h>
#include <ApplicationServices/ApplicationServices.h>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/background_gradient_view.h"
#import "chrome/browser/ui/cocoa/hover_close_button.h"

namespace tabs {

// Nomenclature:
// Tabs _glow_ under two different circumstances, when they are _hovered_ (by
// the mouse) and when they are _alerted_ (to show that the tab's title has
// changed).

// The state of alerting (to show a title change on an unselected, pinned tab).
// This is more complicated than a simple on/off since we want to allow the
// alert glow to go through a full rise-hold-fall cycle to avoid flickering (or
// always holding).
enum AlertState {
  kAlertNone = 0,  // Obj-C initializes to this.
  kAlertRising,
  kAlertHolding,
  kAlertFalling
};

}  // namespace tabs

@class TabController, TabWindowController;

// A view that handles the event tracking (clicking and dragging) for a tab
// on the tab strip. Relies on an associated TabController to provide a
// target/action for selecting the tab.

@interface TabView : BackgroundGradientView {
 @private
  IBOutlet TabController* controller_;
  // TODO(rohitrao): Add this button to a CoreAnimation layer so we can fade it
  // in and out on mouseovers.
  IBOutlet HoverCloseButton* closeButton_;

  BOOL closing_;

  BOOL isMouseInside_;  // Is the mouse hovering over?
  tabs::AlertState alertState_;

  CGFloat hoverAlpha_;  // How strong the hover glow is.
  NSTimeInterval hoverHoldEndTime_;  // When the hover glow will begin dimming.

  CGFloat alertAlpha_;  // How strong the alert glow is.
  NSTimeInterval alertHoldEndTime_;  // When the hover glow will begin dimming.

  NSTimeInterval lastGlowUpdate_;  // Time either glow was last updated.

  NSPoint hoverPoint_;  // Current location of hover in view coords.

  // The location of the current mouseDown event in window coordinates.
  NSPoint mouseDownPoint_;

  NSCellStateValue state_;
}

@property(assign, nonatomic) NSCellStateValue state;
@property(assign, nonatomic) CGFloat hoverAlpha;
@property(assign, nonatomic) CGFloat alertAlpha;

// Determines if the tab is in the process of animating closed. It may still
// be visible on-screen, but should not respond to/initiate any events. Upon
// setting to NO, clears the target/action of the close button to prevent
// clicks inside it from sending messages.
@property(assign, nonatomic, getter=isClosing) BOOL closing;

// Enables/Disables tracking regions for the tab.
- (void)setTrackingEnabled:(BOOL)enabled;

// Begin showing an "alert" glow (shown to call attention to an unselected
// pinned tab whose title changed).
- (void)startAlert;

// Stop showing the "alert" glow; this won't immediately wipe out any glow, but
// will make it fade away.
- (void)cancelAlert;

@end

// The TabController |controller_| is not the only owner of this view. If the
// controller is released before this view, then we could be hanging onto a
// garbage pointer. To prevent this, the TabController uses this interface to
// clear the |controller_| pointer when it is dying.
@interface TabView (TabControllerInterface)
- (void)setController:(TabController*)controller;
@end

#endif  // CHROME_BROWSER_UI_COCOA_TABS_TAB_VIEW_H_
