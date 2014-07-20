// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_SCROLLBAR_NATIVE_SCROLL_BAR_VIEWS_H_
#define VIEWS_CONTROLS_SCROLLBAR_NATIVE_SCROLL_BAR_VIEWS_H_
#pragma once

#include "ui/gfx/native_theme.h"
#include "ui/gfx/point.h"
#include "views/controls/button/button.h"
#include "views/controls/scrollbar/base_scroll_bar.h"
#include "views/controls/scrollbar/native_scroll_bar_wrapper.h"
#include "views/view.h"

namespace gfx {
class Canvas;
}

namespace views {

class NativeScrollBar;

// Views implementation for the scrollbar.
class VIEWS_EXPORT NativeScrollBarViews : public BaseScrollBar,
                                          public ButtonListener,
                                          public NativeScrollBarWrapper {
 public:
  static const char kViewClassName[];

  // Creates new scrollbar, either horizontal or vertical.
  explicit NativeScrollBarViews(NativeScrollBar* native_scroll_bar);
  virtual ~NativeScrollBarViews();

 private:
  // View overrides:
  virtual void Layout();
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;

  // ScrollBar overrides:
  virtual int GetLayoutSize() const OVERRIDE;

  // BaseScrollBar overrides:
  virtual void ScrollToPosition(int position);
  virtual int GetScrollIncrement(bool is_page, bool is_positive);

  // BaseButton::ButtonListener overrides:
  virtual void ButtonPressed(Button* sender,
                             const views::Event& event) OVERRIDE;

  // NativeScrollBarWrapper overrides:
  virtual int GetPosition() const;
  virtual View* GetView();
  virtual void Update(int viewport_size, int content_size, int current_pos);

  // Returns the area for the track. This is the area of the scrollbar minus
  // the size of the arrow buttons.
  virtual gfx::Rect GetTrackBounds() const OVERRIDE;

  // The NativeScrollBar we are bound to.
  NativeScrollBar* native_scroll_bar_;

  // The scroll bar buttons (Up/Down, Left/Right).
  Button* prev_button_;
  Button* next_button_;

  gfx::NativeTheme::ExtraParams params_;
  gfx::NativeTheme::Part part_;
  gfx::NativeTheme::State state_;

  DISALLOW_COPY_AND_ASSIGN(NativeScrollBarViews);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_SCROLLBAR_NATIVE_SCROLL_BAR_VIEWS_H_
