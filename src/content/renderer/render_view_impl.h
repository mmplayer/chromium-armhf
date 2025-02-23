// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_VIEW_IMPL_H_
#define CONTENT_RENDERER_RENDER_VIEW_IMPL_H_
#pragma once

#include <deque>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer.h"
#include "build/build_config.h"
#include "content/renderer/render_view_selection.h"
#include "content/renderer/renderer_webcookiejar_impl.h"
#include "content/common/content_export.h"
#include "content/common/edit_command.h"
#include "content/common/navigation_gesture.h"
#include "content/common/page_zoom.h"
#include "content/common/renderer_preferences.h"
#include "content/public/renderer/render_view.h"
#include "content/renderer/pepper_plugin_delegate_impl.h"
#include "content/renderer/render_widget.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrameClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIconURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPageSerializerClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPageVisibilityState.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextDirection.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebViewClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNavigationType.h"
#include "ui/gfx/surface/transport_dib.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/plugins/npapi/webplugin_page_delegate.h"

#if defined(OS_WIN)
// RenderViewImpl is a diamond-shaped hierarchy, with WebWidgetClient at the
// root. VS warns when we inherit the WebWidgetClient method implementations
// from RenderWidget.  It's safe to ignore that warning.
#pragma warning(disable: 4250)
#endif

class AudioMessageFilter;
class DeviceOrientationDispatcher;
class DevToolsAgent;
class ExternalPopupMenu;
class GeolocationDispatcher;
class GURL;
class IntentsDispatcher;
class LoadProgressTracker;
class MediaStreamImpl;
class NotificationProvider;
class PepperDeviceTest;
class PrintWebViewHelper;
class RenderWidgetFullscreenPepper;
class RendererAccessibility;
class SkBitmap;
class SpeechInputDispatcher;
class WebPluginDelegateProxy;
class WebUIBindings;
struct ContextMenuMediaParams;
struct PP_Flash_NetAddress;
struct ViewHostMsg_RunFileChooser_Params;
struct ViewMsg_SwapOut_Params;
struct ViewMsg_Navigate_Params;
struct ViewMsg_StopFinding_Params;
struct WebDropData;

namespace base {
class WaitableEvent;
}  // namespace base

namespace content {
class RenderViewTest;
class NavigationState;
class P2PSocketDispatcher;
class RenderViewObserver;
class RenderViewVisitor;
}  // namespace content

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace webkit {

namespace ppapi {
class PluginInstance;
class PluginModule;
}  // namespace ppapi

}  // namespace webkit

namespace webkit_glue {
struct CustomContextMenuContext;
class ImageResourceFetcher;
struct FileUploadData;
struct FormData;
struct PasswordFormFillData;
class ResourceFetcher;
}

namespace WebKit {
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebDataSource;
class WebDocument;
class WebDragData;
class WebGeolocationClient;
class WebGeolocationServiceInterface;
class WebIconURL;
class WebImage;
class WebInputElement;
class WebKeyboardEvent;
class WebMediaPlayer;
class WebMediaPlayerClient;
class WebMouseEvent;
class WebSpeechInputController;
class WebSpeechInputListener;
class WebStorageNamespace;
class WebTouchEvent;
class WebURLLoader;
class WebURLRequest;
struct WebFileChooserParams;
struct WebFindOptions;
struct WebMediaPlayerAction;
struct WebPoint;
struct WebWindowFeatures;
}

// We need to prevent a page from trying to create infinite popups. It is not
// as simple as keeping a count of the number of immediate children
// popups. Having an html file that window.open()s itself would create
// an unlimited chain of RenderViews who only have one RenderView child.
//
// Therefore, each new top level RenderView creates a new counter and shares it
// with all its children and grandchildren popup RenderViewImpls created with
// createView() to have a sort of global limit for the page so no more than
// kMaximumNumberOfPopups popups are created.
//
// This is a RefCounted holder of an int because I can't say
// scoped_refptr<int>.
typedef base::RefCountedData<int> SharedRenderViewCounter;

