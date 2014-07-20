// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/native_widget_view.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/native_widget_types.h"

namespace views {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetView, public:

// static
const char NativeWidgetView::kViewClassName[] = "views/NativeWidgetView";

NativeWidgetView::NativeWidgetView(NativeWidgetViews* native_widget)
    : native_widget_(native_widget),
      sent_create_(false),
      delete_native_widget_(true),
      cursor_(gfx::kNullCursor) {
}

NativeWidgetView::~NativeWidgetView() {
  // Don't let NativeWidgetViews delete this again.  This must be outside
  // the |delete_native_widget_| clause so it gets invoked for
  // WIDGET_OWNS_NATIVE_WIDGET.  It is safe because |native_widget_| will
  // still exist in both ways NativeWidgetView can be destroyed: by view
  // hierarchy teardown and from the NativeWidgetViews destructor.
  native_widget_->set_delete_native_view(false);
  if (delete_native_widget_)
    delete native_widget_;
}

Widget* NativeWidgetView::GetAssociatedWidget() {
  return native_widget_->delegate()->AsWidget();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetView, View overrides:

void NativeWidgetView::CalculateOffsetToAncestorWithLayer(
    gfx::Point* offset,
    ui::Layer** layer_parent) {
  View::CalculateOffsetToAncestorWithLayer(offset, layer_parent);
}

void NativeWidgetView::ReorderLayers() {
  View::ReorderLayers();
}

#if !defined(NDEBUG)
std::string NativeWidgetView::PrintViewGraph(bool first) {
  return DoPrintViewGraph(first, GetAssociatedWidget()->GetRootView());
}
#endif

void NativeWidgetView::ViewHierarchyChanged(bool is_add,
                                            View* parent,
                                            View* child) {
  if (is_add && child == this)  {
    GetAssociatedWidget()->GetRootView()->UpdateParentLayers();
    if (!sent_create_) {
      sent_create_ = true;
      delegate()->OnNativeWidgetCreated();
    }
  }
}

void NativeWidgetView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  native_widget_->OnBoundsChanged(bounds(), previous_bounds);
}

void NativeWidgetView::OnPaint(gfx::Canvas* canvas) {
  delegate()->OnNativeWidgetPaint(canvas);
}

gfx::NativeCursor NativeWidgetView::GetCursor(const MouseEvent& event) {
  return cursor_;
}

bool NativeWidgetView::OnMousePressed(const MouseEvent& event) {
  return native_widget_->OnMouseEvent(event);
}

bool NativeWidgetView::OnMouseDragged(const MouseEvent& event) {
  return native_widget_->OnMouseEvent(event);
}

void NativeWidgetView::OnMouseReleased(const MouseEvent& event) {
  native_widget_->OnMouseEvent(event);
}

void NativeWidgetView::OnMouseCaptureLost() {
  delegate()->OnMouseCaptureLost();
}

void NativeWidgetView::OnMouseMoved(const MouseEvent& event) {
  native_widget_->OnMouseEvent(event);
}

void NativeWidgetView::OnMouseEntered(const MouseEvent& event) {
  native_widget_->OnMouseEvent(event);
}

void NativeWidgetView::OnMouseExited(const MouseEvent& event) {
  native_widget_->OnMouseEvent(event);
}

ui::TouchStatus NativeWidgetView::OnTouchEvent(const TouchEvent& event) {
  return delegate()->OnTouchEvent(event);
}

bool NativeWidgetView::OnKeyPressed(const KeyEvent& event) {
  return delegate()->OnKeyEvent(event);
}

bool NativeWidgetView::OnKeyReleased(const KeyEvent& event) {
  return delegate()->OnKeyEvent(event);
}

bool NativeWidgetView::OnMouseWheel(const MouseWheelEvent& event) {
  return native_widget_->OnMouseEvent(event);
}

void NativeWidgetView::VisibilityChanged(View* starting_from,
                                         bool visible) {
  delegate()->OnNativeWidgetVisibilityChanged(visible);
}

void NativeWidgetView::OnFocus() {
  // TODO(beng): check if we have to do this.
  //delegate()->OnNativeFocus(NULL);
}

void NativeWidgetView::OnBlur() {
  // TODO(beng): check if we have to do this.
  //delegate()->OnNativeBlur(NULL);
}

std::string NativeWidgetView::GetClassName() const {
  return kViewClassName;
}

void NativeWidgetView::MoveLayerToParent(ui::Layer* parent_layer,
                                         const gfx::Point& point) {
  View::MoveLayerToParent(parent_layer, point);
  if (!layer() || parent_layer == layer()) {
    gfx::Point new_offset(point);
    if (layer() != parent_layer)
      new_offset.Offset(x(), y());
    GetAssociatedWidget()->GetRootView()->MoveLayerToParent(
        parent_layer, new_offset);
  }
}

void NativeWidgetView::UpdateChildLayerBounds(const gfx::Point& offset) {
  View::UpdateChildLayerBounds(offset);
  if (!layer()) {
    const gfx::Rect& bounds = GetAssociatedWidget()->GetRootView()->bounds();
    gfx::Point new_offset(offset.x() + bounds.x(), offset.y() + bounds.y());
    GetAssociatedWidget()->GetRootView()->UpdateChildLayerBounds(new_offset);
  }
}

void NativeWidgetView::ReorderChildLayers(ui::Layer* parent_layer) {
  if (layer()) {
    View::ReorderChildLayers(parent_layer);
  } else {
    GetAssociatedWidget()->GetRootView()->ReorderChildLayers(parent_layer);
  }
}

}  // namespace internal
}  // namespace views
