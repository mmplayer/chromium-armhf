// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/metrics/thread_watcher.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "content/common/result_codes.h"

#if defined(OS_MACOSX)
#include "chrome/browser/chrome_browser_application_mac.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/cros/update_library.h"
#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chromeos/wm_ipc.h"
#endif
#endif

namespace {

// This object is instantiated when the first Browser object is added to the
// list and delete when the last one is removed. It watches for loads and
// creates histograms of some global object counts.
class BrowserActivityObserver : public NotificationObserver {
 public:
  BrowserActivityObserver() {
    registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                   NotificationService::AllSources());
  }
  ~BrowserActivityObserver() {}

 private:
  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK(type == content::NOTIFICATION_NAV_ENTRY_COMMITTED);
    const content::LoadCommittedDetails& load =
        *Details<content::LoadCommittedDetails>(details).ptr();
    if (!load.is_navigation_to_different_page())
      return;  // Don't log for subframes or other trivial types.

    LogRenderProcessHostCount();
    LogBrowserTabCount();
  }

  // Counts the number of active RenderProcessHosts and logs them.
  void LogRenderProcessHostCount() const {
    int hosts_count = 0;
    for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
         !i.IsAtEnd(); i.Advance())
      ++hosts_count;
    UMA_HISTOGRAM_CUSTOM_COUNTS("MPArch.RPHCountPerLoad", hosts_count,
                                1, 50, 50);
  }

  // Counts the number of tabs in each browser window and logs them. This is
  // different than the number of TabContents objects since TabContents objects
  // can be used for popups and in dialog boxes. We're just counting toplevel
  // tabs here.
  void LogBrowserTabCount() const {
    int tab_count = 0;
    for (BrowserList::const_iterator browser_iterator = BrowserList::begin();
         browser_iterator != BrowserList::end(); browser_iterator++) {
      // Record how many tabs each window has open.
      UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerWindow",
                                  (*browser_iterator)->tab_count(), 1, 200, 50);
      tab_count += (*browser_iterator)->tab_count();
    }
    // Record how many tabs total are open (across all windows).
    UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerLoad", tab_count, 1, 200, 50);

    Browser* browser = BrowserList::GetLastActive();
    if (browser) {
      // Record how many tabs the active window has open.
      UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountActiveWindow",
                                  browser->tab_count(), 1, 200, 50);
    }
  }

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActivityObserver);
};

BrowserActivityObserver* activity_observer = NULL;

// Type used to indicate to match anything.
const int kMatchAny                     = 0;

// See BrowserMatches for details.
const int kMatchOriginalProfile         = 1 << 0;
const int kMatchCanSupportWindowFeature = 1 << 1;
const int kMatchTabbed                  = 1 << 2;

// Returns true if the specified |browser| matches the specified arguments.
// |match_types| is a bitmask dictating what parameters to match:
// . If it contains kMatchOriginalProfile then the original profile of the
//   browser must match |profile->GetOriginalProfile()|. This is used to match
//   incognito windows.
// . If it contains kMatchCanSupportWindowFeature
//   |CanSupportWindowFeature(window_feature)| must return true.
// . If it contains kMatchTabbed, the browser must be a tabbed browser.
bool BrowserMatches(Browser* browser,
                    Profile* profile,
                    Browser::WindowFeature window_feature,
                    uint32 match_types) {
  if (match_types & kMatchCanSupportWindowFeature &&
      !browser->CanSupportWindowFeature(window_feature)) {
    return false;
  }

  if (match_types & kMatchOriginalProfile) {
    if (browser->profile()->GetOriginalProfile() !=
        profile->GetOriginalProfile())
      return false;
  } else if (browser->profile() != profile) {
    return false;
  }

  if (match_types & kMatchTabbed)
    return browser->is_type_tabbed();

  return true;
}

// Returns the first browser in the specified iterator that returns true from
// |BrowserMatches|, or null if no browsers match the arguments. See
// |BrowserMatches| for details on the arguments.
template <class T>
Browser* FindBrowserMatching(const T& begin,
                             const T& end,
                             Profile* profile,
                             Browser::WindowFeature window_feature,
                             uint32 match_types) {
  for (T i = begin; i != end; ++i) {
    if (BrowserMatches(*i, profile, window_feature, match_types))
      return *i;
  }
  return NULL;
}