//
// RenderView is an object that manages a WebView object, and provides a
// communication interface with an embedding application process
//
class RenderViewImpl : public RenderWidget,
                       public WebKit::WebViewClient,
                       public WebKit::WebFrameClient,
                       public WebKit::WebPageSerializerClient,
                       public content::RenderView,
                       public webkit::npapi::WebPluginPageDelegate,
                       public base::SupportsWeakPtr<RenderViewImpl> {
 public:
  // Creates a new RenderView.  The parent_hwnd specifies a HWND to use as the
  // parent of the WebView HWND that will be created.  If this is a blocked
  // popup or as a new tab, opener_id is the routing ID of the RenderView
  // responsible for creating this RenderView (corresponding to parent_hwnd).
  // |counter| is either a currently initialized counter, or NULL (in which case
  // we treat this RenderView as a top level window).
  CONTENT_EXPORT static RenderViewImpl* Create(
      gfx::NativeViewId parent_hwnd,
      int32 opener_id,
      const RendererPreferences& renderer_prefs,
      const WebPreferences& webkit_prefs,
      SharedRenderViewCounter* counter,
      int32 routing_id,
      int64 session_storage_namespace_id,
      const string16& frame_name);

  // Returns the RenderViewImpl containing the given WebView.
  CONTENT_EXPORT static RenderViewImpl* FromWebView(WebKit::WebView* webview);

  // Sets the "next page id" counter.
  static void SetNextPageID(int32 next_page_id);

  // May return NULL when the view is closing.
  CONTENT_EXPORT WebKit::WebView* webview() const;

  // Called by a GraphicsContext associated with this view when swapbuffers
  // is posted, completes or is aborted.
  void OnViewContextSwapBuffersPosted();
  void OnViewContextSwapBuffersComplete();
  void OnViewContextSwapBuffersAborted();

  int history_list_offset() const { return history_list_offset_; }

  const WebPreferences& webkit_preferences() const {
    return webkit_preferences_;
  }

  void set_send_content_state_immediately(bool value) {
    send_content_state_immediately_ = value;
  }

  // Current P2PSocketDispatcher. Set to NULL if P2P API is disabled.
  content::P2PSocketDispatcher* p2p_socket_dispatcher() {
    return p2p_socket_dispatcher_;
  }

  // Functions to add and remove observers for this object.
  void AddObserver(content::RenderViewObserver* observer);
  void RemoveObserver(content::RenderViewObserver* observer);

  // Adds the given file chooser request to the file_chooser_completion_ queue
  // (see that var for more) and requests the chooser be displayed if there are
  // no other waiting items in the queue.
  //
  // Returns true if the chooser was successfully scheduled. False means we
  // didn't schedule anything.
  bool ScheduleFileChooser(const ViewHostMsg_RunFileChooser_Params& params,
                           WebKit::WebFileChooserCompletion* completion);

  // Sets whether  the renderer should report load progress to the browser.
  void SetReportLoadProgressEnabled(bool enabled);

  void LoadNavigationErrorPage(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& failed_request,
      const WebKit::WebURLError& error,
      const std::string& html,
      bool replace);

  // Plugin-related functions --------------------------------------------------
  // (See also WebPluginPageDelegate implementation.)

  // Notification that the given plugin has crashed.
  void PluginCrashed(const FilePath& plugin_path);

  // Creates a fullscreen container for a pepper plugin instance.
  RenderWidgetFullscreenPepper* CreatePepperFullscreenContainer(
      webkit::ppapi::PluginInstance* plugin);

  // Informs the render view that a PPAPI plugin has gained or lost focus.
  void PpapiPluginFocusChanged();

  // Informs the render view that a PPAPI plugin has changed text input status.
  void PpapiPluginTextInputTypeChanged();
  void PpapiPluginCaretPositionChanged();

  // Cancels current composition.
  void PpapiPluginCancelComposition();

  // Request updated policy regarding firewall NAT traversal being enabled.
  void RequestRemoteAccessClientFirewallTraversal();

#if defined(OS_MACOSX) || defined(OS_WIN)
  // Informs the render view that the given plugin has gained or lost focus.
  void PluginFocusChanged(bool focused, int plugin_id);
#endif

#if defined(OS_MACOSX)
  // Starts plugin IME.
  void StartPluginIme();

  // Helper routines for accelerated plugin support. Used by the
  // WebPluginDelegateProxy, which has a pointer to the RenderView.
  gfx::PluginWindowHandle AllocateFakePluginWindowHandle(bool opaque,
                                                         bool root);
  void DestroyFakePluginWindowHandle(gfx::PluginWindowHandle window);
  void AcceleratedSurfaceSetIOSurface(gfx::PluginWindowHandle window,
                                      int32 width,
                                      int32 height,
                                      uint64 io_surface_identifier);
  TransportDIB::Handle AcceleratedSurfaceAllocTransportDIB(size_t size);
  void AcceleratedSurfaceFreeTransportDIB(TransportDIB::Id dib_id);
  void AcceleratedSurfaceSetTransportDIB(gfx::PluginWindowHandle window,
                                         int32 width,
                                         int32 height,
                                         TransportDIB::Handle transport_dib);
  void AcceleratedSurfaceBuffersSwapped(gfx::PluginWindowHandle window,
                                        uint64 surface_id);
#endif

  void RegisterPluginDelegate(WebPluginDelegateProxy* delegate);
  void UnregisterPluginDelegate(WebPluginDelegateProxy* delegate);

  // Helper function to retrieve information about a plugin for a URL and mime
  // type. Returns false if no plugin was found.
  // |actual_mime_type| is the actual mime type supported by the
  // plugin found that match the URL given (one for each item in
  // |info|).
  CONTENT_EXPORT bool GetPluginInfo(const GURL& url,
                                    const GURL& page_url,
                                    const std::string& mime_type,
                                    webkit::WebPluginInfo* plugin_info,
                                    std::string* actual_mime_type);

  // IPC::Channel::Listener implementation -------------------------------------

  virtual bool OnMessageReceived(const IPC::Message& msg);

  // WebKit::WebWidgetClient implementation ------------------------------------

  // Most methods are handled by RenderWidget.
  virtual void didFocus();
  virtual void didBlur();
  virtual void show(WebKit::WebNavigationPolicy policy);
  virtual void runModal();

  // WebKit::WebViewClient implementation --------------------------------------

  virtual WebKit::WebView* createView(
      WebKit::WebFrame* creator,
      const WebKit::WebURLRequest& request,
      const WebKit::WebWindowFeatures& features,
      const WebKit::WebString& frame_name);
  virtual WebKit::WebWidget* createPopupMenu(WebKit::WebPopupType popup_type);
  virtual WebKit::WebWidget* createPopupMenu(
      const WebKit::WebPopupMenuInfo& info);
  virtual WebKit::WebExternalPopupMenu* createExternalPopupMenu(
      const WebKit::WebPopupMenuInfo& popup_menu_info,
      WebKit::WebExternalPopupMenuClient* popup_menu_client);
  virtual WebKit::WebStorageNamespace* createSessionStorageNamespace(
      unsigned quota);
  virtual void didAddMessageToConsole(
      const WebKit::WebConsoleMessage& message,
      const WebKit::WebString& source_name,
      unsigned source_line);
  virtual void printPage(WebKit::WebFrame* frame);
  virtual WebKit::WebNotificationPresenter* notificationPresenter();
  virtual bool enumerateChosenDirectory(
      const WebKit::WebString& path,
      WebKit::WebFileChooserCompletion* chooser_completion);
  virtual void didStartLoading();
  virtual void didStopLoading();
  virtual void didChangeLoadProgress(WebKit::WebFrame* frame,
                                     double load_progress);
  virtual bool isSmartInsertDeleteEnabled();
  virtual bool isSelectTrailingWhitespaceEnabled();
  virtual void didChangeSelection(bool is_selection_empty);
  virtual void didExecuteCommand(const WebKit::WebString& command_name);
  virtual bool handleCurrentKeyboardEvent();
  virtual bool runFileChooser(
      const WebKit::WebFileChooserParams& params,
      WebKit::WebFileChooserCompletion* chooser_completion);
  virtual void runModalAlertDialog(WebKit::WebFrame* frame,
                                   const WebKit::WebString& message);
  virtual bool runModalConfirmDialog(WebKit::WebFrame* frame,
                                     const WebKit::WebString& message);
  virtual bool runModalPromptDialog(WebKit::WebFrame* frame,
                                    const WebKit::WebString& message,
                                    const WebKit::WebString& default_value,
                                    WebKit::WebString* actual_value);
  virtual bool runModalBeforeUnloadDialog(WebKit::WebFrame* frame,
                                          const WebKit::WebString& message);
  virtual void showContextMenu(WebKit::WebFrame* frame,
                               const WebKit::WebContextMenuData& data);
  virtual bool supportsFullscreen();
  virtual void enterFullscreenForNode(const WebKit::WebNode&);
  virtual void exitFullscreenForNode(const WebKit::WebNode&);
  virtual void enterFullscreen() OVERRIDE;
  virtual void exitFullscreen() OVERRIDE;
  virtual void setStatusText(const WebKit::WebString& text);
  virtual void setMouseOverURL(const WebKit::WebURL& url);
  virtual void setKeyboardFocusURL(const WebKit::WebURL& url);
  virtual void startDragging(const WebKit::WebDragData& data,
                             WebKit::WebDragOperationsMask mask,
                             const WebKit::WebImage& image,
                             const WebKit::WebPoint& imageOffset);
  virtual bool acceptsLoadDrops();
  virtual void focusNext();
  virtual void focusPrevious();
  virtual void focusedNodeChanged(const WebKit::WebNode& node);
  virtual void navigateBackForwardSoon(int offset);
  virtual int historyBackListCount();
  virtual int historyForwardListCount();
  virtual void postAccessibilityNotification(
      const WebKit::WebAccessibilityObject& obj,
      WebKit::WebAccessibilityNotification notification);
  virtual void didUpdateInspectorSetting(const WebKit::WebString& key,
                                         const WebKit::WebString& value);
  virtual WebKit::WebGeolocationClient* geolocationClient();
  virtual WebKit::WebSpeechInputController* speechInputController(
      WebKit::WebSpeechInputListener* listener);
  virtual WebKit::WebDeviceOrientationClient* deviceOrientationClient();
  virtual void zoomLimitsChanged(double minimum_level, double maximum_level);
  virtual void zoomLevelChanged();
  virtual void registerProtocolHandler(const WebKit::WebString& scheme,
                                       const WebKit::WebString& base_url,
                                       const WebKit::WebString& url,
                                       const WebKit::WebString& title);
  virtual void registerIntentHandler(const WebKit::WebString& action,
                                     const WebKit::WebString& type,
                                     const WebKit::WebString& href,
                                     const WebKit::WebString& title);
  virtual WebKit::WebPageVisibilityState visibilityState() const;
  virtual void startActivity(const WebKit::WebString& action,
                             const WebKit::WebString& type,
                             const WebKit::WebString& data,
                             int intent_id);

  // WebKit::WebFrameClient implementation -------------------------------------

  virtual WebKit::WebPlugin* createPlugin(
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params);
  virtual WebKit::WebWorker* createWorker(WebKit::WebFrame* frame,
                                          WebKit::WebWorkerClient* client);
  virtual WebKit::WebSharedWorker* createSharedWorker(
      WebKit::WebFrame* frame, const WebKit::WebURL& url,
      const WebKit::WebString& name, unsigned long long documentId);
  virtual WebKit::WebMediaPlayer* createMediaPlayer(
      WebKit::WebFrame* frame,
      WebKit::WebMediaPlayerClient* client);
  virtual WebKit::WebApplicationCacheHost* createApplicationCacheHost(
      WebKit::WebFrame* frame,
      WebKit::WebApplicationCacheHostClient* client);
  virtual WebKit::WebCookieJar* cookieJar(WebKit::WebFrame* frame);
  virtual void frameDetached(WebKit::WebFrame* frame);
  virtual void willClose(WebKit::WebFrame* frame);
  virtual void loadURLExternally(WebKit::WebFrame* frame,
                                 const WebKit::WebURLRequest& request,
                                 WebKit::WebNavigationPolicy policy);
  virtual void loadURLExternally(WebKit::WebFrame* frame,
                                 const WebKit::WebURLRequest& request,
                                 WebKit::WebNavigationPolicy policy,
                                 const WebKit::WebString& suggested_name);
  virtual WebKit::WebNavigationPolicy decidePolicyForNavigation(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request,
      WebKit::WebNavigationType type,
      const WebKit::WebNode&,
      WebKit::WebNavigationPolicy default_policy,
      bool is_redirect);
  virtual bool canHandleRequest(WebKit::WebFrame* frame,
                                const WebKit::WebURLRequest& request);
  virtual WebKit::WebURLError cannotHandleRequestError(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request);
  virtual WebKit::WebURLError cancelledError(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request);
  virtual void unableToImplementPolicyWithError(
      WebKit::WebFrame* frame,
      const WebKit::WebURLError& error);
  virtual void willSendSubmitEvent(WebKit::WebFrame* frame,
                                   const WebKit::WebFormElement& form);
  virtual void willSubmitForm(WebKit::WebFrame* frame,
                              const WebKit::WebFormElement& form);
  virtual void willPerformClientRedirect(WebKit::WebFrame* frame,
                                         const WebKit::WebURL& from,
                                         const WebKit::WebURL& to,
                                         double interval,
                                         double fire_time);
  virtual void didCancelClientRedirect(WebKit::WebFrame* frame);
  virtual void didCompleteClientRedirect(WebKit::WebFrame* frame,
                                         const WebKit::WebURL& from);
  virtual void didCreateDataSource(WebKit::WebFrame* frame,
                                   WebKit::WebDataSource* datasource);
  virtual void didStartProvisionalLoad(WebKit::WebFrame* frame);
  virtual void didReceiveServerRedirectForProvisionalLoad(
      WebKit::WebFrame* frame);
  virtual void didFailProvisionalLoad(WebKit::WebFrame* frame,
                                      const WebKit::WebURLError& error);
  virtual void didReceiveDocumentData(WebKit::WebFrame* frame,
                                      const char* data, size_t length,
                                      bool& prevent_default);
  virtual void didCommitProvisionalLoad(WebKit::WebFrame* frame,
                                        bool is_new_navigation);
  virtual void didClearWindowObject(WebKit::WebFrame* frame);
  virtual void didCreateDocumentElement(WebKit::WebFrame* frame);
  virtual void didReceiveTitle(WebKit::WebFrame* frame,
                               const WebKit::WebString& title,
                               WebKit::WebTextDirection direction);
  virtual void didChangeIcon(WebKit::WebFrame*,
                             WebKit::WebIconURL::Type) OVERRIDE;
  virtual void didFinishDocumentLoad(WebKit::WebFrame* frame);
  virtual void didHandleOnloadEvents(WebKit::WebFrame* frame);
  virtual void didFailLoad(WebKit::WebFrame* frame,
                           const WebKit::WebURLError& error);
  virtual void didFinishLoad(WebKit::WebFrame* frame);
  virtual void didNavigateWithinPage(WebKit::WebFrame* frame,
                                     bool is_new_navigation);
  virtual void didUpdateCurrentHistoryItem(WebKit::WebFrame* frame);
  virtual void assignIdentifierToRequest(WebKit::WebFrame* frame,
                                         unsigned identifier,
                                         const WebKit::WebURLRequest& request);
  virtual void willSendRequest(WebKit::WebFrame* frame,
                               unsigned identifier,
                               WebKit::WebURLRequest& request,
                               const WebKit::WebURLResponse& redirect_response);
  virtual void didReceiveResponse(WebKit::WebFrame* frame,
                                  unsigned identifier,
                                  const WebKit::WebURLResponse& response);
  virtual void didFinishResourceLoad(WebKit::WebFrame* frame,
                                     unsigned identifier);
  virtual void didFailResourceLoad(WebKit::WebFrame* frame,
                                   unsigned identifier,
                                   const WebKit::WebURLError& error);
  virtual void didLoadResourceFromMemoryCache(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request,
      const WebKit::WebURLResponse&);
  virtual void didDisplayInsecureContent(WebKit::WebFrame* frame);
  virtual void didRunInsecureContent(
      WebKit::WebFrame* frame,
      const WebKit::WebSecurityOrigin& origin,
      const WebKit::WebURL& target);
  virtual void didAdoptURLLoader(WebKit::WebURLLoader* loader);
  virtual void didExhaustMemoryAvailableForScript(WebKit::WebFrame* frame);
  virtual void didCreateScriptContext(WebKit::WebFrame* frame,
                                      v8::Handle<v8::Context>,
                                      int world_id);
  virtual void willReleaseScriptContext(WebKit::WebFrame* frame,
                                        v8::Handle<v8::Context>,
                                        int world_id);
  virtual void didUpdateLayout(WebKit::WebFrame* frame);
  virtual void didChangeScrollOffset(WebKit::WebFrame* frame);
  virtual void numberOfWheelEventHandlersChanged(unsigned num_handlers);
  virtual void didChangeContentsSize(WebKit::WebFrame* frame,
                                     const WebKit::WebSize& size);
  virtual void reportFindInPageMatchCount(int request_id,
                                          int count,
                                          bool final_update);
  virtual void reportFindInPageSelection(int request_id,
                                         int active_match_ordinal,
                                         const WebKit::WebRect& sel);

  virtual void openFileSystem(WebKit::WebFrame* frame,
                              WebKit::WebFileSystem::Type type,
                              long long size,
                              bool create,
                              WebKit::WebFileSystemCallbacks* callbacks);

  virtual void queryStorageUsageAndQuota(
      WebKit::WebFrame* frame,
      WebKit::WebStorageQuotaType type,
      WebKit::WebStorageQuotaCallbacks* callbacks);

  virtual void requestStorageQuota(
      WebKit::WebFrame* frame,
      WebKit::WebStorageQuotaType type,
      unsigned long long requested_size,
      WebKit::WebStorageQuotaCallbacks* callbacks);

  // WebKit::WebPageSerializerClient implementation ----------------------------

  virtual void didSerializeDataForFrame(
      const WebKit::WebURL& frame_url,
      const WebKit::WebCString& data,
      PageSerializationStatus status) OVERRIDE;

  // content::RenderView implementation ----------------------------------------

  virtual bool Send(IPC::Message* message) OVERRIDE;
  virtual int GetRoutingId() const OVERRIDE;
  virtual int GetPageId() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual gfx::NativeViewId GetHostWindow() OVERRIDE;
  virtual WebPreferences& GetWebkitPreferences() OVERRIDE;
  virtual WebKit::WebView* GetWebView() OVERRIDE;
  virtual WebKit::WebNode GetFocusedNode() const OVERRIDE;
  virtual WebKit::WebNode GetContextMenuNode() const OVERRIDE;
  virtual bool IsEditableNode(const WebKit::WebNode& node) OVERRIDE;
  virtual WebKit::WebPlugin* CreatePlugin(
      WebKit::WebFrame* frame,
      const webkit::WebPluginInfo& info,
      const WebKit::WebPluginParams& params) OVERRIDE;
  virtual void EvaluateScript(const string16& frame_xpath,
                              const string16& jscript,
                              int id,
                              bool notify_result) OVERRIDE;
  virtual bool ShouldDisplayScrollbars(int width, int height) const OVERRIDE;
  virtual int GetEnabledBindings() OVERRIDE;
  virtual void SetEnabledBindings(int enabled_bindings) OVERRIDE;
  virtual bool GetContentStateImmediately() OVERRIDE;
  virtual float GetFilteredTimePerFrame() OVERRIDE;
  virtual void ShowContextMenu(WebKit::WebFrame* frame,
                               const WebKit::WebContextMenuData& data) OVERRIDE;
  virtual WebKit::WebPageVisibilityState GetVisibilityState() const OVERRIDE;
  virtual void RunModalAlertDialog(WebKit::WebFrame* frame,
                                   const WebKit::WebString& message) OVERRIDE;
  virtual void LoadURLExternally(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request,
      WebKit::WebNavigationPolicy policy) OVERRIDE;

  // webkit_glue::WebPluginPageDelegate implementation -------------------------

  virtual webkit::npapi::WebPluginDelegate* CreatePluginDelegate(
      const FilePath& file_path,
      const std::string& mime_type);
  virtual void CreatedPluginWindow(gfx::PluginWindowHandle handle);
  virtual void WillDestroyPluginWindow(gfx::PluginWindowHandle handle);
  virtual void DidMovePlugin(const webkit::npapi::WebPluginGeometry& move);
  virtual void DidStartLoadingForPlugin();
  virtual void DidStopLoadingForPlugin();
  virtual WebKit::WebCookieJar* GetCookieJar();

  // Please do not add your stuff randomly to the end here. If there is an
  // appropriate section, add it there. If not, there are some random functions
  // nearer to the top you can add it to.
  virtual void DidFlushPaint();

  // Cannot use std::set unfortunately since linked_ptr<> does not support
  // operator<.
  typedef std::vector<linked_ptr<webkit_glue::ImageResourceFetcher> >
      ImageResourceFetcherList;

 protected:
  // RenderWidget overrides:
  virtual void Close();
  virtual void OnResize(const gfx::Size& new_size,
                        const gfx::Rect& resizer_rect,
                        bool is_fullscreen);
  virtual void DidInitiatePaint();
  virtual webkit::ppapi::PluginInstance* GetBitmapForOptimizedPluginPaint(
      const gfx::Rect& paint_bounds,
      TransportDIB** dib,
      gfx::Rect* location,
      gfx::Rect* clip);
  virtual gfx::Point GetScrollOffset();
  virtual void DidHandleKeyEvent();
  virtual bool WillHandleMouseEvent(
      const WebKit::WebMouseEvent& event) OVERRIDE;
  virtual void DidHandleMouseEvent(const WebKit::WebMouseEvent& event);
  virtual void DidHandleTouchEvent(const WebKit::WebTouchEvent& event);
  virtual void OnSetFocus(bool enable);
  virtual void OnWasHidden();
  virtual void OnWasRestored(bool needs_repainting);
  virtual bool SupportsAsynchronousSwapBuffers() OVERRIDE;
  virtual void OnImeSetComposition(
      const string16& text,
      const std::vector<WebKit::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end) OVERRIDE;
  virtual void OnImeConfirmComposition(
      const string16& text, const ui::Range& replacement_range) OVERRIDE;
  virtual ui::TextInputType GetTextInputType() OVERRIDE;
  virtual void GetSelectionBounds(gfx::Rect* start, gfx::Rect* end) OVERRIDE;
  virtual bool CanComposeInline() OVERRIDE;
  virtual bool WebWidgetHandlesCompositorScheduling() const OVERRIDE;

 private:
  // For unit tests.
  friend class ExternalPopupMenuTest;
  friend class PepperDeviceTest;
  friend class content::RenderViewTest;

  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuRemoveTest, RemoveOnChange);
  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuTest, NormalCase);
  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuTest, ShowPopupThenNavigate);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           DontIgnoreBackAfterNavEntryLimit);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, ImeComposition);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, InsertCharacters);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, JSBlockSentAfterPageLoad);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, LastCommittedUpdateState);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnHandleKeyboardEvent);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnImeStateChanged);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnNavStateChanged);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnSetTextDirection);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnUpdateWebPreferences);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, StaleNavigationsIgnored);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, UpdateTargetURLWithInvalidURL);
