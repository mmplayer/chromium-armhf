// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_view.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "content/browser/content_browser_client.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "views/widget/widget.h"

#if defined(OS_WIN)
#include "content/browser/renderer_host/render_widget_host_view_win.h"
#elif defined(TOUCH_UI) || defined(USE_AURA)
#include "chrome/browser/renderer_host/render_widget_host_view_views.h"
#elif defined(TOOLKIT_USES_GTK)
#include "content/browser/renderer_host/render_widget_host_view_gtk.h"
#endif

ExtensionView::ExtensionView(ExtensionHost* host, Browser* browser)
    : host_(host),
      browser_(browser),
      initialized_(false),
      container_(NULL),
      is_clipped_(false) {
  host_->set_view(this);

  // This view needs to be focusable so it can act as the focused view for the
  // focus manager. This is required to have SkipDefaultKeyEventProcessing
  // called so the tab key events are forwarded to the renderer.
  set_focusable(true);
}

ExtensionView::~ExtensionView() {
  if (parent())
    parent()->RemoveChildView(this);
  CleanUp();
}

const Extension* ExtensionView::extension() const {
  return host_->extension();
}

RenderViewHost* ExtensionView::render_view_host() const {
  return host_->render_view_host();
}

void ExtensionView::DidStopLoading() {
  ShowIfCompletelyLoaded();
}

void ExtensionView::SetIsClipped(bool is_clipped) {
  if (is_clipped_ != is_clipped) {
    is_clipped_ = is_clipped;
    if (IsVisible())
      ShowIfCompletelyLoaded();
  }
}

gfx::NativeCursor ExtensionView::GetCursor(const views::MouseEvent& event) {
  return gfx::kNullCursor;
}

void ExtensionView::SetVisible(bool is_visible) {
  if (is_visible != IsVisible()) {
    NativeViewHost::SetVisible(is_visible);

    // Also tell RenderWidgetHostView the new visibility. Despite its name, it
    // is not part of the View hierarchy and does not know about the change
    // unless we tell it.
    if (render_view_host()->view()) {
      if (is_visible)
        render_view_host()->view()->Show();
      else
        render_view_host()->view()->Hide();
    }
  }
}

void ExtensionView::CreateWidgetHostView() {
  DCHECK(!initialized_);
  initialized_ = true;
  RenderWidgetHostView* view =
      content::GetContentClient()->browser()->CreateViewForWidget(
      render_view_host());

  // TODO(mpcomplete): RWHV needs a cross-platform Init function.
#if defined(USE_AURA)
  // TODO(beng): should be same as TOUCH_UI
  NOTIMPLEMENTED();
#elif defined(OS_WIN)
  // Create the HWND. Note:
  // RenderWidgetHostHWND supports windowed plugins, but if we ever also
  // wanted to support constrained windows with this, we would need an
  // additional HWND to parent off of because windowed plugin HWNDs cannot
  // exist in the same z-order as constrained windows.
  RenderWidgetHostViewWin* view_win =
      static_cast<RenderWidgetHostViewWin*>(view);
  HWND hwnd = view_win->Create(GetWidget()->GetNativeView());
  view_win->ShowWindow(SW_SHOW);
  Attach(hwnd);
#elif defined(TOUCH_UI)
  RenderWidgetHostViewViews* view_views =
      static_cast<RenderWidgetHostViewViews*>(view);
  view_views->InitAsChild();
  AttachToView(view_views);
#elif defined(TOOLKIT_USES_GTK)
  RenderWidgetHostViewGtk* view_gtk =
      static_cast<RenderWidgetHostViewGtk*>(view);
  view_gtk->InitAsChild();
  Attach(view_gtk->GetNativeView());
#else
  NOTIMPLEMENTED();
#endif

  host_->CreateRenderViewSoon(view);
  SetVisible(false);
}

void ExtensionView::ShowIfCompletelyLoaded() {
  if (IsVisible() || is_clipped_)
    return;

  // We wait to show the ExtensionView until it has loaded, and the view has
  // actually been created. These can happen in different orders.
  if (host_->did_stop_loading()) {
    SetVisible(true);
    UpdatePreferredSize(pending_preferred_size_);
  }
}

void ExtensionView::CleanUp() {
  if (!initialized_)
    return;
  if (native_view())
    Detach();
  initialized_ = false;
}

void ExtensionView::SetBackground(const SkBitmap& background) {
  if (render_view_host()->IsRenderViewLive() && render_view_host()->view()) {
    render_view_host()->view()->SetBackground(background);
  } else {
    pending_background_ = background;
  }
  ShowIfCompletelyLoaded();
}

void ExtensionView::UpdatePreferredSize(const gfx::Size& new_size) {
  // Don't actually do anything with this information until we have been shown.
  // Size changes will not be honored by lower layers while we are hidden.
  if (!IsVisible()) {
    pending_preferred_size_ = new_size;
    return;
  }

  gfx::Size preferred_size = GetPreferredSize();
  if (new_size != preferred_size)
    SetPreferredSize(new_size);
}

void ExtensionView::ViewHierarchyChanged(bool is_add,
                                         views::View *parent,
                                         views::View *child) {
  NativeViewHost::ViewHierarchyChanged(is_add, parent, child);
  if (is_add && GetWidget() && !initialized_)
    CreateWidgetHostView();
}

void ExtensionView::PreferredSizeChanged() {
  View::PreferredSizeChanged();
  if (container_)
    container_->OnExtensionPreferredSizeChanged(this);
}

bool ExtensionView::SkipDefaultKeyEventProcessing(const views::KeyEvent& e) {
  // Let the tab key event be processed by the renderer (instead of moving the
  // focus to the next focusable view). Also handle Backspace, since otherwise
  // (on Windows at least), pressing Backspace, when focus is on a text field
  // within the ExtensionView, will navigate the page back instead of erasing a
  // character.
  return (e.key_code() == ui::VKEY_TAB || e.key_code() == ui::VKEY_BACK);
}

void ExtensionView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Propagate the new size to RenderWidgetHostView.
  // We can't send size zero because RenderWidget DCHECKs that.
  if (render_view_host()->view() && !bounds().IsEmpty())
    render_view_host()->view()->SetSize(size());
}

void ExtensionView::HandleMouseMove() {
  if (container_)
    container_->OnExtensionMouseMove(this);
}

void ExtensionView::HandleMouseLeave() {
  if (container_)
    container_->OnExtensionMouseLeave(this);
}

void ExtensionView::RenderViewCreated() {
  if (!pending_background_.empty() && render_view_host()->view()) {
    render_view_host()->view()->SetBackground(pending_background_);
    pending_background_.reset();
  }

  // Tell the renderer not to draw scroll bars in popups unless the
  // popups are at the maximum allowed size.
  gfx::Size largest_popup_size(ExtensionPopup::kMaxWidth,
                               ExtensionPopup::kMaxHeight);
  host_->DisableScrollbarsForSmallWindows(largest_popup_size);
}
