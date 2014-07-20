// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_aura.h"

#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_delegate.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_views.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "ui/aura/event.h"
#include "ui/aura/window.h"
#include "views/views_delegate.h"
#include "views/widget/widget.h"

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewAura, public:

NativeTabContentsViewAura::NativeTabContentsViewAura(
    internal::NativeTabContentsViewDelegate* delegate)
    : views::NativeWidgetAura(delegate->AsNativeWidgetDelegate()),
      delegate_(delegate) {
}

NativeTabContentsViewAura::~NativeTabContentsViewAura() {
}

TabContents* NativeTabContentsViewAura::GetTabContents() const {
  return delegate_->GetTabContents();
}

void NativeTabContentsViewAura::EndDragging() {
  delegate_->OnNativeTabContentsViewDraggingEnded();
  // TODO(beng):
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewAura, NativeTabContentsView implementation:

void NativeTabContentsViewAura::InitNativeTabContentsView() {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.native_widget = this;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = NULL;
  GetWidget()->Init(params);
}

void NativeTabContentsViewAura::Unparent() {
  // Note that we do not DCHECK on focus_manager_ as it may be NULL when used
  // with an external tab container.
  views::Widget::ReparentNativeView(GetNativeView(), NULL);
  // TODO(beng):
  NOTIMPLEMENTED();
}

RenderWidgetHostView* NativeTabContentsViewAura::CreateRenderWidgetHostView(
    RenderWidgetHost* render_widget_host) {
  RenderWidgetHostViewAura* view =
      new RenderWidgetHostViewAura(render_widget_host);
  view->Init();
  GetNativeView()->AddChild(view->GetNativeView());
  view->Show();
  return view;
}

gfx::NativeWindow NativeTabContentsViewAura::GetTopLevelNativeWindow() const {
  // TODO(beng):
  NOTIMPLEMENTED();
  return NULL;
}

void NativeTabContentsViewAura::SetPageTitle(const string16& title) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

void NativeTabContentsViewAura::StartDragging(const WebDropData& drop_data,
                                             WebKit::WebDragOperationsMask ops,
                                             const SkBitmap& image,
                                             const gfx::Point& image_offset) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

void NativeTabContentsViewAura::CancelDrag() {
  // TODO(beng):
  NOTIMPLEMENTED();
}

bool NativeTabContentsViewAura::IsDoingDrag() const {
  // TODO(beng):
  NOTIMPLEMENTED();
  return false;
}

void NativeTabContentsViewAura::SetDragCursor(
    WebKit::WebDragOperation operation) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

views::NativeWidget* NativeTabContentsViewAura::AsNativeWidget() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewAura, views::NativeWidgetAura overrides:

void NativeTabContentsViewAura::OnBoundsChanged(const gfx::Rect& old_bounds,
                                                const gfx::Rect& new_bounds) {
  delegate_->OnNativeTabContentsViewSized(new_bounds.size());
  views::NativeWidgetAura::OnBoundsChanged(old_bounds, new_bounds);
}

bool NativeTabContentsViewAura::OnMouseEvent(aura::MouseEvent* event) {
  if (!delegate_->IsShowingSadTab()) {
    switch (event->type()) {
      case ui::ET_MOUSE_EXITED:
        delegate_->OnNativeTabContentsViewMouseMove(false);
        break;
      case ui::ET_MOUSE_MOVED:
        delegate_->OnNativeTabContentsViewMouseMove(true);
        break;
      default:
        // TODO(oshima): mouse wheel
        break;
    }
  }
  // Pass all mouse event to renderer.
  return views::NativeWidgetAura::OnMouseEvent(event);
}

#if !defined(TOUCH_UI)
////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsView, public:

// static
NativeTabContentsView* NativeTabContentsView::CreateNativeTabContentsView(
    internal::NativeTabContentsViewDelegate* delegate) {
  return new NativeTabContentsViewViews(delegate);
  // TODO(beng): switch over to this.
  // return new NativeTabContentsViewAura(delegate);
}
#endif
