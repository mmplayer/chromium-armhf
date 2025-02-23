// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TAB_CONTENTS_RENDER_VIEW_HOST_MANAGER_H_
#define CONTENT_BROWSER_TAB_CONTENTS_RENDER_VIEW_HOST_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/common/content_export.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/browser/site_instance.h"

class InterstitialPage;
class NavigationController;
class NavigationEntry;
class RenderViewHost;
class RenderWidgetHostView;
class WebUI;

namespace content {
class BrowserContext;
}

// Manages RenderViewHosts for a TabContents. Normally there is only one and
// it is easy to do. But we can also have transitions of processes (and hence
// RenderViewHosts) that can get complex.
class CONTENT_EXPORT RenderViewHostManager
    : public RenderViewHostDelegate::RendererManagement,
      public NotificationObserver {
 public:
  // Functions implemented by our owner that we need.
  //
  // TODO(brettw) Clean this up! These are all the functions in TabContents that
  // are required to run this class. The design should probably be better such
  // that these are more clear.
  //
  // There is additional complexity that some of the functions we need in
  // TabContents are inherited and non-virtual. These are named with
  // "RenderManager" so that the duplicate implementation of them will be clear.
  class CONTENT_EXPORT Delegate {
   public:
    // See tab_contents.h's implementation for more.
    virtual bool CreateRenderViewForRenderManager(
        RenderViewHost* render_view_host) = 0;
    virtual void BeforeUnloadFiredFromRenderManager(
        bool proceed, bool* proceed_to_fire_unload) = 0;
    virtual void DidStartLoadingFromRenderManager(
        RenderViewHost* render_view_host) = 0;
    virtual void RenderViewGoneFromRenderManager(
        RenderViewHost* render_view_host) = 0;
    virtual void UpdateRenderViewSizeForRenderManager() = 0;
    virtual void NotifySwappedFromRenderManager() = 0;
    virtual NavigationController& GetControllerForRenderManager() = 0;

    // Creates a WebUI object for the given URL if one applies. Ownership of the
    // returned pointer will be passed to the caller. If no WebUI applies,
    // returns NULL.
    virtual WebUI* CreateWebUIForRenderManager(const GURL& url) = 0;

    // Returns the navigation entry of the current navigation, or NULL if there
    // is none.
    virtual NavigationEntry*
        GetLastCommittedNavigationEntryForRenderManager() = 0;

    // Returns true if the location bar should be focused by default rather than
    // the page contents.
    virtual bool FocusLocationBarByDefault() = 0;

    // Focuses the location bar.
    virtual void SetFocusToLocationBar(bool select_all) = 0;

    // Creates a view and sets the size for the specified RVH.
    virtual void CreateViewAndSetSizeForRVH(RenderViewHost* rvh) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Both delegate pointers must be non-NULL and are not owned by this class.
  // They must outlive this class. The RenderViewHostDelegate is what will be
  // installed into all RenderViewHosts that are created.
  //
  // You must call Init() before using this class.
  RenderViewHostManager(RenderViewHostDelegate* render_view_delegate,
                        Delegate* delegate);
  virtual ~RenderViewHostManager();

  // For arguments, see TabContents constructor.
  void Init(content::BrowserContext* browser_context,
            SiteInstance* site_instance,
            int routing_id);

  // Returns the currently active RenderViewHost.
  //
  // This will be non-NULL between Init() and Shutdown(). You may want to NULL
  // check it in many cases, however. Windows can send us messages during the
  // destruction process after it has been shut down.
  RenderViewHost* current_host() const {
    return render_view_host_;
  }

  // Returns the view associated with the current RenderViewHost, or NULL if
  // there is no current one.
  RenderWidgetHostView* GetRenderWidgetHostView() const;

  // Returns the pending render view host, or NULL if there is no pending one.
  RenderViewHost* pending_render_view_host() const {
    return pending_render_view_host_;
  }

  // Returns the current committed Web UI or NULL if none applies.
  WebUI* web_ui() const { return web_ui_.get(); }

  // Returns the Web UI for the pending navigation, or NULL of none applies.
  WebUI* pending_web_ui() const { return pending_web_ui_.get(); }

  // Called when we want to instruct the renderer to navigate to the given
  // navigation entry. It may create a new RenderViewHost or re-use an existing
  // one. The RenderViewHost to navigate will be returned. Returns NULL if one
  // could not be created.
  RenderViewHost* Navigate(const NavigationEntry& entry);

  // Instructs the various live views to stop. Called when the user directed the
  // page to stop loading.
  void Stop();

  // Notifies the regular and pending RenderViewHosts that a load is or is not
  // happening. Even though the message is only for one of them, we don't know
  // which one so we tell both.
  void SetIsLoading(bool is_loading);

  // Whether to close the tab or not when there is a hang during an unload
  // handler. If we are mid-crosssite navigation, then we should proceed
  // with the navigation instead of closing the tab.
  bool ShouldCloseTabOnUnresponsiveRenderer();

  // Called when a renderer's main frame navigates.
  void DidNavigateMainFrame(RenderViewHost* render_view_host);

  // Set the WebUI after committing a page load. This is useful for navigations
  // initiated from a renderer, where we want to give the new renderer WebUI
  // privileges from the originating renderer.
  void SetWebUIPostCommit(WebUI* web_ui);

  // Called when a provisional load on the given renderer is aborted.
  void RendererAbortedProvisionalLoad(RenderViewHost* render_view_host);

  // Sets the passed passed interstitial as the currently showing interstitial.
  // |interstitial_page| should be non NULL (use the remove_interstitial_page
  // method to unset the interstitial) and no interstitial page should be set
  // when there is already a non NULL interstitial page set.
  void set_interstitial_page(InterstitialPage* interstitial_page) {
    DCHECK(!interstitial_page_ && interstitial_page);
    interstitial_page_ = interstitial_page;
  }

  // Unsets the currently showing interstitial.
  void remove_interstitial_page() {
    DCHECK(interstitial_page_);
    interstitial_page_ = NULL;
  }

  // Returns the currently showing interstitial, NULL if no interstitial is
  // showing.
  InterstitialPage* interstitial_page() const {
    return interstitial_page_;
  }

  // RenderViewHostDelegate::RendererManagement implementation.
  virtual void ShouldClosePage(bool for_cross_site_transition, bool proceed);
  virtual void OnCrossSiteResponse(int new_render_process_host_id,
                                   int new_request_id);
  virtual void OnCrossSiteNavigationCanceled();

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Called when a RenderViewHost is about to be deleted.
  void RenderViewDeleted(RenderViewHost* rvh);

  // Returns whether the given RenderViewHost is on the list of swapped out
  // RenderViewHosts.
  bool IsSwappedOut(RenderViewHost* rvh);

 private:
  friend class TestTabContents;
  friend class RenderViewHostManagerTest;

  // Returns whether this tab should transition to a new renderer for
  // cross-site URLs.  Enabled unless we see the --process-per-tab command line
  // switch.  Can be overridden in unit tests.
  bool ShouldTransitionCrossSite();

  // Returns true if the two navigation entries are incompatible in some way
  // other than site instances. Cases where this can happen include Web UI
  // to regular web pages. It will cause us to swap RenderViewHosts (and hence
  // RenderProcessHosts) even if the site instance would otherwise be the same.
  // As part of this, we'll also force new SiteInstances and BrowsingInstances.
  // Either of the entries may be NULL.
  bool ShouldSwapProcessesForNavigation(
      const NavigationEntry* cur_entry,
      const NavigationEntry* new_entry) const;

  // Returns an appropriate SiteInstance object for the given NavigationEntry,
  // possibly reusing the current SiteInstance.
  // Never called if --process-per-tab is used.
  SiteInstance* GetSiteInstanceForEntry(const NavigationEntry& entry,
                                        SiteInstance* curr_instance);

  // Helper method to create a pending RenderViewHost for a cross-site
  // navigation.
  bool CreatePendingRenderView(const NavigationEntry& entry,
                               SiteInstance* instance);

  // Sets up the necessary state for a new RenderViewHost navigating to the
  // given entry.
  bool InitRenderView(RenderViewHost* render_view_host,
                      const NavigationEntry& entry);

  // Sets the pending RenderViewHost/WebUI to be the active one. Note that this
  // doesn't require the pending render_view_host_ pointer to be non-NULL, since
  // there could be Web UI switching as well. Call this for every commit.
  void CommitPending();

  // Helper method to terminate the pending RenderViewHost.
  void CancelPending();

  RenderViewHost* UpdateRendererStateForNavigate(const NavigationEntry& entry);

  // Called when a renderer process is starting to close.  We should not
  // schedule new navigations in its swapped out RenderViewHosts after this.
  void RendererProcessClosing(RenderProcessHost* render_process_host);

  // Our delegate, not owned by us. Guaranteed non-NULL.
  Delegate* delegate_;

  // Whether a navigation requiring different RenderView's is pending. This is
  // either cross-site request is (in the new process model), or when required
  // for the view type (like view source versus not).
  bool cross_navigation_pending_;

  // Implemented by the owner of this class, this delegate is installed into all
  // the RenderViewHosts that we create.
  RenderViewHostDelegate* render_view_delegate_;

  // Our RenderView host and its associated Web UI (if any, will be NULL for
  // non-DOM-UI pages). This object is responsible for all communication with
  // a child RenderView instance.
  RenderViewHost* render_view_host_;
  scoped_ptr<WebUI> web_ui_;

  // A RenderViewHost used to load a cross-site page. This remains hidden
  // while a cross-site request is pending until it calls DidNavigate. It may
  // have an associated Web UI, in which case the Web UI pointer will be non-
  // NULL.
  //
  // The pending_web_ui may be non-NULL even when the pending_render_view_host_
  // is. This will happen when we're transitioning between two Web UI pages:
  // the RVH won't be swapped, so the pending pointer will be unused, but there
  // will be a pending Web UI associated with the navigation.
  RenderViewHost* pending_render_view_host_;
  scoped_ptr<WebUI> pending_web_ui_;

  // A map of site instance ID to swapped out RenderViewHosts.
  typedef base::hash_map<int32, RenderViewHost*> RenderViewHostMap;
  RenderViewHostMap swapped_out_hosts_;

  // The intersitial page currently shown if any, not own by this class
  // (the InterstitialPage is self-owned, it deletes itself when hidden).
  InterstitialPage* interstitial_page_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostManager);
};

// The "details" for a NOTIFY_RENDER_VIEW_HOST_CHANGED notification. The old
// host can be NULL when the first RenderViewHost is set.
struct RenderViewHostSwitchedDetails {
  RenderViewHost* old_host;
  RenderViewHost* new_host;
};

#endif  // CONTENT_BROWSER_TAB_CONTENTS_RENDER_VIEW_HOST_MANAGER_H_
