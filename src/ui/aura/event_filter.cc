// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/event_filter.h"

#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"

namespace aura {

EventFilter::EventFilter(Window* owner) : owner_(owner) {
}

EventFilter::~EventFilter() {
}

bool EventFilter::OnMouseEvent(Window* target, MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      ActivateIfNecessary(target, event);
      break;
    case ui::ET_MOUSE_MOVED:
      Desktop::GetInstance()->SetCursor(target->GetCursor(event->location()));
      break;
    default:
      break;
  }
  return false;
}

ui::TouchStatus EventFilter::OnTouchEvent(Window* target, TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_PRESSED)
    ActivateIfNecessary(target, event);
  return ui::TOUCH_STATUS_UNKNOWN;
}

void EventFilter::ActivateIfNecessary(
    Window* window,
    Event* event) {
  // TODO(beng): some windows (e.g. disabled ones, tooltips, etc) may not be
  //             focusable.

  Window* active_window = Desktop::GetInstance()->active_window();
  Window* toplevel_window = window;
  while (toplevel_window && toplevel_window != active_window &&
         toplevel_window->parent() &&
         !toplevel_window->parent()->AsToplevelWindowContainer()) {
    toplevel_window = toplevel_window->parent();
  }
  if (toplevel_window == active_window) {
    // |window| is a descendant of the active window, no need to activate.
    window->GetFocusManager()->SetFocusedWindow(window);
    return;
  }
  if (!toplevel_window) {
    // |window| is not in a top level window.
    return;
  }
  if (!toplevel_window->delegate() ||
      !toplevel_window->delegate()->ShouldActivate(event))
    return;

  Desktop::GetInstance()->SetActiveWindow(toplevel_window, window);
}

}  // namespace aura
