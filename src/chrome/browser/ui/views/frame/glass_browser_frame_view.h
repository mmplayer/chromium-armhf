// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_FRAME_VIEW_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/frame/browser_frame_win.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "views/controls/button/button.h"
#include "views/window/non_client_view.h"

class BrowserView;
class AvatarMenuButton;
class SkBitmap;

class GlassBrowserFrameView : public BrowserNonClientFrameView,
                              public NotificationObserver {
 public:
  // Constructs a non-client view for an BrowserFrame.
  GlassBrowserFrameView(BrowserFrame* frame, BrowserView* browser_view);
  virtual ~GlassBrowserFrameView();

  // Overridden from BrowserNonClientFrameView:
  virtual gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const OVERRIDE;
  virtual int GetHorizontalTabStripVerticalOffset(bool restored) const OVERRIDE;
  virtual void UpdateThrobber(bool running) OVERRIDE;
  virtual gfx::Size GetMinimumSize() OVERRIDE;

  // Overridden from views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask)
      OVERRIDE { }
  virtual void EnableClose(bool enable) OVERRIDE { }
  virtual void ResetWindowControls() OVERRIDE { }
  virtual void UpdateWindowIcon() OVERRIDE { }
  virtual AvatarMenuButton* GetAvatarMenuButton() OVERRIDE;

 protected:
  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual bool HitTest(const gfx::Point& l) const OVERRIDE;

 private:
  // Returns the thickness of the border that makes up the window frame edges.
  // This does not include any client edge.
  int FrameBorderThickness() const;

  // Returns the thickness of the entire nonclient left, right, and bottom
  // borders, including both the window frame and any client edge.
  int NonClientBorderThickness() const;

  // Returns the height of the entire nonclient top border, including the window
  // frame, any title area, and any connected client edge.  If |restored| is
  // true, acts as if the window is restored regardless of the real mode.
  int NonClientTopBorderHeight(bool restored) const;

  // Paint various sub-components of this view.
  void PaintToolbarBackground(gfx::Canvas* canvas);
  void PaintRestoredClientEdge(gfx::Canvas* canvas);

  // Layout various sub-components of this view.
  void LayoutAvatar();
  void LayoutClientView();

  // Returns the insets of the client area.
  gfx::Insets GetClientAreaInsets() const;

  // Returns the bounds of the client area for the specified view size.
  gfx::Rect CalculateClientAreaBounds(int width, int height) const;

  // Starts/Stops the window throbber running.
  void StartThrobber();
  void StopThrobber();

  // Displays the next throbber frame.
  void DisplayNextThrobberFrame();

  // NotificationObserver implementation:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Updates the title and icon of the avatar button.
  void UpdateAvatarInfo();

  // The layout rect of the avatar icon, if visible.
  gfx::Rect avatar_bounds_;

  // The frame that hosts this view.
  BrowserFrame* frame_;

  // The BrowserView hosted within this View.
  BrowserView* browser_view_;

  // The bounds of the ClientView.
  gfx::Rect client_view_bounds_;

  // Menu button that displays that either the incognito icon or the profile
  // icon.
  scoped_ptr<AvatarMenuButton> avatar_button_;

  // Whether or not the window throbber is currently animating.
  bool throbber_running_;

  // The index of the current frame of the throbber animation.
  int throbber_frame_;

  NotificationRegistrar registrar_;

  static const int kThrobberIconCount = 24;
  static HICON throbber_icons_[kThrobberIconCount];
  static void InitThrobberIcons();

  DISALLOW_COPY_AND_ASSIGN(GlassBrowserFrameView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_FRAME_VIEW_H_
