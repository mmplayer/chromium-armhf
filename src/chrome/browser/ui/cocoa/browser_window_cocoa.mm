// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sidebar/sidebar_container.h"
#include "chrome/browser/sidebar/sidebar_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/browser/avatar_button_controller.h"
#import "chrome/browser/ui/cocoa/browser/avatar_menu_bubble_controller.h"
#import "chrome/browser/ui/cocoa/browser/edit_search_engine_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#import "chrome/browser/ui/cocoa/content_settings/collected_cookies_mac.h"
#import "chrome/browser/ui/cocoa/download/download_shelf_controller.h"
#include "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"
#import "chrome/browser/ui/cocoa/html_dialog_window_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/nsmenuitem_additions.h"
#include "chrome/browser/ui/cocoa/page_info_window.h"
#include "chrome/browser/ui/cocoa/repost_form_warning_mac.h"
#include "chrome/browser/ui/cocoa/restart_browser.h"
#include "chrome/browser/ui/cocoa/status_bubble_mac.h"
#include "chrome/browser/ui/cocoa/task_manager_mac.h"
#import "chrome/browser/ui/cocoa/theme_install_bubble_view.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/task_manager_dialog.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_details.h"
#include "content/common/notification_service.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/rect.h"

// Replicate specific 10.7 SDK declarations for building with prior SDKs.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

enum {
  NSWindowAnimationBehaviorDefault = 0,
  NSWindowAnimationBehaviorNone = 2,
  NSWindowAnimationBehaviorDocumentWindow = 3,
  NSWindowAnimationBehaviorUtilityWindow = 4,
  NSWindowAnimationBehaviorAlertPanel = 5
};
typedef NSInteger NSWindowAnimationBehavior;

@interface NSWindow (LionSDKDeclarations)
- (NSWindowAnimationBehavior)animationBehavior;
- (void)setAnimationBehavior:(NSWindowAnimationBehavior)newAnimationBehavior;
@end

#endif  // MAC_OS_X_VERSION_10_7

BrowserWindowCocoa::BrowserWindowCocoa(Browser* browser,
                                       BrowserWindowController* controller)
  : browser_(browser),
    controller_(controller),
    confirm_close_factory_(browser) {
  registrar_.Add(this, chrome::NOTIFICATION_SIDEBAR_CHANGED,
                 Source<SidebarManager>(SidebarManager::GetInstance()));

  pref_change_registrar_.Init(browser_->profile()->GetPrefs());
  pref_change_registrar_.Add(prefs::kShowBookmarkBar, this);

  initial_show_state_ = browser_->GetSavedWindowShowState();
}

BrowserWindowCocoa::~BrowserWindowCocoa() {
}

void BrowserWindowCocoa::Show() {
  // The Browser associated with this browser window must become the active
  // browser at the time |Show()| is called. This is the natural behaviour under
  // Windows, but |-makeKeyAndOrderFront:| won't send |-windowDidBecomeMain:|
  // until we return to the runloop. Therefore any calls to
  // |BrowserList::GetLastActive()| (for example, in bookmark_util), will return
  // the previous browser instead if we don't explicitly set it here.
  BrowserList::SetLastActive(browser_);

  bool is_session_restore = browser_->is_session_restore();
  NSWindowAnimationBehavior saved_animation_behavior =
      NSWindowAnimationBehaviorDefault;
  bool did_save_animation_behavior = false;
  // Turn off swishing when restoring windows.
  if (is_session_restore &&
      [window() respondsToSelector:@selector(animationBehavior)] &&
      [window() respondsToSelector:@selector(setAnimationBehavior:)]) {
    did_save_animation_behavior = true;
    saved_animation_behavior = [window() animationBehavior];
    [window() setAnimationBehavior:NSWindowAnimationBehaviorNone];
  }

  [window() makeKeyAndOrderFront:controller_];

  // When creating windows from nibs it is necessary to |makeKeyAndOrderFront:|
  // prior to |orderOut:| then |miniaturize:| when restoring windows in the
  // minimized state.
  if (initial_show_state_ == ui::SHOW_STATE_MINIMIZED) {
    [window() orderOut:controller_];
    [window() miniaturize:controller_];
  }
  initial_show_state_ = ui::SHOW_STATE_DEFAULT;

  // Restore window animation behavior.
  if (did_save_animation_behavior)
    [window() setAnimationBehavior:saved_animation_behavior];

  browser_->OnWindowDidShow();
}