Browser* FindBrowserWithTabbedOrAnyType(Profile* profile,
                                        bool match_tabbed,
                                        bool match_incognito) {
  uint32 match_types = kMatchAny;
  if (match_tabbed)
    match_types |= kMatchTabbed;
  if (match_incognito)
    match_types |= kMatchOriginalProfile;
  Browser* browser = FindBrowserMatching(
      BrowserList::begin_last_active(), BrowserList::end_last_active(),
      profile, Browser::FEATURE_NONE, match_types);
  // Fall back to a forward scan of all Browsers if no active one was found.
  return browser ? browser :
      FindBrowserMatching(BrowserList::begin(), BrowserList::end(), profile,
                          Browser::FEATURE_NONE, match_types);
}

printing::BackgroundPrintingManager* GetBackgroundPrintingManager() {
  return g_browser_process->background_printing_manager();
}

// Returns true if all browsers can be closed without user interaction.
// This currently checks if there is pending download, or if it needs to
// handle unload handler.
bool AreAllBrowsersCloseable() {
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    bool normal_downloads_are_present = false;
    bool incognito_downloads_are_present = false;
    (*i)->CheckDownloadsInProgress(&normal_downloads_are_present,
                                   &incognito_downloads_are_present);
    if (normal_downloads_are_present ||
        incognito_downloads_are_present ||
        (*i)->TabsNeedBeforeUnloadFired())
      return false;
  }
  return true;
}

#if defined(OS_CHROMEOS)

bool signout = false;

// Fast shutdown for ChromeOS. It tells session manager to start
// shutdown process when closing browser windows won't be canceled.
// Returns true if fast shutdown is successfully started.
bool FastShutdown() {
  signout = true;
  if (chromeos::CrosLibrary::Get()->EnsureLoaded()
      && AreAllBrowsersCloseable()) {
    BrowserList::NotifyAndTerminate(true);
    return true;
  }
  return false;
}

void NotifyWindowManagerAboutSignout() {
#if defined(TOOLKIT_USES_GTK)
  static bool notified = false;
  if (!notified) {
    // Let the window manager know that we're going away before we start closing
    // windows so it can display a graceful transition to a black screen.
    chromeos::WmIpc::instance()->NotifyAboutSignout();
    notified = true;
  }
#endif
}

#endif

}  // namespace

BrowserList::BrowserVector BrowserList::browsers_;
ObserverList<BrowserList::Observer> BrowserList::observers_;

// static
void BrowserList::AddBrowser(Browser* browser) {
  DCHECK(browser);
  browsers_.push_back(browser);

  g_browser_process->AddRefModule();

  if (!activity_observer)
    activity_observer = new BrowserActivityObserver;

  NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSER_OPENED,
      Source<Browser>(browser),
      NotificationService::NoDetails());

  // Send out notifications after add has occurred. Do some basic checking to
  // try to catch evil observers that change the list from under us.
  size_t original_count = observers_.size();
  FOR_EACH_OBSERVER(Observer, observers_, OnBrowserAdded(browser));
  DCHECK_EQ(original_count, observers_.size())
      << "observer list modified during notification";
}

// static
void BrowserList::MarkAsCleanShutdown() {
  for (const_iterator i = begin(); i != end(); ++i) {
    (*i)->profile()->MarkAsCleanShutdown();
  }
}

void BrowserList::AttemptExitInternal() {
  NotificationService::current()->Notify(
      content::NOTIFICATION_APP_EXITING,
      NotificationService::AllSources(),
      NotificationService::NoDetails());

#if !defined(OS_MACOSX)
  // On most platforms, closing all windows causes the application to exit.
  CloseAllBrowsers();
#else
  // On the Mac, the application continues to run once all windows are closed.
  // Terminate will result in a CloseAllBrowsers() call, and once (and if)
  // that is done, will cause the application to exit cleanly.
  chrome_browser_application_mac::Terminate();
#endif
}

