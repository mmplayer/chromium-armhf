// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

#include "base/command_line.h"
#include "chrome/browser/chromeos/frame/browser_frame_view_chromeos.h"
#include "chrome/browser/ui/panels/panel_browser_frame_view.h"
#include "chrome/browser/ui/panels/panel_browser_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/popup_non_client_frame_view.h"
#include "chrome/common/chrome_switches.h"

namespace browser {

BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame, BrowserView* browser_view) {
  Browser::Type type = browser_view->browser()->type();
  switch (type) {
    case Browser::TYPE_PANEL:
      if (CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnablePanels)) {
        return new PanelBrowserFrameView(
            frame, static_cast<PanelBrowserView*>(browser_view));
      }  // else, fall-through and treat as popup
    case Browser::TYPE_POPUP:
      return new PopupNonClientFrameView(frame);
    default:
      return new chromeos::BrowserFrameViewChromeos(frame, browser_view);
  }
}

}  // browser
