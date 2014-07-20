// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DESKTOP_HOST_H_
#define UI_AURA_DESKTOP_HOST_H_
#pragma once

#include "base/message_loop.h"
#include "ui/aura/cursor.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
class Rect;
class Size;
}

namespace aura {

class Desktop;

// DesktopHost bridges between a native window and the embedded Desktop. It
// provides the accelerated widget and maps events from the native os to aura.
class DesktopHost : public MessageLoop::Dispatcher {
 public:
  virtual ~DesktopHost() {}

  // Creates a new DesktopHost. The caller owns the returned value.
  static DesktopHost* Create(const gfx::Rect& bounds);

  // Sets the Desktop this DesktopHost is hosting. DesktopHost does not own the
  // Desktop.
  virtual void SetDesktop(Desktop* desktop) = 0;

  // Returns the accelerated widget.
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() = 0;

  // Shows the DesktopHost.
  virtual void Show() = 0;

  // Gets/Sets the size of the DesktopHost.
  virtual gfx::Size GetSize() const = 0;
  virtual void SetSize(const gfx::Size& size) = 0;

  // Sets the currently displayed cursor.
  virtual void SetCursor(gfx::NativeCursor cursor) = 0;

  // Queries the mouse's current position relative to the host window.
  virtual gfx::Point QueryMouseLocation() = 0;
};

}  // namespace aura

#endif  // UI_AURA_DESKTOP_HOST_H_