void BrowserWindowCocoa::ShowInactive() {
  [window() orderFront:controller_];
}

void BrowserWindowCocoa::SetBounds(const gfx::Rect& bounds) {
  gfx::Rect real_bounds = [controller_ enforceMinWindowSize:bounds];

  ExitFullscreen();
  NSRect cocoa_bounds = NSMakeRect(real_bounds.x(), 0,
                                   real_bounds.width(),
                                   real_bounds.height());
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  cocoa_bounds.origin.y =
      [screen frame].size.height - real_bounds.height() - real_bounds.y();

  [window() setFrame:cocoa_bounds display:YES];
}

// Callers assume that this doesn't immediately delete the Browser object.
// The controller implementing the window delegate methods called from
// |-performClose:| must take precautions to ensure that.
void BrowserWindowCocoa::Close() {
  // If there is an overlay window, we contain a tab being dragged between
  // windows. Don't hide the window as it makes the UI extra confused. We can
  // still close the window, as that will happen when the drag completes.
  if ([controller_ overlayWindow]) {
    [controller_ deferPerformClose];
  } else {
    // Make sure we hide the window immediately. Even though performClose:
    // calls orderOut: eventually, it leaves the window on-screen long enough
    // that we start to see tabs shutting down. http://crbug.com/23959
    // TODO(viettrungluu): This is kind of bad, since |-performClose:| calls
    // |-windowShouldClose:| (on its delegate, which is probably the
    // controller) which may return |NO| causing the window to not be closed,
    // thereby leaving a hidden window. In fact, our window-closing procedure
    // involves a (indirect) recursion on |-performClose:|, which is also bad.
    [window() orderOut:controller_];
    [window() performClose:controller_];
  }
}

void BrowserWindowCocoa::Activate() {
  [controller_ activate];
}

