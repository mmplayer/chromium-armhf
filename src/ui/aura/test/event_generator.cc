// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/event_generator.h"

#include "ui/aura/desktop.h"
#include "ui/aura/event.h"

namespace {

gfx::Point CenterOfWindowInDesktopCoordinate(aura::Window* window) {
  gfx::Point center = window->bounds().CenterPoint();
  aura::Desktop* desktop = aura::Desktop::GetInstance();
  aura::Window::ConvertPointToWindow(
      window->parent(), desktop->window(), &center);
  return center;
}

}  // namespace

namespace aura {
namespace test {

EventGenerator::EventGenerator() : flags_(0) {
}

EventGenerator::EventGenerator(const gfx::Point& point)
    : flags_(0),
      current_location_(point) {
}

EventGenerator::EventGenerator(Window* window)
    : flags_(0),
      current_location_(CenterOfWindowInDesktopCoordinate(window)) {
}

EventGenerator::~EventGenerator() {
}

void EventGenerator::ClickLeftButton() {
  PressLeftButton();
  ReleaseLeftButton();
}

void EventGenerator::PressLeftButton() {
  if ((flags_ & ui::EF_LEFT_BUTTON_DOWN) == 0) {
    flags_ |= ui::EF_LEFT_BUTTON_DOWN;
    Dispatch(MouseEvent(ui::ET_MOUSE_PRESSED, current_location_, flags_));
  }
}

void EventGenerator::ReleaseLeftButton() {
  if (flags_ & ui::EF_LEFT_BUTTON_DOWN) {
    flags_ ^= ui::EF_LEFT_BUTTON_DOWN;
    Dispatch(MouseEvent(ui::ET_MOUSE_RELEASED, current_location_, 0));
  }
}

void EventGenerator::MoveMouseTo(const gfx::Point& point) {
  if (flags_ & ui::EF_LEFT_BUTTON_DOWN ) {
    Dispatch(MouseEvent(
        ui::ET_MOUSE_DRAGGED, current_location_.Middle(point), flags_));
    Dispatch(MouseEvent(ui::ET_MOUSE_DRAGGED, point, flags_));
  } else {
    Dispatch(MouseEvent(
        ui::ET_MOUSE_MOVED, current_location_.Middle(point), flags_));
    Dispatch(MouseEvent(ui::ET_MOUSE_MOVED, point, flags_));
  }
  current_location_ = point;
}

void EventGenerator::DragMouseTo(const gfx::Point& point) {
  PressLeftButton();
  MoveMouseTo(point);
  ReleaseLeftButton();
}

void EventGenerator::Dispatch(const Event& event) {
  switch (event.type()) {
    case ui::ET_KEY_PRESSED:
    case ui::ET_KEY_RELEASED:
      aura::Desktop::GetInstance()->OnKeyEvent(
          *static_cast<const KeyEvent*>(&event));
      break;
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSEWHEEL:
      aura::Desktop::GetInstance()->OnMouseEvent(
          *static_cast<const MouseEvent*>(&event));
      break;
    default:
      NOTIMPLEMENTED();
      break;
  }
}

void EventGenerator::MoveMouseToCenterOf(Window* window) {
  MoveMouseTo(CenterOfWindowInDesktopCoordinate(window));
}

}  // namespace test
}  // namespace aura