// static
void BrowserList::NotifyAndTerminate(bool fast_path) {
#if defined(OS_CHROMEOS)
  if (!signout)
    return;
#endif

  if (fast_path) {
    NotificationService::current()->Notify(
        content::NOTIFICATION_APP_TERMINATING,
        NotificationService::AllSources(),
        NotificationService::NoDetails());
  }

#if defined(OS_CHROMEOS)
  NotifyWindowManagerAboutSignout();
  chromeos::CrosLibrary* cros_library = chromeos::CrosLibrary::Get();
  if (cros_library->EnsureLoaded()) {
    // If update has been installed, reboot, otherwise, sign out.
    if (cros_library->GetUpdateLibrary()->status().status ==
          chromeos::UPDATE_STATUS_UPDATED_NEED_REBOOT) {
      cros_library->GetUpdateLibrary()->RebootAfterUpdate();
    } else {
      cros_library->GetLoginLibrary()->StopSession("");
    }
    return;
  }
  // If running the Chrome OS build, but we're not on the device, fall through
#endif
  AllBrowsersClosedAndAppExiting();
}

// static
void BrowserList::RemoveBrowser(Browser* browser) {
  RemoveBrowserFrom(browser, &last_active_browsers_);

  // Closing all windows does not indicate quitting the application on the Mac,
  // however, many UI tests rely on this behavior so leave it be for now and
  // simply ignore the behavior on the Mac outside of unit tests.
  // TODO(andybons): Fix the UI tests to Do The Right Thing.
  bool closing_last_browser = (browsers_.size() == 1);
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      Source<Browser>(browser), Details<bool>(&closing_last_browser));

  RemoveBrowserFrom(browser, &browsers_);

  // Do some basic checking to try to catch evil observers
  // that change the list from under us.
  size_t original_count = observers_.size();
  FOR_EACH_OBSERVER(Observer, observers_, OnBrowserRemoved(browser));
  DCHECK_EQ(original_count, observers_.size())
      << "observer list modified during notification";

  // If the last Browser object was destroyed, make sure we try to close any
  // remaining dependent windows too.
  if (browsers_.empty()) {
    delete activity_observer;
    activity_observer = NULL;
  }

  g_browser_process->ReleaseModule();

  // If we're exiting, send out the APP_TERMINATING notification to allow other
  // modules to shut themselves down.
  if (browsers_.empty() &&
      (browser_shutdown::IsTryingToQuit() ||
       g_browser_process->IsShuttingDown())) {
    // Last browser has just closed, and this is a user-initiated quit or there
    // is no module keeping the app alive, so send out our notification. No need
    // to call ProfileManager::ShutdownSessionServices() as part of the
    // shutdown, because Browser::WindowClosing() already makes sure that the
    // SessionService is created and notified.
    NotificationService::current()->Notify(
        content::NOTIFICATION_APP_TERMINATING,
        NotificationService::AllSources(),
        NotificationService::NoDetails());
    AllBrowsersClosedAndAppExiting();
  }
}

// static
void BrowserList::AddObserver(BrowserList::Observer* observer) {
  observers_.AddObserver(observer);
}