void BrowserWindowCocoa::Deactivate() {
  // TODO(jcivelli): http://crbug.com/51364 Implement me.
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::FlashFrame() {
  [NSApp requestUserAttention:NSInformationalRequest];
}

bool BrowserWindowCocoa::IsActive() const {
  return [window() isKeyWindow];
}

gfx::NativeWindow BrowserWindowCocoa::GetNativeHandle() {
  return window();
}

BrowserWindowTesting* BrowserWindowCocoa::GetBrowserWindowTesting() {
  return NULL;
}

StatusBubble* BrowserWindowCocoa::GetStatusBubble() {
  return [controller_ statusBubble];
}

void BrowserWindowCocoa::ToolbarSizeChanged(bool is_animating) {
  // According to beng, this is an ugly method that comes from the days when the
  // download shelf was a ChromeView attached to the TabContents, and as its
  // size changed via animation it notified through TCD/etc to the browser view
  // to relayout for each tick of the animation. We don't need anything of the
  // sort on Mac.
}

void BrowserWindowCocoa::UpdateTitleBar() {
  NSString* newTitle =
      base::SysUTF16ToNSString(browser_->GetWindowTitleForCurrentTab());

  pending_window_title_.reset(
      [BrowserWindowUtils scheduleReplaceOldTitle:pending_window_title_.get()
                                     withNewTitle:newTitle
                                        forWindow:window()]);
}

void BrowserWindowCocoa::BookmarkBarStateChanged(
    BookmarkBar::AnimateChangeType change_type) {
  // TODO: route changes to state through this.
}

void BrowserWindowCocoa::UpdateDevTools() {
  [controller_ updateDevToolsForContents:
      browser_->GetSelectedTabContents()];
}

void BrowserWindowCocoa::UpdateLoadingAnimations(bool should_animate) {
  // Do nothing on Mac.
}

void BrowserWindowCocoa::SetStarredState(bool is_starred) {
  [controller_ setStarredState:is_starred ? YES : NO];
}

gfx::Rect BrowserWindowCocoa::GetRestoredBounds() const {
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  NSRect frame = [controller_ regularWindowFrame];
  gfx::Rect bounds(frame.origin.x, 0, frame.size.width, frame.size.height);
  bounds.set_y([screen frame].size.height - frame.origin.y - frame.size.height);
  return bounds;
}

gfx::Rect BrowserWindowCocoa::GetBounds() const {
  return GetRestoredBounds();
}

bool BrowserWindowCocoa::IsMaximized() const {
  return [window() isZoomed];
}

bool BrowserWindowCocoa::IsMinimized() const {
  return [window() isMiniaturized];
}

void BrowserWindowCocoa::EnterFullscreen(
      const GURL& url, FullscreenExitBubbleType bubble_type) {
  [controller_ enterFullscreenForURL:url
                          bubbleType:bubble_type];
}

void BrowserWindowCocoa::ExitFullscreen() {
  [controller_ exitFullscreen];
}

void BrowserWindowCocoa::UpdateFullscreenExitBubbleContent(
      const GURL& url,
      FullscreenExitBubbleType bubble_type) {
  [controller_ updateFullscreenExitBubbleURL:url bubbleType:bubble_type];
}

bool BrowserWindowCocoa::IsFullscreen() const {
  return [controller_ isFullscreen];
}

bool BrowserWindowCocoa::IsFullscreenBubbleVisible() const {
  return false;
}

void BrowserWindowCocoa::ConfirmAddSearchProvider(
    const TemplateURL* template_url,
    Profile* profile) {
  // The controller will release itself when the window closes.
  EditSearchEngineCocoaController* editor =
      [[EditSearchEngineCocoaController alloc] initWithProfile:profile
                                                      delegate:NULL
                                                   templateURL:template_url];
  [NSApp beginSheet:[editor window]
     modalForWindow:window()
      modalDelegate:controller_
     didEndSelector:@selector(sheetDidEnd:returnCode:context:)
        contextInfo:NULL];
}

LocationBar* BrowserWindowCocoa::GetLocationBar() const {
  return [controller_ locationBarBridge];
}

void BrowserWindowCocoa::SetFocusToLocationBar(bool select_all) {
  [controller_ focusLocationBar:select_all ? YES : NO];
}

void BrowserWindowCocoa::UpdateReloadStopState(bool is_loading, bool force) {
  [controller_ setIsLoading:is_loading force:force];
}

void BrowserWindowCocoa::UpdateToolbar(TabContentsWrapper* contents,
                                       bool should_restore_state) {
  [controller_ updateToolbarWithContents:contents->tab_contents()
                      shouldRestoreState:should_restore_state ? YES : NO];
}

void BrowserWindowCocoa::FocusToolbar() {
  // Not needed on the Mac.
}

void BrowserWindowCocoa::FocusAppMenu() {
  // Chrome uses the standard Mac OS X menu bar, so this isn't needed.
}

void BrowserWindowCocoa::RotatePaneFocus(bool forwards) {
  // Not needed on the Mac.
}

void BrowserWindowCocoa::FocusBookmarksToolbar() {
  // Not needed on the Mac.
}

void BrowserWindowCocoa::FocusChromeOSStatus() {
  // Not needed on the Mac.
}

bool BrowserWindowCocoa::IsBookmarkBarVisible() const {
  return browser_->profile()->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
}

bool BrowserWindowCocoa::IsBookmarkBarAnimating() const {
  return [controller_ isBookmarkBarAnimating];
}

bool BrowserWindowCocoa::IsTabStripEditable() const {
  return ![controller_ isDragSessionActive];
}

bool BrowserWindowCocoa::IsToolbarVisible() const {
  return browser_->SupportsWindowFeature(Browser::FEATURE_TOOLBAR) ||
         browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR);
}

// This is called from Browser, which in turn is called directly from
// a menu option.  All we do here is set a preference.  The act of
// setting the preference sends notifications to all windows who then
// know what to do.
void BrowserWindowCocoa::ToggleBookmarkBar() {
  bookmark_utils::ToggleWhenVisible(browser_->profile());
}

void BrowserWindowCocoa::AddFindBar(
    FindBarCocoaController* find_bar_cocoa_controller) {
  [controller_ addFindBar:find_bar_cocoa_controller];
}

void BrowserWindowCocoa::ShowAboutChromeDialog() {
  // Go through AppController's implementation to bring up the branded panel.
  [[NSApp delegate] orderFrontStandardAboutPanel:nil];
}

void BrowserWindowCocoa::ShowUpdateChromeDialog() {
  restart_browser::RequestRestart(window());
}

