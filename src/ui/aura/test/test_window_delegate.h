// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_WINDOW_DELEGATE_H_
#define UI_AURA_TEST_TEST_WINDOW_DELEGATE_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/aura/window_delegate.h"

namespace aura {
namespace test {

// WindowDelegate implementation with all methods stubbed out.
class TestWindowDelegate : public WindowDelegate {
 public:
  TestWindowDelegate();
  virtual ~TestWindowDelegate();

  // Overridden from WindowDelegate:
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds);
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual bool OnKeyEvent(KeyEvent* event) OVERRIDE;
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) OVERRIDE;
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE;
  virtual bool OnMouseEvent(MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus OnTouchEvent(TouchEvent* event) OVERRIDE;
  virtual bool ShouldActivate(Event* event) OVERRIDE;
  virtual void OnActivated() OVERRIDE;
  virtual void OnLostActive() OVERRIDE;
  virtual void OnCaptureLost() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnWindowDestroying() OVERRIDE;
  virtual void OnWindowDestroyed() OVERRIDE;
  virtual void OnWindowVisibilityChanged(bool visible) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWindowDelegate);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_TEST_WINDOW_DELEGATE_H_