// static
void BrowserList::RemoveObserver(BrowserList::Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
void BrowserList::CloseAllBrowsers() {
  bool session_ending =
      browser_shutdown::GetShutdownType() == browser_shutdown::END_SESSION;
  bool force_exit = false;
#if defined(USE_X11)
  if (session_ending)
    force_exit = true;
#endif
  // Tell everyone that we are shutting down.
  browser_shutdown::SetTryingToQuit(true);

  // Before we close the browsers shutdown all session services. That way an
  // exit can restore all browsers open before exiting.
  ProfileManager::ShutdownSessionServices();

  // If there are no browsers, send the APP_TERMINATING action here. Otherwise,
  // it will be sent by RemoveBrowser() when the last browser has closed.
  if (force_exit || browsers_.empty()) {
    NotifyAndTerminate(true);
    return;
  }
#if defined(OS_CHROMEOS)
  chromeos::BootTimesLoader::Get()->AddLogoutTimeMarker(
      "StartedClosingWindows", false);
#endif
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end();) {
    Browser* browser = *i;
    browser->window()->Close();
    if (!session_ending) {
      ++i;
    } else {
      // This path is hit during logoff/power-down. In this case we won't get
      // a final message and so we force the browser to be deleted.
      // Close doesn't immediately destroy the browser
      // (Browser::TabStripEmpty() uses invoke later) but when we're ending the
      // session we need to make sure the browser is destroyed now. So, invoke
      // DestroyBrowser to make sure the browser is deleted and cleanup can
      // happen.
      while (browser->tab_count())
        delete browser->GetTabContentsWrapperAt(0);
      browser->window()->DestroyBrowser();
      i = BrowserList::begin();
      if (i != BrowserList::end() && browser == *i) {
        // Destroying the browser should have removed it from the browser list.
        // We should never get here.
        NOTREACHED();
        return;
      }
    }
  }
}

void BrowserList::CloseAllBrowsersWithProfile(Profile* profile) {
  BrowserVector browsers_to_close;
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if (BrowserMatches(*i, profile, Browser::FEATURE_NONE,
        kMatchOriginalProfile)) {
      browsers_to_close.push_back(*i);
    }
  }

  for (BrowserVector::const_iterator i = browsers_to_close.begin();
       i != browsers_to_close.end(); ++i) {
    (*i)->window()->Close();
  }
}

// static
void BrowserList::AttemptUserExit() {
#if defined(OS_CHROMEOS)
  chromeos::BootTimesLoader::Get()->AddLogoutTimeMarker("LogoutStarted", false);
  // Write /tmp/uptime-logout-started as well.
  const char kLogoutStarted[] = "logout-started";
  chromeos::BootTimesLoader::Get()->RecordCurrentStats(kLogoutStarted);

  // Login screen should show up in owner's locale.
  PrefService* state = g_browser_process->local_state();
  if (state) {
    std::string owner_locale = state->GetString(prefs::kOwnerLocale);
    if (!owner_locale.empty() &&
        state->GetString(prefs::kApplicationLocale) != owner_locale &&
        !state->IsManagedPreference(prefs::kApplicationLocale)) {
      state->SetString(prefs::kApplicationLocale, owner_locale);
      state->SavePersistentPrefs();
    }
  }
  if (FastShutdown())
    return;
#else
  // Reset the restart bit that might have been set in cancelled restart
  // request.
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, false);
#endif
  AttemptExitInternal();
}

// static
void BrowserList::AttemptRestart() {
#if defined(OS_CHROMEOS)
  // For CrOS instead of browser restart (which is not supported) perform a full
  // sign out. Session will be only restored if user has that setting set.
  // Same session restore behavior happens in case of full restart after update.
  AttemptUserExit();
#else
  // Set the flag to restore state after the restart.
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, true);
  AttemptExit();
#endif
}

// static
void BrowserList::AttemptExit() {
  // If we know that all browsers can be closed without blocking,
  // don't notify users of crashes beyond this point.
  // Note that MarkAsCleanShutdown does not set UMA's exit cleanly bit
  // so crashes during shutdown are still reported in UMA.
  if (AreAllBrowsersCloseable())
    MarkAsCleanShutdown();
  AttemptExitInternal();
}

#if defined(OS_CHROMEOS)
// static
void BrowserList::ExitCleanly() {
  // We always mark exit cleanly.
  g_browser_process->EndSession();
  AttemptExitInternal();
}
#endif