void BrowserWindowCocoa::ShowTaskManager() {
#if defined(WEBUI_TASK_MANAGER)
  TaskManagerDialog::Show();
#else
  // Uses WebUI TaskManager when swiches is set. It is beta feature.
  if (CommandLine::ForCurrentProcess()
        ->HasSwitch(switches::kEnableWebUITaskManager)) {
    TaskManagerDialog::Show();
  } else {
    TaskManagerMac::Show(false);
  }
#endif  // defined(WEBUI_TASK_MANAGER)
}

void BrowserWindowCocoa::ShowBackgroundPages() {
#if defined(WEBUI_TASK_MANAGER)
  TaskManagerDialog::ShowBackgroundPages();
#else
  // Uses WebUI TaskManager when swiches is set. It is beta feature.
  if (CommandLine::ForCurrentProcess()
        ->HasSwitch(switches::kEnableWebUITaskManager)) {
    TaskManagerDialog::ShowBackgroundPages();
  } else {
    TaskManagerMac::Show(true);
  }
#endif  // defined(WEBUI_TASK_MANAGER)
}

void BrowserWindowCocoa::ShowBookmarkBubble(const GURL& url,
                                            bool already_bookmarked) {
  [controller_ showBookmarkBubbleForURL:url
                      alreadyBookmarked:(already_bookmarked ? YES : NO)];
}

bool BrowserWindowCocoa::IsDownloadShelfVisible() const {
  return [controller_ isDownloadShelfVisible] != NO;
}

DownloadShelf* BrowserWindowCocoa::GetDownloadShelf() {
  DownloadShelfController* shelfController = [controller_ downloadShelf];
  return [shelfController bridge];
}

void BrowserWindowCocoa::ShowRepostFormWarningDialog(
    TabContents* tab_contents) {
  RepostFormWarningMac::Create(GetNativeHandle(), tab_contents);
}

void BrowserWindowCocoa::ShowCollectedCookiesDialog(
    TabContentsWrapper* wrapper) {
  // Deletes itself on close.
  new CollectedCookiesMac(GetNativeHandle(), wrapper);
}

void BrowserWindowCocoa::ShowThemeInstallBubble() {
  ThemeInstallBubbleView::Show(window());
}

// We allow closing the window here since the real quit decision on Mac is made
// in [AppController quit:].
void BrowserWindowCocoa::ConfirmBrowserCloseWithPendingDownloads() {
  // Call InProgressDownloadResponse asynchronously to avoid a crash when the
  // browser window is closed here (http://crbug.com/44454).
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&Browser::InProgressDownloadResponse,
                 confirm_close_factory_.GetWeakPtr(), true));
}

void BrowserWindowCocoa::UserChangedTheme() {
  [controller_ userChangedTheme];
}

int BrowserWindowCocoa::GetExtraRenderViewHeight() const {
  // Currently this is only used on linux.
  return 0;
}

void BrowserWindowCocoa::TabContentsFocused(TabContents* tab_contents) {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowPageInfo(Profile* profile,
                                      const GURL& url,
                                      const NavigationEntry::SSLStatus& ssl,
                                      bool show_history) {
  browser::ShowPageInfoBubble(window(), profile, url, ssl, show_history);
}

void BrowserWindowCocoa::ShowAppMenu() {
  // No-op. Mac doesn't support showing the menus via alt keys.
}

bool BrowserWindowCocoa::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event, bool* is_keyboard_shortcut) {
  if (![BrowserWindowUtils shouldHandleKeyboardEvent:event])
    return false;

  int id = [BrowserWindowUtils getCommandId:event];
  if (id == -1)
    return false;

  if (browser_->IsReservedCommandOrKey(id, event)) {
      return [BrowserWindowUtils handleKeyboardEvent:event.os_event
                                            inWindow:window()];
  }

  DCHECK(is_keyboard_shortcut);
  *is_keyboard_shortcut = true;
  return false;
}

void BrowserWindowCocoa::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if ([BrowserWindowUtils shouldHandleKeyboardEvent:event])
    [BrowserWindowUtils handleKeyboardEvent:event.os_event inWindow:window()];
}