#if defined(OS_MACOSX)
  FRIEND_TEST_ALL_PREFIXES(RenderViewTest, MacTestCmdUp);
#endif
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, SetHistoryLengthAndPrune);

  typedef std::map<GURL, double> HostZoomLevels;

  enum ErrorPageType {
    DNS_ERROR,
    HTTP_404,
    CONNECTION_ERROR,
  };

  RenderViewImpl(gfx::NativeViewId parent_hwnd,
                 int32 opener_id,
                 const RendererPreferences& renderer_prefs,
                 const WebPreferences& webkit_prefs,
                 SharedRenderViewCounter* counter,
                 int32 routing_id,
                 int64 session_storage_namespace_id,
                 const string16& frame_name);

  // Do not delete directly.  This class is reference counted.
  virtual ~RenderViewImpl();

  void UpdateURL(WebKit::WebFrame* frame);
  void UpdateTitle(WebKit::WebFrame* frame, const string16& title,
                   WebKit::WebTextDirection title_direction);
  void UpdateSessionHistory(WebKit::WebFrame* frame);

  // Update current main frame's encoding and send it to browser window.
  // Since we want to let users see the right encoding info from menu
  // before finishing loading, we call the UpdateEncoding in
  // a) function:DidCommitLoadForFrame. When this function is called,
  // that means we have got first data. In here we try to get encoding
  // of page if it has been specified in http header.
  // b) function:DidReceiveTitle. When this function is called,
  // that means we have got specified title. Because in most of webpages,
  // title tags will follow meta tags. In here we try to get encoding of
  // page if it has been specified in meta tag.
  // c) function:DidFinishDocumentLoadForFrame. When this function is
  // called, that means we have got whole html page. In here we should
  // finally get right encoding of page.
  void UpdateEncoding(WebKit::WebFrame* frame,
                      const std::string& encoding_name);

  void OpenURL(WebKit::WebFrame* frame,
               const GURL& url,
               const GURL& referrer,
               WebKit::WebNavigationPolicy policy);

  bool RunJavaScriptMessage(int type,
                            const string16& message,
                            const string16& default_value,
                            const GURL& frame_url,
                            string16* result);

  // Sends a message and runs a nested message loop.
  bool SendAndRunNestedMessageLoop(IPC::SyncMessage* message);

  // Called when the "pinned to left/right edge" state needs to be updated.
  void UpdateScrollState(WebKit::WebFrame* frame);

  // IPC message handlers ------------------------------------------------------
  //
  // The documentation for these functions should be in
  // render_messages_internal.h for the message that the function is handling.

  void OnAllowBindings(int enabled_bindings_flags);
  void OnAllowScriptToClose(bool script_can_close);
  void OnAsyncFileOpened(base::PlatformFileError error_code,
                         IPC::PlatformFileForTransit file_for_transit,
                         int message_id);
  void OnPpapiBrokerChannelCreated(int request_id,
                                   base::ProcessHandle broker_process_handle,
                                   const IPC::ChannelHandle& handle);
  void OnCancelDownload(int32 download_id);
  void OnClearFocusedNode();
  void OnClosePage();
