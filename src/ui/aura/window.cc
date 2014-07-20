// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window.h"

#include <algorithm>

#include "base/logging.h"
#include "ui/aura/desktop.h"
#include "ui/aura/desktop_delegate.h"
#include "ui/aura/event.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_types.h"
#include "ui/base/animation/multi_animation.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/screen.h"

namespace aura {

using internal::RootWindow;

Window::Window(WindowDelegate* delegate)
    : type_(kWindowType_Toplevel),
      delegate_(delegate),
      show_state_(ui::SHOW_STATE_NORMAL),
      parent_(NULL),
      id_(-1),
      user_data_(NULL),
      stops_event_propagation_(false) {
}

Window::~Window() {
  // Let the delegate know we're in the processing of destroying.
  if (delegate_)
    delegate_->OnWindowDestroying();

  // Let the root know so that it can remove any references to us.
  RootWindow* root = GetRoot();
  if (root)
    root->WindowDestroying(this);

  // Then destroy the children.
  while (!children_.empty()) {
    Window* child = children_[0];
    delete child;
    // Deleting the child so remove it from out children_ list.
    DCHECK(std::find(children_.begin(), children_.end(), child) ==
           children_.end());
  }
  // And let the delegate do any post cleanup.
  if (delegate_)
    delegate_->OnWindowDestroyed();
  if (parent_)
    parent_->RemoveChild(this);
}

void Window::Init() {
  ui::Layer::LayerType type = ui::Layer::LAYER_HAS_NO_TEXTURE;
  if (delegate_)
    type = ui::Layer::LAYER_HAS_TEXTURE;
  layer_.reset(new ui::Layer(Desktop::GetInstance()->compositor(), type));
  // Windows (and therefore their layers) should initially be hidden, except for
  // controls.
  layer_->SetVisible(type_ == kWindowType_Control);
  layer_->set_delegate(this);
}

void Window::SetType(int type) {
  // Cannot change type after the window is initialized.
  DCHECK(!layer());
  type_ = type;
}

void Window::Show() {
  SetVisible(true);
}

void Window::Hide() {
  SetVisible(false);
  ReleaseCapture();
  if (Desktop::GetInstance()->active_window() == this ||
      !Desktop::GetInstance()->active_window()) {
    Desktop::GetInstance()->ActivateTopmostWindow();
  }
}

bool Window::IsVisible() const {
  return layer_->IsDrawn();
}

void Window::Maximize() {
  if (UpdateShowStateAndRestoreBounds(ui::SHOW_STATE_MAXIMIZED))
    SetBoundsInternal(gfx::Screen::GetMonitorWorkAreaNearestWindow(this));
}

void Window::Fullscreen() {
  if (UpdateShowStateAndRestoreBounds(ui::SHOW_STATE_FULLSCREEN))
    SetBoundsInternal(gfx::Screen::GetMonitorAreaNearestWindow(this));
}

void Window::Restore() {
  if (show_state_ != ui::SHOW_STATE_NORMAL) {
    show_state_ = ui::SHOW_STATE_NORMAL;
    SetBoundsInternal(restore_bounds_);
    restore_bounds_.SetRect(0, 0, 0, 0);
  }
}

void Window::Activate() {
  // If we support minimization need to ensure this restores the window first.
  aura::Desktop::GetInstance()->SetActiveWindow(this, this);
}

void Window::Deactivate() {
  aura::Desktop::GetInstance()->Deactivate(this);
}

bool Window::IsActive() const {
  return aura::Desktop::GetInstance()->active_window() == this;
}

ToplevelWindowContainer* Window::AsToplevelWindowContainer() {
  return NULL;
}

const ToplevelWindowContainer* Window::AsToplevelWindowContainer() const {
  return NULL;
}

void Window::SetLayoutManager(LayoutManager* layout_manager) {
  layout_manager_.reset(layout_manager);
}

void Window::SetBounds(const gfx::Rect& new_bounds) {
  if (show_state_ == ui::SHOW_STATE_MAXIMIZED ||
      show_state_ == ui::SHOW_STATE_FULLSCREEN) {
    restore_bounds_ = new_bounds;
    return;
  }
  SetBoundsInternal(new_bounds);
}

const gfx::Rect& Window::bounds() const {
  return layer_->bounds();
}

void Window::SchedulePaintInRect(const gfx::Rect& rect) {
  layer_->SchedulePaint(rect);
}

void Window::SetCanvas(const SkCanvas& canvas, const gfx::Point& origin) {
  // TODO: figure out how this is going to work when animating the layer. In
  // particular if we're animating the size then the underlying Texture is going
  // to be unhappy if we try to set a texture on a size bigger than the size of
  // the texture.
  layer_->SetCanvas(canvas, origin);
}

void Window::SetParent(Window* parent) {
  if (parent)
    parent->AddChild(this);
  else if (Desktop::GetInstance()->delegate())
    Desktop::GetInstance()->delegate()->AddChildToDefaultParent(this);
  else
    NOTREACHED();
}

void Window::MoveChildToFront(Window* child) {
  DCHECK_EQ(child->parent(), this);
  const Windows::iterator i(std::find(children_.begin(), children_.end(),
                                      child));
  DCHECK(i != children_.end());
  children_.erase(i);

  // TODO(beng): this obviously has to handle different window types.
  children_.insert(children_.begin() + children_.size(), child);
  SchedulePaintInRect(gfx::Rect());

  child->layer()->parent()->MoveToFront(child->layer());
}

bool Window::CanActivate() const {
  return IsVisible() && delegate_ && delegate_->ShouldActivate(NULL);
}

void Window::AddChild(Window* child) {
  DCHECK(std::find(children_.begin(), children_.end(), child) ==
      children_.end());
  if (child->parent())
    child->parent()->RemoveChild(child);
  child->parent_ = this;
  layer_->Add(child->layer_.get());
  children_.push_back(child);
  FOR_EACH_OBSERVER(WindowObserver, observers_, OnWindowAdded(child));
}

void Window::RemoveChild(Window* child) {
  Windows::iterator i = std::find(children_.begin(), children_.end(), child);
  DCHECK(i != children_.end());
  FOR_EACH_OBSERVER(WindowObserver, observers_, OnWillRemoveWindow(child));
  child->parent_ = NULL;
  layer_->Remove(child->layer_.get());
  children_.erase(i);
}

Window* Window::GetChildById(int id) {
  return const_cast<Window*>(const_cast<const Window*>(this)->GetChildById(id));
}

const Window* Window::GetChildById(int id) const {
  Windows::const_iterator i;
  for (i = children_.begin(); i != children_.end(); ++i) {
    if ((*i)->id() == id)
      return *i;
    const Window* result = (*i)->GetChildById(id);
    if (result)
      return result;
  }
  return NULL;
}

// static
void Window::ConvertPointToWindow(Window* source,
                                  Window* target,
                                  gfx::Point* point) {
  ui::Layer::ConvertPointToLayer(source->layer(), target->layer(), point);
}

gfx::NativeCursor Window::GetCursor(const gfx::Point& point) const {
  return delegate_ ? delegate_->GetCursor(point) : gfx::kNullCursor;
}

void Window::SetEventFilter(EventFilter* event_filter) {
  event_filter_.reset(event_filter);
}

bool Window::OnMouseEvent(MouseEvent* event) {
  if (!parent_ || !IsVisible())
    return false;
  if (!parent_->event_filter_.get())
    parent_->SetEventFilter(new EventFilter(parent_));
  return parent_->event_filter_->OnMouseEvent(this, event) ||
      delegate_->OnMouseEvent(event);
}

bool Window::OnKeyEvent(KeyEvent* event) {
  return IsVisible() && delegate_->OnKeyEvent(event);
}

ui::TouchStatus Window::OnTouchEvent(TouchEvent* event) {
  if (!parent_ || !IsVisible())
    return ui::TOUCH_STATUS_UNKNOWN;

  if (!parent_->event_filter_.get())
    parent_->SetEventFilter(new EventFilter(parent_));

  ui::TouchStatus status = parent_->event_filter_->OnTouchEvent(this, event);
  if (status == ui::TOUCH_STATUS_UNKNOWN && delegate_)
    status = delegate_->OnTouchEvent(event);
  return status;
}

void Window::AddObserver(WindowObserver* observer) {
  observers_.AddObserver(observer);
}

void Window::RemoveObserver(WindowObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool Window::ContainsPoint(const gfx::Point& local_point) {
  gfx::Rect local_bounds(gfx::Point(), bounds().size());
  return local_bounds.Contains(local_point);
}

bool Window::HitTest(const gfx::Point& local_point) {
  // TODO(beng): hittest masks.
  return ContainsPoint(local_point);
}

Window* Window::GetEventHandlerForPoint(const gfx::Point& local_point) {
  return GetWindowForPoint(local_point, true, true);
}

Window* Window::GetTopWindowContainingPoint(const gfx::Point& local_point) {
  return GetWindowForPoint(local_point, false, false);
}

void Window::Focus() {
  DCHECK(GetFocusManager());
  GetFocusManager()->SetFocusedWindow(this);
}

void Window::Blur() {
  DCHECK(GetFocusManager());
  GetFocusManager()->SetFocusedWindow(NULL);
}

bool Window::HasFocus() const {
  const internal::FocusManager* focus_manager = GetFocusManager();
  return focus_manager ? focus_manager->IsFocusedWindow(this) : false;
}

// For a given window, we determine its focusability by inspecting each sibling
// after it (i.e. drawn in front of it in the z-order) to see if it stops
// propagation of events that would otherwise be targeted at windows behind it.
// We then perform this same check on every window up to the root.
bool Window::CanFocus() const {
  // TODO(beng): Figure out how to consult the delegate wrt. focusability also.
  if (!IsVisible() || !parent_)
    return false;

  Windows::const_iterator i = std::find(parent_->children().begin(),
                                        parent_->children().end(),
                                        this);
  for (++i; i != parent_->children().end(); ++i) {
    if ((*i)->StopsEventPropagation())
      return false;
  }
  return parent_->CanFocus();
}

internal::FocusManager* Window::GetFocusManager() {
  return const_cast<internal::FocusManager*>(
      const_cast<const Window*>(this)->GetFocusManager());
}

const internal::FocusManager* Window::GetFocusManager() const {
  return parent_ ? parent_->GetFocusManager() : NULL;
}

void Window::SetCapture() {
  if (!IsVisible())
    return;

  RootWindow* root = GetRoot();
  if (!root)
    return;

  root->SetCapture(this);
}

void Window::ReleaseCapture() {
  RootWindow* root = GetRoot();
  if (!root)
    return;

  root->ReleaseCapture(this);
}

bool Window::HasCapture() {
  RootWindow* root = GetRoot();
  return root && root->capture_window() == this;
}

Window* Window::GetToplevelWindow() {
  Window* window = this;
  while (window && window->parent() &&
         !window->parent()->AsToplevelWindowContainer())
    window = window->parent();
  return window && window->parent() ? window : NULL;
}

bool Window::IsOrContainsFullscreenWindow() const {
  if (delegate_)
    return IsVisible() && show_state_ == ui::SHOW_STATE_FULLSCREEN;

  for (Windows::const_iterator it = children_.begin();
       it != children_.end(); ++it) {
    if ((*it)->IsOrContainsFullscreenWindow())
      return true;
  }
  return false;
}

// static
ui::Animation* Window::CreateDefaultAnimation() {
  std::vector<ui::MultiAnimation::Part> parts;
  parts.push_back(ui::MultiAnimation::Part(200, ui::Tween::LINEAR));
  ui::MultiAnimation* multi_animation = new ui::MultiAnimation(parts);
  multi_animation->set_continuous(false);
  return multi_animation;
}

internal::RootWindow* Window::GetRoot() {
  return parent_ ? parent_->GetRoot() : NULL;
}

void Window::SetBoundsInternal(const gfx::Rect& new_bounds) {
  const gfx::Rect old_bounds = bounds();
  bool was_move = old_bounds.size() == new_bounds.size();
  layer_->SetBounds(new_bounds);

  if (layout_manager_.get())
    layout_manager_->OnWindowResized();
  if (delegate_)
    delegate_->OnBoundsChanged(old_bounds, new_bounds);
  if (IsVisible()) {
    if (was_move)
      layer()->ScheduleDraw();
    else
      layer()->SchedulePaint(gfx::Rect());
  }
}

void Window::SetVisible(bool visible) {
  if (visible == layer_->visible())
    return;  // No change.

  bool was_visible = IsVisible();
  layer_->SetVisible(visible);
  bool is_visible = IsVisible();
  if (was_visible != is_visible) {
    SchedulePaint();
    if (delegate_)
      delegate_->OnWindowVisibilityChanged(is_visible);
  }
  FOR_EACH_OBSERVER(WindowObserver, observers_,
                    OnWindowVisibilityChanged(this, is_visible));
}

void Window::SchedulePaint() {
  SchedulePaintInRect(gfx::Rect(0, 0, bounds().width(), bounds().height()));
}

bool Window::StopsEventPropagation() const {
  return stops_event_propagation_ && !children_.empty();
}

bool Window::UpdateShowStateAndRestoreBounds(
    ui::WindowShowState new_show_state) {
  if (show_state_ == new_show_state)
    return false;
  show_state_ = new_show_state;
  if (restore_bounds_.IsEmpty())
    restore_bounds_ = bounds();
  return true;
}

Window* Window::GetWindowForPoint(const gfx::Point& local_point,
                                  bool return_tightest,
                                  bool for_event_handling) {
  if (!IsVisible())
    return NULL;

  if ((for_event_handling && !HitTest(local_point)) ||
      (!for_event_handling && !ContainsPoint(local_point)))
    return NULL;

  if (!return_tightest && delegate_)
    return this;

  for (Windows::const_reverse_iterator it = children_.rbegin();
       it != children_.rend(); ++it) {
    Window* child = *it;
    if (!child->IsVisible())
      continue;

    gfx::Point point_in_child_coords(local_point);
    Window::ConvertPointToWindow(this, child, &point_in_child_coords);
    Window* match = child->GetWindowForPoint(point_in_child_coords,
                                             return_tightest,
                                             for_event_handling);
    if (match)
      return match;

    if (for_event_handling && child->StopsEventPropagation())
      break;
  }

  return delegate_ ? this : NULL;
}

void Window::OnPaintLayer(gfx::Canvas* canvas) {
  delegate_->OnPaint(canvas);
}

}  // namespace aura