void BrowserWindowCocoa::ShowCreateWebAppShortcutsDialog(
    TabContentsWrapper* tab_contents) {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::ShowCreateChromeAppShortcutsDialog(
    Profile* profile, const Extension* app) {
  NOTIMPLEMENTED();
}

void BrowserWindowCocoa::Cut() {
  [NSApp sendAction:@selector(cut:) to:nil from:nil];
}

void BrowserWindowCocoa::Copy() {
  [NSApp sendAction:@selector(copy:) to:nil from:nil];
}

void BrowserWindowCocoa::Paste() {
  [NSApp sendAction:@selector(paste:) to:nil from:nil];
}

void BrowserWindowCocoa::OpenTabpose() {
  [controller_ openTabpose];
}

void BrowserWindowCocoa::EnterPresentationMode(
      const GURL& url,
      FullscreenExitBubbleType bubble_type) {
  [controller_ enterPresentationModeForURL:url
                                bubbleType:bubble_type];
}

void BrowserWindowCocoa::ExitPresentationMode() {
  [controller_ exitPresentationMode];
}

bool BrowserWindowCocoa::InPresentationMode() {
  return [controller_ inPresentationMode];
}

void BrowserWindowCocoa::ShowInstant(TabContentsWrapper* preview) {
  [controller_ showInstant:preview->tab_contents()];
}

void BrowserWindowCocoa::HideInstant() {
  [controller_ hideInstant];
}

gfx::Rect BrowserWindowCocoa::GetInstantBounds() {
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  NSRect monitorFrame = [screen frame];
  NSRect frame = [controller_ instantFrame];
  gfx::Rect bounds(NSRectToCGRect(frame));
  bounds.set_y(NSHeight(monitorFrame) - bounds.y() - bounds.height());
  return bounds;
}

WindowOpenDisposition BrowserWindowCocoa::GetDispositionForPopupBounds(
    const gfx::Rect& bounds) {
  return NEW_POPUP;
}

FindBar* BrowserWindowCocoa::CreateFindBar() {
  // We could push the AddFindBar() call into the FindBarBridge
  // constructor or the FindBarCocoaController init, but that makes
  // unit testing difficult, since we would also require a
  // BrowserWindow object.
  FindBarBridge* bridge = new FindBarBridge();
  AddFindBar(bridge->find_bar_cocoa_controller());
  return bridge;
}

void BrowserWindowCocoa::Observe(int type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PREF_CHANGED: {
      const std::string& pref_name = *Details<std::string>(details).ptr();
      DCHECK(pref_name == prefs::kShowBookmarkBar);
      [controller_ updateBookmarkBarVisibilityWithAnimation:YES];
      break;
    }
    case chrome::NOTIFICATION_SIDEBAR_CHANGED:
      UpdateSidebarForContents(
          Details<SidebarContainer>(details)->tab_contents());
      break;
    default:
      NOTREACHED();  // we don't ask for anything else!
      break;
  }
}

void BrowserWindowCocoa::DestroyBrowser() {
  [controller_ destroyBrowser];

  // at this point the controller is dead (autoreleased), so
  // make sure we don't try to reference it any more.
}

NSWindow* BrowserWindowCocoa::window() const {
  return [controller_ window];
}

void BrowserWindowCocoa::UpdateSidebarForContents(TabContents* tab_contents) {
  if (tab_contents == browser_->GetSelectedTabContents()) {
    [controller_ updateSidebarForContents:tab_contents];
  }
}

void BrowserWindowCocoa::ShowAvatarBubble(TabContents* tab_contents,
                                          const gfx::Rect& rect) {
  NSView* view = tab_contents->GetNativeView();
  NSRect bounds = [view bounds];
  NSPoint point;
  point.x = NSMinX(bounds) + rect.right();
  // The view's origin is at the bottom but |rect|'s origin is at the top.
  point.y = NSMaxY(bounds) - rect.bottom();
  point = [view convertPoint:point toView:nil];
  point = [[view window] convertBaseToScreen:point];

  // |menu| will automatically release itself on close.
  AvatarMenuBubbleController* menu =
      [[AvatarMenuBubbleController alloc] initWithBrowser:browser_
                                               anchoredAt:point];
  [[menu bubble] setAlignment:info_bubble::kAlignEdgeToAnchorEdge];
  [menu showWindow:nil];
}

void BrowserWindowCocoa::ShowAvatarBubbleFromAvatarButton() {
  [[controller_ avatarButtonController] showAvatarBubble];
}