#if defined(ENABLE_FLAPPER_HACKS)
  void OnConnectTcpACK(int request_id,
                       IPC::PlatformFileForTransit socket_for_transit,
                       const PP_Flash_NetAddress& local_addr,
                       const PP_Flash_NetAddress& remote_addr);
#endif
  void OnContextMenuClosed(
      const webkit_glue::CustomContextMenuContext& custom_context);
  void OnCopy();
  void OnCopyImageAt(int x, int y);
#if defined(OS_MACOSX)
  void OnCopyToFindPboard();
#endif
  void OnCut();
  void OnCSSInsertRequest(const string16& frame_xpath,
                          const std::string& css);
  void OnCustomContextMenuAction(
      const webkit_glue::CustomContextMenuContext& custom_context,
      unsigned action);
  void OnDelete();
  void OnDeterminePageLanguage();
  void OnDisableScrollbarsForSmallWindows(
      const gfx::Size& disable_scrollbars_size_limit);
  void OnDisassociateFromPopupCount();
  void OnDragSourceEndedOrMoved(const gfx::Point& client_point,
                                const gfx::Point& screen_point,
                                bool ended,
                                WebKit::WebDragOperation drag_operation);
  void OnDragSourceSystemDragEnded();
  void OnDragTargetDrop(const gfx::Point& client_pt,
                        const gfx::Point& screen_pt);
  void OnDragTargetDragEnter(const WebDropData& drop_data,
                             const gfx::Point& client_pt,
                             const gfx::Point& screen_pt,
                             WebKit::WebDragOperationsMask operations_allowed);
  void OnDragTargetDragLeave();
  void OnDragTargetDragOver(const gfx::Point& client_pt,
                            const gfx::Point& screen_pt,
                            WebKit::WebDragOperationsMask operations_allowed);
  void OnEnablePreferredSizeChangedMode(int flags);
  void OnEnumerateDirectoryResponse(int id, const std::vector<FilePath>& paths);
  void OnExecuteEditCommand(const std::string& name, const std::string& value);
  void OnFileChooserResponse(const std::vector<FilePath>& paths);
  void OnFind(int request_id, const string16&, const WebKit::WebFindOptions&);
  void OnFindReplyAck();
  void OnGetAllSavableResourceLinksForCurrentPage(const GURL& page_url);
  void OnGetSerializedHtmlDataForCurrentPageWithLocalLinks(
      const std::vector<GURL>& links,
      const std::vector<FilePath>& local_paths,
      const FilePath& local_directory_name);
  void OnLockMouseACK(bool succeeded);
  void OnMediaPlayerActionAt(const gfx::Point& location,
                             const WebKit::WebMediaPlayerAction& action);
  void OnMouseLockLost();
  void OnMoveOrResizeStarted();
  CONTENT_EXPORT void OnNavigate(const ViewMsg_Navigate_Params& params);
  void OnPaste();
  void OnPasteAndMatchStyle();