static void TimeLimitedSessionEnding() {
  // Start watching for hang during shutdown, and crash it if takes too long.
  // We disarm when |shutdown_watcher| object is destroyed, which is when we
  // exit this function.
  ShutdownWatcherHelper shutdown_watcher;
  shutdown_watcher.Arm(base::TimeDelta::FromSeconds(90));

  // EndSession is invoked once per frame. Only do something the first time.
  static bool already_ended = false;
  // We may get called in the middle of shutdown, e.g. http://crbug.com/70852
  // In this case, do nothing.
  if (already_ended || !NotificationService::current())
    return;
  already_ended = true;

  browser_shutdown::OnShutdownStarting(browser_shutdown::END_SESSION);

  NotificationService::current()->Notify(
      content::NOTIFICATION_APP_EXITING,
      NotificationService::AllSources(),
      NotificationService::NoDetails());

  // Write important data first.
  g_browser_process->EndSession();

  BrowserList::CloseAllBrowsers();

  // Send out notification. This is used during testing so that the test harness
  // can properly shutdown before we exit.
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_SESSION_END,
      NotificationService::AllSources(),
      NotificationService::NoDetails());

  // And shutdown.
  browser_shutdown::Shutdown();
}

// static
void BrowserList::SessionEnding() {
  TimeLimitedSessionEnding();

#if defined(OS_WIN)
  // At this point the message loop is still running yet we've shut everything
  // down. If any messages are processed we'll likely crash. Exit now.
  ExitProcess(content::RESULT_CODE_NORMAL_EXIT);
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
  _exit(content::RESULT_CODE_NORMAL_EXIT);
#else
  NOTIMPLEMENTED();
#endif
}

// static
bool BrowserList::HasBrowserWithProfile(Profile* profile) {
  return FindBrowserMatching(BrowserList::begin(),
                             BrowserList::end(),
                             profile,
                             Browser::FEATURE_NONE,
                             kMatchAny) != NULL;
}

// static
int BrowserList::keep_alive_count_ = 0;

// static
void BrowserList::StartKeepAlive() {
  // Increment the browser process refcount as long as we're keeping the
  // application alive.
  if (!WillKeepAlive())
    g_browser_process->AddRefModule();
  keep_alive_count_++;
}

// static
void BrowserList::EndKeepAlive() {
  DCHECK_GT(keep_alive_count_, 0);
  keep_alive_count_--;

  DCHECK(g_browser_process);
  // Although we should have a browser process, if there is none,
  // there is nothing to do.
  if (!g_browser_process) return;

  // Allow the app to shutdown again.
  if (!WillKeepAlive()) {
    g_browser_process->ReleaseModule();
    // If there are no browsers open and we aren't already shutting down,
    // initiate a shutdown. Also skips shutdown if this is a unit test
    // (MessageLoop::current() == null).
    if (browsers_.empty() && !browser_shutdown::IsTryingToQuit() &&
        MessageLoop::current())
      CloseAllBrowsers();
  }
}

// static
bool BrowserList::WillKeepAlive() {
  return keep_alive_count_ > 0;
}

// static
BrowserList::BrowserVector BrowserList::last_active_browsers_;

// static
void BrowserList::SetLastActive(Browser* browser) {
  // If the browser is currently trying to quit, we don't want to set the last
  // active browser because that can alter the last active browser that the user
  // intended depending on the order in which the windows close.
  if (browser_shutdown::IsTryingToQuit())
    return;
  RemoveBrowserFrom(browser, &last_active_browsers_);
  last_active_browsers_.push_back(browser);

  FOR_EACH_OBSERVER(Observer, observers_, OnBrowserSetLastActive(browser));
}

// static
Browser* BrowserList::GetLastActive() {
  if (!last_active_browsers_.empty())
    return *(last_active_browsers_.rbegin());

  return NULL;
}

// static
Browser* BrowserList::GetLastActiveWithProfile(Profile* profile) {
  // We are only interested in last active browsers, so we don't fall back to
  // all browsers like FindBrowserWith* do.
  return FindBrowserMatching(
      BrowserList::begin_last_active(), BrowserList::end_last_active(), profile,
      Browser::FEATURE_NONE, kMatchAny);
}

// static
Browser* BrowserList::FindTabbedBrowser(Profile* profile,
                                        bool match_incognito) {
  return FindBrowserWithTabbedOrAnyType(profile, true, match_incognito);
}

// static
Browser* BrowserList::FindAnyBrowser(Profile* profile, bool match_incognito) {
  return FindBrowserWithTabbedOrAnyType(profile, false, match_incognito);
}

