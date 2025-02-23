// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_BACKGROUND_CONTENTS_H_
#define CHROME_BROWSER_TAB_CONTENTS_BACKGROUND_CONTENTS_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "content/browser/javascript_dialogs.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/window_container_type.h"
#include "webkit/glue/window_open_disposition.h"

class TabContents;
struct WebPreferences;

namespace gfx {
class Rect;
}

// This class is a peer of TabContents. It can host a renderer, but does not
// have any visible display. Its navigation is not managed by a
// NavigationController because is has no facility for navigating (other than
// programatically view window.location.href) or RenderViewHostManager because
// it is never allowed to navigate across a SiteInstance boundary.
class BackgroundContents : public RenderViewHostDelegate,
                           public RenderViewHostDelegate::View,
                           public NotificationObserver,
                           public content::JavaScriptDialogDelegate {
 public:
  class Delegate {
   public:
    // Called by ShowCreatedWindow. Asks the delegate to attach the opened
    // TabContents to a suitable container (e.g. browser) or to show it if it's
    // a popup window.
    virtual void AddTabContents(TabContents* new_contents,
                                WindowOpenDisposition disposition,
                                const gfx::Rect& initial_pos,
                                bool user_gesture) = 0;

   protected:
    virtual ~Delegate() {}
  };

  BackgroundContents(SiteInstance* site_instance,
                     int routing_id,
                     Delegate* delegate);
  virtual ~BackgroundContents();

  // Provide access to the RenderViewHost for the
  // RenderViewHostDelegateViewHelper
  RenderViewHost* render_view_host() { return render_view_host_; }

  // RenderViewHostDelegate implementation.
  virtual BackgroundContents* GetAsBackgroundContents() OVERRIDE;
  virtual RenderViewHostDelegate::View* GetViewDelegate() OVERRIDE;
  virtual const GURL& GetURL() const OVERRIDE;
  virtual content::ViewType GetRenderViewType() const OVERRIDE;
  virtual void DidNavigate(
      RenderViewHost* render_view_host,
      const ViewHostMsg_FrameNavigate_Params& params) OVERRIDE;
  virtual WebPreferences GetWebkitPrefs() OVERRIDE;
  virtual void RunJavaScriptMessage(const RenderViewHost* rvh,
                                    const string16& message,
                                    const string16& default_prompt,
                                    const GURL& frame_url,
                                    const int flags,
                                    IPC::Message* reply_msg,
                                    bool* did_suppress_message) OVERRIDE;
  virtual void Close(RenderViewHost* render_view_host) OVERRIDE;
  virtual RendererPreferences GetRendererPrefs(
      content::BrowserContext* browser_context) const OVERRIDE;
  virtual void RenderViewGone(RenderViewHost* rvh,
                              base::TerminationStatus status,
                              int error_code) OVERRIDE;

  // RenderViewHostDelegate::View
  virtual void CreateNewWindow(
      int route_id,
      const ViewHostMsg_CreateWindow_Params& params);
  virtual void CreateNewWidget(int route_id, WebKit::WebPopupType popup_type);
  virtual void CreateNewFullscreenWidget(int route_id);
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture);
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos);
  virtual void ShowCreatedFullscreenWidget(int route_id);
  virtual void ShowContextMenu(const ContextMenuParams& params) {}
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned) {}
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_operations,
                             const SkBitmap& image,
                             const gfx::Point& image_offset) {}
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) {}
  virtual void GotFocus() {}
  virtual void TakeFocus(bool reverse) {}

  // NotificationObserver
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from JavaScriptDialogDelegate:
  virtual void OnDialogClosed(IPC::Message* reply_msg,
                              bool success,
                              const string16& user_input) OVERRIDE;
  virtual gfx::NativeWindow GetDialogRootWindow() OVERRIDE;

  // Helper to find the BackgroundContents that originated the given request.
  // Can be NULL if the page has been closed or some other error occurs.
  // Should only be called from the UI thread, since it accesses
  // BackgroundContents.
  static BackgroundContents* GetBackgroundContentsByID(int render_process_id,
                                                       int render_view_id);

 protected:
  // Exposed for testing.
  BackgroundContents();

 private:
  // The delegate for this BackgroundContents.
  Delegate* delegate_;

  // The host for our HTML content.
  RenderViewHost* render_view_host_;

  // Common implementations of some RenderViewHostDelegate::View methods.
  RenderViewHostDelegateViewHelper delegate_view_helper_;

  // The URL being hosted.
  GURL url_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundContents);
};

// This is the data sent out as the details with BACKGROUND_CONTENTS_OPENED.
struct BackgroundContentsOpenedDetails {
  // The BackgroundContents object that has just been opened.
  BackgroundContents* contents;

  // The name of the parent frame for these contents.
  const string16& frame_name;

  // The ID of the parent application (if any).
  const string16& application_id;
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_BACKGROUND_CONTENTS_H_