#if defined(OS_MACOSX)
  void OnPluginImeCompositionCompleted(const string16& text, int plugin_id);
#endif
  void OnRedo();
  void OnReloadFrame();
  void OnReplace(const string16& text);
  void OnReservePageIDRange(int size_of_range);
  void OnResetPageEncodingToDefault();
  void OnScriptEvalRequest(const string16& frame_xpath,
                           const string16& jscript,
                           int id,
                           bool notify_result);
  void OnSelectAll();
  void OnSelectRange(const gfx::Point& start, const gfx::Point& end);
  void OnSetActive(bool active);
  void OnSetAltErrorPageURL(const GURL& gurl);
  void OnSetBackground(const SkBitmap& background);
  void OnSetWebUIProperty(const std::string& name, const std::string& value);
  void OnSetEditCommandsForNextKeyEvent(const EditCommands& edit_commands);
  CONTENT_EXPORT void OnSetHistoryLengthAndPrune(int history_length,
                                                 int32 minimum_page_id);
  void OnSetInitialFocus(bool reverse);
#if defined(OS_MACOSX)
  void OnSetInLiveResize(bool in_live_resize);
#endif
  void OnScrollFocusedEditableNodeIntoRect(const gfx::Rect& rect);
  void OnSetPageEncoding(const std::string& encoding_name);
  void OnSetRendererPrefs(const RendererPreferences& renderer_prefs);
