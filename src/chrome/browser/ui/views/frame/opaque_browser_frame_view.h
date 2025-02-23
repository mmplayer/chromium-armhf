// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "views/controls/button/button.h"
#include "views/window/non_client_view.h"

class BrowserView;
namespace gfx {
class Font;
}
class TabContents;
namespace views {
class ImageButton;
class ImageView;
}

class OpaqueBrowserFrameView : public BrowserNonClientFrameView,
                               public NotificationObserver,
                               public views::ButtonListener,
                               public TabIconView::TabIconViewModel {
 public:
  // Constructs a non-client view for an BrowserFrame.
  OpaqueBrowserFrameView(BrowserFrame* frame, BrowserView* browser_view);
  virtual ~OpaqueBrowserFrameView();

  // Overridden from BrowserNonClientFrameView:
  virtual gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const OVERRIDE;
  virtual int GetHorizontalTabStripVerticalOffset(bool restored) const OVERRIDE;
  virtual void UpdateThrobber(bool running) OVERRIDE;
  virtual gfx::Size GetMinimumSize() OVERRIDE;

 protected:
  BrowserView* browser_view() const { return browser_view_; }
  views::ImageButton* minimize_button() const { return minimize_button_; }
  views::ImageButton* maximize_button() const { return maximize_button_; }
  views::ImageButton* restore_button() const { return restore_button_; }
  views::ImageButton* close_button() const { return close_button_; }

  // Used to allow subclasses to reserve height for other components they
  // will add.  The space is reserved below the ClientView.
  virtual int GetReservedHeight() const;
  virtual gfx::Rect GetBoundsForReservedArea() const;

  // Returns the height of the entire nonclient top border, including the window
  // frame, any title area, and any connected client edge.  If |restored| is
  // true, acts as if the window is restored regardless of the real mode.
  int NonClientTopBorderHeight(bool restored) const;

  // Allows a subclass to tweak the frame. Chromeos uses this to support
  // drawing themes correctly. |theme_offset| is used to adjust the y offset
  // of the theme frame bitmap, so they start at the right location.
  // |left_corner| and |right_corner| will be used on the left and right of
  // the tabstrip area as opposed to the theme frame.
  virtual void ModifyMaximizedFramePainting(
      int* theme_offset,
      SkBitmap** left_corner,
      SkBitmap** right_corner);

  // Expose these to subclasses.
  BrowserFrame* frame() { return frame_; }
  BrowserView* browser_view() { return browser_view_; }

  // Overridden from views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask)
      OVERRIDE;
  virtual void EnableClose(bool enable) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;
  virtual AvatarMenuButton* GetAvatarMenuButton() OVERRIDE;

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual bool HitTest(const gfx::Point& l) const OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event)
      OVERRIDE;

  // Overridden from TabIconView::TabIconViewModel:
  virtual bool ShouldTabIconViewAnimate() const OVERRIDE;
  virtual SkBitmap GetFaviconForTabIconView() OVERRIDE;

 protected:
  // NotificationObserver implementation:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  // Returns the thickness of the border that makes up the window frame edges.
  // This does not include any client edge.  If |restored| is true, acts as if
  // the window is restored regardless of the real mode.
  int FrameBorderThickness(bool restored) const;

  // Returns the height of the top resize area.  This is smaller than the frame
  // border height in order to increase the window draggable area.
  int TopResizeHeight() const;

  // Returns the thickness of the entire nonclient left, right, and bottom
  // borders, including both the window frame and any client edge.
  int NonClientBorderThickness() const;

  // Returns the y-coordinate of the caption buttons.  If |restored| is true,
  // acts as if the window is restored regardless of the real mode.
  int CaptionButtonY(bool restored) const;

  // Returns the thickness of the 3D edge along the bottom of the titlebar.  If
  // |restored| is true, acts as if the window is restored regardless of the
  // real mode.
  int TitlebarBottomThickness(bool restored) const;

  // Returns the size of the titlebar icon.  This is used even when the icon is
  // not shown, e.g. to set the titlebar height.
  int IconSize() const;

  // Returns the bounds of the titlebar icon (or where the icon would be if
  // there was one).
  gfx::Rect IconBounds() const;

  // Paint various sub-components of this view.  The *FrameBorder() functions
  // also paint the background of the titlebar area, since the top frame border
  // and titlebar background are a contiguous component.
  void PaintRestoredFrameBorder(gfx::Canvas* canvas);
  void PaintMaximizedFrameBorder(gfx::Canvas* canvas);
  void PaintTitleBar(gfx::Canvas* canvas);
  void PaintToolbarBackground(gfx::Canvas* canvas);
  void PaintRestoredClientEdge(gfx::Canvas* canvas);

  // Returns the properly themed bitmap and frame color, given various
  // attributes of this view (normal browser or not, OTR or not, active or not).
  SkBitmap* GetFrameBitmap() const;
  SkColor GetFrameColor() const;

  // Layout various sub-components of this view.
  void LayoutWindowControls();
  void LayoutTitleBar();
  void LayoutAvatar();

  // Returns the bounds of the client area for the specified view size.
  gfx::Rect CalculateClientAreaBounds(int width, int height) const;

  // Updates the title and icon of the avatar button.
  void UpdateAvatarInfo();

  // The layout rect of the title, if visible.
  gfx::Rect title_bounds_;

  // The layout rect of the avatar icon, if visible.
  gfx::Rect avatar_bounds_;

  // Window controls.
  views::ImageButton* minimize_button_;
  views::ImageButton* maximize_button_;
  views::ImageButton* restore_button_;
  views::ImageButton* close_button_;

  // The Window icon.
  TabIconView* window_icon_;

  // The frame that hosts this view.
  BrowserFrame* frame_;

  // The BrowserView hosted within this View.
  BrowserView* browser_view_;

  // The bounds of the ClientView.
  gfx::Rect client_view_bounds_;

  // Menu button that displays that either the incognito icon or the profile
  // icon.
  scoped_ptr<AvatarMenuButton> avatar_button_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(OpaqueBrowserFrameView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_H_
