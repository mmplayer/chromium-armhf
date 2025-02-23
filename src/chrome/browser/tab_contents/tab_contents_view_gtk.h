// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_VIEW_GTK_H_
#define CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_VIEW_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "chrome/browser/ui/gtk/focus_store_gtk.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"

class ConstrainedWindowGtk;
class RenderViewContextMenuGtk;
class SadTabGtk;
class TabContentsDragSource;
class WebDragBookmarkHandlerGtk;

namespace content {
class WebDragDestGtk;
}

class TabContentsViewGtk : public TabContentsView,
                           public NotificationObserver {
 public:
  // The corresponding TabContents is passed in the constructor, and manages our
  // lifetime. This doesn't need to be the case, but is this way currently
  // because that's what was easiest when they were split.
  explicit TabContentsViewGtk(TabContents* tab_contents);
  virtual ~TabContentsViewGtk();

  // Unlike Windows, ConstrainedWindows need to collaborate with the
  // TabContentsViewGtk to position the dialogs.
  void AttachConstrainedWindow(ConstrainedWindowGtk* constrained_window);
  void RemoveConstrainedWindow(ConstrainedWindowGtk* constrained_window);

  // Override the stored focus widget. This call only makes sense when the
  // tab contents is not focused.
  void SetFocusedWidget(GtkWidget* widget);

  // TabContentsView implementation --------------------------------------------

  virtual void CreateView(const gfx::Size& initial_size) OVERRIDE;
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* render_widget_host) OVERRIDE;

  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeView GetContentNativeView() const OVERRIDE;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const OVERRIDE;
  virtual void GetContainerBounds(gfx::Rect* out) const OVERRIDE;
  virtual void SetPageTitle(const string16& title) OVERRIDE;
  virtual void OnTabCrashed(base::TerminationStatus status,
                            int error_code) OVERRIDE;
  virtual void SizeContents(const gfx::Size& size) OVERRIDE;
  virtual void RenderViewCreated(RenderViewHost* host) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void SetInitialFocus() OVERRIDE;
  virtual void StoreFocus() OVERRIDE;
  virtual void RestoreFocus() OVERRIDE;
  virtual bool IsDoingDrag() const OVERRIDE;
  virtual void CancelDragAndCloseTab() OVERRIDE;
  virtual bool IsEventTracking() const OVERRIDE;
  virtual void CloseTabAfterEventTracking() OVERRIDE;
  virtual void GetViewBounds(gfx::Rect* out) const OVERRIDE;

  // Backend implementation of RenderViewHostDelegate::View.
  virtual void CreateNewWindow(
      int route_id,
      const ViewHostMsg_CreateWindow_Params& params);
  virtual void CreateNewWidget(int route_id, WebKit::WebPopupType popup_type);
  virtual void CreateNewFullscreenWidget(int route_id);
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture);
  virtual void ShowCreatedWidget(int route_id, const gfx::Rect& initial_pos);
  virtual void ShowCreatedFullscreenWidget(int route_id);
  virtual void ShowContextMenu(const ContextMenuParams& params);
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned);
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_ops,
                             const SkBitmap& image,
                             const gfx::Point& image_offset);
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation);
  virtual void GotFocus();
  virtual void TakeFocus(bool reverse);

  // NotificationObserver implementation ---------------------------------------

  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Insert the given widget into the content area. Should only be used for
  // web pages and the like (including interstitials and sad tab). Note that
  // this will be perfectly happy to insert overlapping render views, so care
  // should be taken that the correct one is hidden/shown.
  void InsertIntoContentArea(GtkWidget* widget);

  void CancelDragIfAny();

  // Handle focus traversal on the render widget native view.
  CHROMEGTK_CALLBACK_1(TabContentsViewGtk, gboolean, OnFocus, GtkDirectionType);

  // Used to adjust the size of its children when the size of |expanded_| is
  // changed.
  CHROMEGTK_CALLBACK_2(TabContentsViewGtk, void, OnChildSizeRequest,
                       GtkWidget*, GtkRequisition*);

  // Used to propagate the size change of |expanded_| to our RWHV to resize the
  // renderer content.
  CHROMEGTK_CALLBACK_1(TabContentsViewGtk, void, OnSizeAllocate,
                       GtkAllocation*);

  CHROMEGTK_CALLBACK_1(TabContentsViewGtk, void, OnSetFloatingPosition,
                       GtkAllocation*);

  // The TabContents whose contents we display.
  TabContents* tab_contents_;

  // Common implementations of some RenderViewHostDelegate::View methods.
  RenderViewHostDelegateViewHelper delegate_view_helper_;


  // Contains |expanded_| as its GtkBin member.
  ui::OwnedWidgetGtk floating_;

  // This container holds the tab's web page views. It is a GtkExpandedContainer
  // so that we can control the size of the web pages.
  GtkWidget* expanded_;

  // The context menu is reset every time we show it, but we keep a pointer to
  // between uses so that it won't go out of scope before we're done with it.
  scoped_ptr<RenderViewContextMenuGtk> context_menu_;

  // Used to get notifications about renderers coming and going.
  NotificationRegistrar registrar_;

  scoped_ptr<SadTabGtk> sad_tab_;

  FocusStoreGtk focus_store_;

  // The UI for the constrained dialog currently displayed. This is owned by
  // TabContents, not the view.
  ConstrainedWindowGtk* constrained_window_;

  // The helper object that handles drag destination related interactions with
  // GTK.
  scoped_ptr<content::WebDragDestGtk> drag_dest_;

  // The chrome specific delegate that receives events from WebDragDestGtk.
  scoped_ptr<WebDragBookmarkHandlerGtk> bookmark_handler_gtk_;

  // Object responsible for handling drags from the page for us.
  scoped_ptr<TabContentsDragSource> drag_source_;

  // The size we want the tab contents view to be.  We keep this in a separate
  // variable because resizing in GTK+ is async.
  gfx::Size requested_size_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsViewGtk);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_VIEW_GTK_H_