#if defined(OS_MACOSX)
  void OnSetWindowVisibility(bool visible);
#endif
  void OnSetZoomLevel(double zoom_level);
  void OnSetZoomLevelForLoadingURL(const GURL& url, double zoom_level);
  void OnExitFullscreen();
  void OnShouldClose();
  void OnStop();
  void OnStopFinding(const ViewMsg_StopFinding_Params& params);
  void OnSwapOut(const ViewMsg_SwapOut_Params& params);
  void OnThemeChanged();
  void OnUndo();
  void OnUpdateTargetURLAck();
  void OnUpdateWebPreferences(const WebPreferences& prefs);
  void OnUpdateRemoteAccessClientFirewallTraversal(const std::string& policy);

#if defined(OS_MACOSX)
  void OnWindowFrameChanged(const gfx::Rect& window_frame,
                            const gfx::Rect& view_frame);
  void OnSelectPopupMenuItem(int selected_index);
#endif
  void OnZoom(PageZoom::Function function);
  void OnEnableViewSourceMode();

  // Adding a new message handler? Please add it in alphabetical order above
  // and put it in the same position in the .cc file.

  // Misc private functions ----------------------------------------------------

  void AltErrorPageFinished(WebKit::WebFrame* frame,
                            const WebKit::WebURLError& original_error,
                            const std::string& html);

  // Check whether the preferred size has changed.
  void CheckPreferredSize();

  // This callback is triggered when DownloadFavicon completes, either
  // succesfully or with a failure. See DownloadFavicon for more
  // details.
  void DidDownloadFavicon(webkit_glue::ImageResourceFetcher* fetcher,
                          const SkBitmap& image);

  // Requests to download a favicon image. When done, the RenderView is notified
  // by way of DidDownloadFavicon. Returns true if the request was successfully
  // started, false otherwise. id is used to uniquely identify the request and
  // passed back to the DidDownloadFavicon method. If the image has multiple
  // frames, the frame whose size is image_size is returned. If the image
  // doesn't have a frame at the specified size, the first is returned.
  bool DownloadFavicon(int id, const GURL& image_url, int image_size);

  GURL GetAlternateErrorPageURL(const GURL& failed_url,
                                ErrorPageType error_type);

  // Locates a sub frame with given xpath
  WebKit::WebFrame* GetChildFrame(const string16& frame_xpath) const;

  // Returns the opener url if present, else an empty url.
  GURL GetOpenerUrl() const;

  WebUIBindings* GetWebUIBindings();

  // Should only be called if this object wraps a PluginDocument.
  WebKit::WebPlugin* GetWebPluginFromPluginDocument();

  // Returns true if the |params| navigation is to an entry that has been
  // cropped due to a recent navigation the browser did not know about.
  bool IsBackForwardToStaleEntry(const ViewMsg_Navigate_Params& params,
                                 bool is_reload);

  // Returns false unless this is a top-level navigation that crosses origins.
  bool IsNonLocalTopLevelNavigation(const GURL& url,
                                    WebKit::WebFrame* frame,
                                    WebKit::WebNavigationType type);

  bool MaybeLoadAlternateErrorPage(WebKit::WebFrame* frame,
                                   const WebKit::WebURLError& error,
                                   bool replace);

  // Starts nav_state_sync_timer_ if it isn't already running.
  void StartNavStateSyncTimerIfNecessary();

  // Dispatches the current navigation state to the browser. Called on a
  // periodic timer so we don't send too many messages.
  void SyncNavigationState();

  // Dispatches the current state of selection on the webpage to the browser if
  // it has changed.
  // TODO(varunjain): delete this method once we figure out how to keep
  // selection handles in sync with the webpage.
  void SyncSelectionIfRequired();

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  void UpdateFontRenderingFromRendererPrefs();
#else
  void UpdateFontRenderingFromRendererPrefs() {}
