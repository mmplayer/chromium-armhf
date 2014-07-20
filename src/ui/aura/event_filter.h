// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_EVENT_FILTER_H_
#define UI_AURA_EVENT_FILTER_H_
#pragma once

#include "base/basictypes.h"
#include "ui/base/events.h"

namespace aura {

class Event;
class MouseEvent;
class TouchEvent;
class Window;

// An object that filters events sent to an owner window, potentially performing
// adjustments to the window's position, size and z-index.
class EventFilter {
 public:
  explicit EventFilter(Window* owner);
  virtual ~EventFilter();

  // Try to handle |event| (before the owner's delegate gets a chance to).
  // Returns true if the event was handled by the WindowManager and should not
  // be forwarded to the owner's delegate.
  // Default implementation for ET_MOUSE_PRESSED and ET_TOUCH_PRESSED focuses
  // the window.
  virtual bool OnMouseEvent(Window* target, MouseEvent* event);
  virtual ui::TouchStatus OnTouchEvent(Window* target, TouchEvent* event);

 protected:
  Window* owner() { return owner_; }

 private:
  // If necessary, activates |window| and changes focus.
  void ActivateIfNecessary(Window* window, Event* event);

  Window* owner_;

  DISALLOW_COPY_AND_ASSIGN(EventFilter);
};

}  // namespace aura

#endif  // UI_AURA_EVENT_FILTER_H_
