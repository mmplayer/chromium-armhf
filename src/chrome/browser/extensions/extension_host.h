// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/perftimer.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "content/browser/javascript_dialogs.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/extensions/extension_view.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/extensions/extension_view_mac.h"
#elif defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/extensions/extension_view_gtk.h"
#endif

class Browser;
class Extension;
class FileSelectHelper;
class RenderProcessHost;
class RenderWidgetHostView;
class TabContents;
struct ViewHostMsg_RunFileChooser_Params;
struct WebPreferences;

// This class is the browser component of an extension component's RenderView.
// It handles setting up the renderer process, if needed, with special
// privileges available to extensions.  It may have a view to be shown in the
// browser UI, or it may be hidden.
class ExtensionHost : public RenderViewHostDelegate,
                      public RenderViewHostDelegate::View,
                      public ExtensionFunctionDispatcher::Delegate,
                      public NotificationObserver,
                      public content::JavaScriptDialogDelegate {
 public:
  class ProcessCreationQueue;

  // Enable DOM automation in created render view hosts.
  static void EnableDOMAutomation() { enable_dom_automation_ = true; }

  ExtensionHost(const Extension* extension, SiteInstance* site_instance,
                const GURL& url, content::ViewType host_type);
  virtual ~ExtensionHost();

#if defined(TOOLKIT_VIEWS)
  void set_view(ExtensionView* view) { view_.reset(view); }
  const ExtensionView* view() const { return view_.get(); }
  ExtensionView* view() { return view_.get(); }
#elif defined(OS_MACOSX)
  const ExtensionViewMac* view() const { return view_.get(); }
  ExtensionViewMac* view() { return view_.get(); }
#elif defined(TOOLKIT_GTK)
  const ExtensionViewGtk* view() const { return view_.get(); }
  ExtensionViewGtk* view() { return view_.get(); }
#endif

  // Create an ExtensionView and tie it to this host and |browser|.  Note NULL
  // is a valid argument for |browser|.  Extension views may be bound to
  // tab-contents hosted in ExternalTabContainer objects, which do not
  // instantiate Browser objects.
  void CreateView(Browser* browser);

  const Extension* extension() const { return extension_; }
  const std::string& extension_id() const { return extension_id_; }
  RenderViewHost* render_view_host() const { return render_view_host_; }
  RenderProcessHost* render_process_host() const;
  SiteInstance* site_instance() const;
  bool did_stop_loading() const { return did_stop_loading_; }
  bool document_element_available() const {
    return document_element_available_;
  }

  Profile* profile() const { return profile_; }

  content::ViewType extension_host_type() const {
    return extension_host_type_;
  }

  // ExtensionFunctionDispatcher::Delegate
  virtual TabContents* GetAssociatedTabContents() const OVERRIDE;
  void set_associated_tab_contents(TabContents* associated_tab_contents) {
    associated_tab_contents_ = associated_tab_contents;
  }

  // Returns true if the render view is initialized and didn't crash.
  bool IsRenderViewLive() const;

  // Prepares to initializes our RenderViewHost by creating its RenderView and
  // navigating to this host's url. Uses host_view for the RenderViewHost's view
  // (can be NULL). This happens delayed to avoid locking the UI.
  void CreateRenderViewSoon(RenderWidgetHostView* host_view);

  // Sets |url_| and navigates |render_view_host_|.
  void NavigateToURL(const GURL& url);

  // Insert a default style sheet for Extension Infobars.
  void InsertInfobarCSS();

  // Tell the renderer not to draw scrollbars on windows smaller than
  // |size_limit| in both width and height.
  void DisableScrollbarsForSmallWindows(const gfx::Size& size_limit);

  // RenderViewHostDelegate implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual const GURL& GetURL() const OVERRIDE;
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;
  virtual content::ViewType GetRenderViewType() const OVERRIDE;
  virtual void RenderViewGone(RenderViewHost* render_view_host,
                              base::TerminationStatus status,
                              int error_code) OVERRIDE;
  virtual void DidNavigate(
      RenderViewHost* render_view_host,
      const ViewHostMsg_FrameNavigate_Params& params) OVERRIDE;
  virtual void DidStopLoading() OVERRIDE;
  virtual void DocumentAvailableInMainFrame(
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void DocumentOnLoadCompletedInMainFrame(
      RenderViewHost* render_view_host,
      int32 page_id) OVERRIDE;
  virtual RenderViewHostDelegate::View* GetViewDelegate() OVERRIDE;
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
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event)
      OVERRIDE;
  virtual void HandleMouseMove() OVERRIDE;
  virtual void HandleMouseDown() OVERRIDE;
  virtual void HandleMouseLeave() OVERRIDE;
  virtual void HandleMouseUp() OVERRIDE;
  virtual void HandleMouseActivate() OVERRIDE;
  virtual void RunFileChooser(RenderViewHost* render_view_host,
                              const ViewHostMsg_RunFileChooser_Params& params);
  virtual void UpdatePreferredSize(const gfx::Size& new_size);

  // RenderViewHostDelegate::View
  virtual void CreateNewWindow(
      int route_id,
      const ViewHostMsg_CreateWindow_Params& params) OVERRIDE;
  virtual void CreateNewWidget(int route_id,
                               WebKit::WebPopupType popup_type) OVERRIDE;
  virtual void CreateNewFullscreenWidget(int route_id) OVERRIDE;
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) OVERRIDE;
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos) OVERRIDE;
  virtual void ShowCreatedFullscreenWidget(int route_id) OVERRIDE;
  virtual void ShowContextMenu(const ContextMenuParams& params) OVERRIDE;
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned) OVERRIDE;
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_operations,
                             const SkBitmap& image,
                             const gfx::Point& image_offset) OVERRIDE;
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) OVERRIDE;
  virtual void GotFocus() OVERRIDE;
  virtual void TakeFocus(bool reverse) OVERRIDE;

  // NotificationObserver
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Overridden from content::JavaScriptDialogDelegate:
  virtual void OnDialogClosed(IPC::Message* reply_msg,
                              bool success,
                              const string16& user_input) OVERRIDE;
  virtual gfx::NativeWindow GetDialogRootWindow() OVERRIDE;

 protected:
  // This should only be used by unit tests.
  ExtensionHost(const Extension* extension, content::ViewType host_type);

  // Internal functions used to support the CreateNewWidget() method. If a
  // platform requires plugging into widget creation at a lower level, then a
  // subclass might want to override these functions, but otherwise they should
  // be fine just implementing RenderWidgetHostView::InitAsPopup().
  //
  // The Create function returns the newly created widget so it can be
  // associated with the given route. When the widget needs to be shown later,
  // we'll look it up again and pass the object to the Show functions rather
  // than the route ID.
  virtual RenderWidgetHostView* CreateNewWidgetInternal(
      int route_id,
      WebKit::WebPopupType popup_type);
  virtual void ShowCreatedWidgetInternal(RenderWidgetHostView* widget_host_view,
                                         const gfx::Rect& initial_pos);
 private:
  friend class ProcessCreationQueue;

  // Whether to allow DOM automation for created RenderViewHosts. This is used
  // for testing.
  static bool enable_dom_automation_;

  // Actually create the RenderView for this host. See CreateRenderViewSoon.
  void CreateRenderViewNow();

  // Const version of below function.
  const Browser* GetBrowser() const;

  // ExtensionFunctionDispatcher::Delegate
  virtual Browser* GetBrowser() OVERRIDE;
  virtual gfx::NativeView GetNativeViewOfHost() OVERRIDE;

  // Message handlers.
  void OnRequest(const ExtensionHostMsg_Request_Params& params);

  // Handles keyboard events that were not handled by HandleKeyboardEvent().
  // Platform specific implementation may override this method to handle the
  // event in platform specific way.
  virtual void UnhandledKeyboardEvent(const NativeWebKeyboardEvent& event) {}

  // Returns true if we're hosting a background page.
  // This isn't valid until CreateRenderView is called.
  bool is_background_page() const { return !view(); }

  // The extension that we're hosting in this view.
  const Extension* extension_;

  // Id of extension that we're hosting in this view.
  const std::string extension_id_;

  // The profile that this host is tied to.
  Profile* profile_;

  // Optional view that shows the rendered content in the UI.
#if defined(TOOLKIT_VIEWS)
  scoped_ptr<ExtensionView> view_;
#elif defined(OS_MACOSX)
  scoped_ptr<ExtensionViewMac> view_;
#elif defined(TOOLKIT_GTK)
  scoped_ptr<ExtensionViewGtk> view_;
#endif

  // The host for our HTML content.
  RenderViewHost* render_view_host_;

  // Common implementations of some RenderViewHostDelegate::View methods.
  RenderViewHostDelegateViewHelper delegate_view_helper_;

  // Whether the RenderWidget has reported that it has stopped loading.
  bool did_stop_loading_;

  // True if the main frame has finished parsing.
  bool document_element_available_;

  // The URL being hosted.
  GURL url_;

  NotificationRegistrar registrar_;

  ExtensionFunctionDispatcher extension_function_dispatcher_;

  // Only EXTENSION_INFOBAR, EXTENSION_POPUP, and EXTENSION_BACKGROUND_PAGE
  // are used here, others are not hosted by ExtensionHost.
  content::ViewType extension_host_type_;

  // The relevant TabContents associated with this ExtensionHost, if any.
  TabContents* associated_tab_contents_;

  // Used to measure how long it's been since the host was created.
  PerfTimer since_created_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionHost);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_H_