#endif

  // Update the target url and tell the browser that the target URL has changed.
  // If |url| is empty, show |fallback_url|.
  void UpdateTargetURL(const GURL& url, const GURL& fallback_url);

  // ---------------------------------------------------------------------------
  // ADDING NEW FUNCTIONS? Please keep private functions alphabetized and put
  // it in the same order in the .cc file as it was in the header.
  // ---------------------------------------------------------------------------

  // Settings ------------------------------------------------------------------

  WebPreferences webkit_preferences_;
  RendererPreferences renderer_preferences_;

  HostZoomLevels host_zoom_levels_;

  // Whether content state (such as form state, scroll position and page
  // contents) should be sent to the browser immediately. This is normally
  // false, but set to true by some tests.
  bool send_content_state_immediately_;

  // Bitwise-ORed set of extra bindings that have been enabled.  See
  // BindingsPolicy for details.
  int enabled_bindings_;

  // The alternate error page URL, if one exists.
  GURL alternate_error_page_url_;

  // If true, we send IPC messages when |preferred_size_| changes.
  bool send_preferred_size_changes_;

  // If non-empty, and |send_preferred_size_changes_| is true, disable drawing
  // scroll bars on windows smaller than this size.  Used for windows that the
  // browser resizes to the size of the content, such as browser action popups.
  // If a render view is set to the minimum size of its content, webkit may add
  // scroll bars.  This makes sense for fixed sized windows, but it does not
  // make sense when the size of the view was chosen to fit the content.
  // This setting ensures that no scroll bars are drawn.  The size limit exists
  // because if the view grows beyond a size known to the browser, scroll bars
  // should be drawn.
  gfx::Size disable_scrollbars_size_limit_;

  // Loading state -------------------------------------------------------------

  // True if the top level frame is currently being loaded.
  bool is_loading_;

  // The gesture that initiated the current navigation.
  NavigationGesture navigation_gesture_;

  // Used for popups.
  bool opened_by_user_gesture_;
  GURL creator_url_;

  // Whether this RenderView was created by a frame that was suppressing its
  // opener. If so, we may want to load pages in a separate process.  See
  // decidePolicyForNavigation for details.
  bool opener_suppressed_;

  // If we are handling a top-level client-side redirect, this tracks the URL
  // of the page that initiated it. Specifically, when a load is committed this
  // is used to determine if that load originated from a client-side redirect.
  // It is empty if there is no top-level client-side redirect.
  GURL completed_client_redirect_src_;

  // Holds state pertaining to a navigation that we initiated.  This is held by
  // the WebDataSource::ExtraData attribute.  We use pending_navigation_state_
  // as a temporary holder for the state until the WebDataSource corresponding
  // to the new navigation is created.  See DidCreateDataSource.
  scoped_ptr<content::NavigationState> pending_navigation_state_;

  // Timer used to delay the updating of nav state (see SyncNavigationState).
  base::OneShotTimer<RenderViewImpl> nav_state_sync_timer_;

  // Page IDs ------------------------------------------------------------------
  // See documentation in content::RenderView.
  int32 page_id_;

  // Indicates the ID of the last page that we sent a FrameNavigate to the
  // browser for. This is used to determine if the most recent transition
  // generated a history entry (less than page_id_), or not (equal to or
  // greater than). Note that this will be greater than page_id_ if the user
  // goes back.
  int32 last_page_id_sent_to_browser_;

  // The next available page ID to use. This ensures that the page IDs are
  // globally unique in the renderer.
  static int32 next_page_id_;

  // The offset of the current item in the history list.
  int history_list_offset_;

  // The RenderView's current impression of the history length.  This includes
  // any items that have committed in this process, but because of cross-process
  // navigations, the history may have some entries that were committed in other
  // processes.  We won't know about them until the next navigation in this
  // process.
  int history_list_length_;

  // The list of page IDs for each history item this RenderView knows about.
  // Some entries may be -1 if they were rendered by other processes or were
  // restored from a previous session.  This lets us detect attempts to
  // navigate to stale entries that have been cropped from our history.
  std::vector<int32> history_page_ids_;

  // Page info -----------------------------------------------------------------

  // The last gotten main frame's encoding.
  std::string last_encoding_name_;

  // UI state ------------------------------------------------------------------

  // The state of our target_url transmissions. When we receive a request to
  // send a URL to the browser, we set this to TARGET_INFLIGHT until an ACK
  // comes back - if a new request comes in before the ACK, we store the new
  // URL in pending_target_url_ and set the status to TARGET_PENDING. If an
  // ACK comes back and we are in TARGET_PENDING, we send the stored URL and
  // revert to TARGET_INFLIGHT.
  //
  // We don't need a queue of URLs to send, as only the latest is useful.
  enum {
    TARGET_NONE,
    TARGET_INFLIGHT,  // We have a request in-flight, waiting for an ACK
    TARGET_PENDING    // INFLIGHT + we have a URL waiting to be sent
  } target_url_status_;

  // The URL we show the user in the status bar. We use this to determine if we
  // want to send a new one (we do not need to send duplicates). It will be
  // equal to either |mouse_over_url_| or |focus_url_|, depending on which was
  // updated last.
  GURL target_url_;

  // The URL the user's mouse is hovering over.
  GURL mouse_over_url_;

  // The URL that has keyboard focus.
  GURL focus_url_;

  // The next target URL we want to send to the browser.
  GURL pending_target_url_;

  // The text selection the last time DidChangeSelection got called.
  string16 selection_text_;
  size_t selection_text_offset_;
  ui::Range selection_range_;

  // View ----------------------------------------------------------------------

  // Cache the preferred size of the page in order to prevent sending the IPC
  // when layout() recomputes but doesn't actually change sizes.
  gfx::Size preferred_size_;

  // Used to delay determining the preferred size (to avoid intermediate
  // states for the sizes).
  base::OneShotTimer<RenderViewImpl> check_preferred_size_timer_;

  // These store the "is main frame is scrolled all the way to the left
  // or right" state that was last sent to the browser.
  bool cached_is_main_frame_pinned_to_left_;
  bool cached_is_main_frame_pinned_to_right_;

  // These store the "has scrollbars" state last sent to the browser.
  bool cached_has_main_frame_horizontal_scrollbar_;
  bool cached_has_main_frame_vertical_scrollbar_;

