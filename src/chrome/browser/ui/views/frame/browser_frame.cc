// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame.h"

#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "chrome/common/chrome_switches.h"
#include "ui/base/theme_provider.h"
#include "views/widget/native_widget.h"

#if defined(OS_WIN) && !defined(USE_AURA)
#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"
#elif defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system/runtime_environment.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// BrowserFrame, public:

BrowserFrame::BrowserFrame(BrowserView* browser_view)
    : native_browser_frame_(NULL),
      root_view_(NULL),
      browser_frame_view_(NULL),
      browser_view_(browser_view) {
  browser_view_->set_frame(this);
  set_is_secondary_widget(false);
  // Don't focus anything on creation, selecting a tab will set the focus.
  set_focus_on_creation(false);
}

BrowserFrame::~BrowserFrame() {
}

void BrowserFrame::InitBrowserFrame() {
  native_browser_frame_ =
      NativeBrowserFrame::CreateNativeBrowserFrame(this, browser_view_);
  views::Widget::InitParams params;
  params.delegate = browser_view_;
  params.native_widget = native_browser_frame_->AsNativeWidget();
  if (browser_view_->browser()->is_type_tabbed()) {
    // Typed panel/popup can only return a size once the widget has been
    // created.
    params.bounds = browser_view_->browser()->GetSavedWindowBounds();
    params.show_state = browser_view_->browser()->GetSavedWindowShowState();
  }
  if (browser_view_->browser()->is_type_panel()) {
    // We need to set the top-most bit when the panel window is created.
    // There is a Windows bug/feature that would very likely prevent the window
    // from being changed to top-most after the window is created without
    // activation.
    params.keep_on_top = true;
  }
  Init(params);
#if defined(OS_CHROMEOS)
  // On ChromeOS we always want top-level windows to appear active.
  if (!browser_view_->IsBrowserTypePopup())
    DisableInactiveRendering();
#endif
}

int BrowserFrame::GetMinimizeButtonOffset() const {
  return native_browser_frame_->GetMinimizeButtonOffset();
}

gfx::Rect BrowserFrame::GetBoundsForTabStrip(views::View* tabstrip) const {
  return browser_frame_view_->GetBoundsForTabStrip(tabstrip);
}

int BrowserFrame::GetHorizontalTabStripVerticalOffset(bool restored) const {
  return browser_frame_view_->GetHorizontalTabStripVerticalOffset(restored);
}

void BrowserFrame::UpdateThrobber(bool running) {
  browser_frame_view_->UpdateThrobber(running);
}

views::View* BrowserFrame::GetFrameView() const {
  return browser_frame_view_;
}

void BrowserFrame::TabStripDisplayModeChanged() {
  if (GetRootView()->has_children()) {
    // Make sure the child of the root view gets Layout again.
    GetRootView()->child_at(0)->InvalidateLayout();
  }
  GetRootView()->Layout();
  native_browser_frame_->TabStripDisplayModeChanged();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameWin, views::Window overrides:

bool BrowserFrame::IsMaximized() const {
#if defined(OS_CHROMEOS) && !defined(USE_AURA)
  if (chromeos::system::runtime_environment::IsRunningOnChromeOS()) {
    return !IsFullscreen() &&
        (!browser_view_->IsBrowserTypePopup() || Widget::IsMaximized());
  }
#endif
  return Widget::IsMaximized();
}

views::internal::RootView* BrowserFrame::CreateRootView() {
  root_view_ = new BrowserRootView(browser_view_, this);
  return root_view_;
}

views::NonClientFrameView* BrowserFrame::CreateNonClientFrameView() {
#if defined(OS_WIN) && !defined(USE_AURA)
  if (ShouldUseNativeFrame()) {
    browser_frame_view_ = new GlassBrowserFrameView(this, browser_view_);
  } else {
#endif
    browser_frame_view_ =
        browser::CreateBrowserNonClientFrameView(this, browser_view_);
#if defined(OS_WIN) && !defined(USE_AURA)
  }
#endif
  return browser_frame_view_;
}

bool BrowserFrame::GetAccelerator(int command_id,
                                  ui::Accelerator* accelerator) {
  return browser_view_->GetAccelerator(command_id, accelerator);
}

ThemeProvider* BrowserFrame::GetThemeProvider() const {
  return ThemeServiceFactory::GetForProfile(
      browser_view_->browser()->profile());
}

void BrowserFrame::OnNativeWidgetActivationChanged(bool active) {
  if (active) {
    // When running under remote desktop, if the remote desktop client is not
    // active on the users desktop, then none of the windows contained in the
    // remote desktop will be activated.  However, NativeWidgetWin::Activate()
    // will still bring this browser window to the foreground.  We explicitly
    // set ourselves as the last active browser window to ensure that we get
    // treated as such by the rest of Chrome.
    BrowserList::SetLastActive(browser_view_->browser());
  }
  Widget::OnNativeWidgetActivationChanged(active);
}

AvatarMenuButton* BrowserFrame::GetAvatarMenuButton() {
  return browser_frame_view_->GetAvatarMenuButton();
}