// static
Browser* BrowserList::FindBrowserWithFeature(Profile* profile,
                                             Browser::WindowFeature feature) {
  Browser* browser = FindBrowserMatching(
      BrowserList::begin_last_active(), BrowserList::end_last_active(),
      profile, feature, kMatchCanSupportWindowFeature);
  // Fall back to a forward scan of all Browsers if no active one was found.
  return browser ? browser :
      FindBrowserMatching(BrowserList::begin(), BrowserList::end(), profile,
                          feature, kMatchCanSupportWindowFeature);
}

// static
Browser* BrowserList::FindBrowserWithProfile(Profile* profile) {
  return FindAnyBrowser(profile, false);
}

// static
Browser* BrowserList::FindBrowserWithID(SessionID::id_type desired_id) {
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if ((*i)->session_id().id() == desired_id)
      return *i;
  }
  return NULL;
}

// static
Browser* BrowserList::FindBrowserWithWindow(gfx::NativeWindow window) {
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end();
       ++it) {
    Browser* browser = *it;
    if (browser->window() && browser->window()->GetNativeHandle() == window)
      return browser;
  }
  return NULL;
}

// static
Browser* BrowserList::FindBrowserWithTabContents(TabContents* tab_contents) {
  DCHECK(tab_contents);
  for (TabContentsIterator it; !it.done(); ++it) {
    if (it->tab_contents() == tab_contents)
      return it.browser();
  }
  return NULL;
}

// static
size_t BrowserList::GetBrowserCountForType(Profile* profile,
                                           bool match_tabbed) {
  size_t result = 0;
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if (BrowserMatches(*i, profile, Browser::FEATURE_NONE,
                       match_tabbed ? kMatchTabbed : kMatchAny))
      ++result;
  }
  return result;
}

// static
size_t BrowserList::GetBrowserCount(Profile* profile) {
  size_t result = 0;
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if (BrowserMatches(*i, profile, Browser::FEATURE_NONE, kMatchAny)) {
      result++;
    }
  }
  return result;
}

// static
bool BrowserList::IsOffTheRecordSessionActive() {
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if ((*i)->profile()->IsOffTheRecord())
      return true;
  }
  return false;
}
// static
bool BrowserList::IsOffTheRecordSessionActiveForProfile(Profile* profile) {
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if ((*i)->profile()->IsSameProfile(profile) &&
        (*i)->profile()->IsOffTheRecord()) {
      return true;
    }
  }
  return false;
}

// static
void BrowserList::RemoveBrowserFrom(Browser* browser,
                                    BrowserVector* browser_list) {
  const iterator remove_browser =
      std::find(browser_list->begin(), browser_list->end(), browser);
  if (remove_browser != browser_list->end())
    browser_list->erase(remove_browser);
}

TabContentsIterator::TabContentsIterator()
    : browser_iterator_(BrowserList::begin()),
      web_view_index_(-1),
      bg_printing_iterator_(GetBackgroundPrintingManager()->begin()),
      cur_(NULL) {
  Advance();
}

void TabContentsIterator::Advance() {
  // The current TabContents should be valid unless we are at the beginning.
  DCHECK(cur_ || (web_view_index_ == -1 &&
                  browser_iterator_ == BrowserList::begin()))
      << "Trying to advance past the end";

  // Update cur_ to the next TabContents in the list.
  while (browser_iterator_ != BrowserList::end()) {
    if (++web_view_index_ >= (*browser_iterator_)->tab_count()) {
      // Advance to the next Browser in the list.
      ++browser_iterator_;
      web_view_index_ = -1;
      continue;
    }

    TabContentsWrapper* next_tab =
        (*browser_iterator_)->GetTabContentsWrapperAt(web_view_index_);
    if (next_tab) {
      cur_ = next_tab;
      return;
    }
  }
  // If no more TabContents from Browsers, check the BackgroundPrintingManager.
  while (bg_printing_iterator_ != GetBackgroundPrintingManager()->end()) {
    cur_ = *bg_printing_iterator_;
    CHECK(cur_);
    ++bg_printing_iterator_;
    return;
  }
  // Reached the end - no more TabContents.
  cur_ = NULL;
}