#if defined(OS_MACOSX)
  // Track the fake plugin window handles allocated on the browser side for
  // the accelerated compositor and (currently) accelerated plugins so that
  // we can discard them when the view goes away.
  std::set<gfx::PluginWindowHandle> fake_plugin_window_handles_;
#endif

  // Plugins -------------------------------------------------------------------

  PepperPluginDelegateImpl pepper_delegate_;

  // All the currently active plugin delegates for this RenderView; kept so
  // that we can enumerate them to send updates about things like window
  // location or tab focus and visibily. These are non-owning references.
  std::set<WebPluginDelegateProxy*> plugin_delegates_;

#if defined(OS_WIN)
  // The ID of the focused NPAPI plug-in.
  int focused_plugin_id_;
#endif

  // Helper objects ------------------------------------------------------------

  RendererWebCookieJarImpl cookie_jar_;

  // The next group of objects all implement RenderViewObserver, so are deleted
  // along with the RenderView automatically.  This is why we just store
  // weak references.

  // Holds a reference to the service which provides desktop notifications.
  NotificationProvider* notification_provider_;

  // The geolocation dispatcher attached to this view, lazily initialized.
  GeolocationDispatcher* geolocation_dispatcher_;

  // The intents dispatcher attached to this view. Not lazily initialized.
  IntentsDispatcher* intents_dispatcher_;

  // The speech dispatcher attached to this view, lazily initialized.
  SpeechInputDispatcher* speech_input_dispatcher_;

  // Device orientation dispatcher attached to this view; lazily initialized.
  DeviceOrientationDispatcher* device_orientation_dispatcher_;

  // MediaStreamImpl attached to this view; lazily initialized.
  scoped_refptr<MediaStreamImpl> media_stream_impl_;

  // Dispatches all P2P socket used by the renderer.
  content::P2PSocketDispatcher* p2p_socket_dispatcher_;

  DevToolsAgent* devtools_agent_;

  RendererAccessibility* renderer_accessibility_;

  // Misc ----------------------------------------------------------------------

  // The current and pending file chooser completion objects. If the queue is
  // nonempty, the first item represents the currently running file chooser
  // callback, and the remaining elements are the other file chooser completion
  // still waiting to be run (in order).
  struct PendingFileChooser;
  std::deque< linked_ptr<PendingFileChooser> > file_chooser_completions_;

  // The current directory enumeration callback
  std::map<int, WebKit::WebFileChooserCompletion*> enumeration_completions_;
  int enumeration_completion_id_;

  // The SessionStorage namespace that we're assigned to has an ID, and that ID
  // is passed to us upon creation.  WebKit asks for this ID upon first use and
  // uses it whenever asking the browser process to allocate new storage areas.
  int64 session_storage_namespace_id_;

  // The total number of unrequested popups that exist and can be followed back
  // to a common opener. This count is shared among all RenderViews created with
  // createView(). All popups are treated as unrequested until specifically
  // instructed otherwise by the Browser process.
  scoped_refptr<SharedRenderViewCounter> shared_popup_counter_;

  // Whether this is a top level window (instead of a popup). Top level windows
  // shouldn't count against their own |shared_popup_counter_|.
  bool decrement_shared_popup_at_destruction_;

  // If the browser hasn't sent us an ACK for the last FindReply we sent
  // to it, then we need to queue up the message (keeping only the most
  // recent message if new ones come in).
  scoped_ptr<IPC::Message> queued_find_reply_message_;

  // Stores edit commands associated to the next key event.
  // Shall be cleared as soon as the next key event is processed.
  EditCommands edit_commands_;

  // Allows Web UI pages (new tab page, etc.) to talk to the browser. The JS
  // object is only exposed when Web UI bindings are enabled.
  scoped_ptr<WebUIBindings> web_ui_bindings_;

  // The external popup for the currently showing select popup.
  scoped_ptr<ExternalPopupMenu> external_popup_menu_;

  // The node that the context menu was pressed over.
  WebKit::WebNode context_menu_node_;

  // Reports load progress to the browser.
  scoped_ptr<LoadProgressTracker> load_progress_tracker_;

  // All the registered observers.  We expect this list to be small, so vector
  // is fine.
  ObserverList<content::RenderViewObserver> observers_;

  // Used to inform didChangeSelection() when it is called in the context
  // of handling a ViewMsg_SelectRange IPC.
  bool handling_select_range_;

  // ---------------------------------------------------------------------------
  // ADDING NEW DATA? Please see if it fits appropriately in one of the above
  // sections rather than throwing it randomly at the end. If you're adding a
  // bunch of stuff, you should probably create a helper class and put your
  // data and methods on that to avoid bloating RenderView more.  You can
  // use the Observer interface to filter IPC messages and receive frame change
  // notifications.
  // ---------------------------------------------------------------------------

  DISALLOW_COPY_AND_ASSIGN(RenderViewImpl);
};

#endif  // CONTENT_RENDERER_RENDER_VIEW_IMPL_H_
