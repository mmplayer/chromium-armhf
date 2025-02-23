// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_SERVICE_H_
#define CHROME_BROWSER_SESSIONS_SESSION_SERVICE_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/time.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/sessions/base_session_service.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/base/ui_base_types.h"

class NavigationEntry;
class Profile;
class SessionCommand;
struct SessionTab;
struct SessionWindow;
class TabContentsWrapper;

// SessionService ------------------------------------------------------------

// SessionService is responsible for maintaining the state of open windows
// and tabs so that they can be restored at a later date. The state of the
// currently open browsers is referred to as the current session.
//
// SessionService supports restoring from the last session. The last session
// typically corresponds to the last run of the browser, but not always. For
// example, if the user has a tabbed browser and app window running, closes the
// tabbed browser, then creates a new tabbed browser the current session is made
// the last session and the current session reset. This is done to provide the
// illusion that app windows run in separate processes. Similar behavior occurs
// with incognito windows.
//
// SessionService itself maintains a set of SessionCommands that allow
// SessionService to rebuild the open state of the browser (as
// SessionWindow, SessionTab and TabNavigation). The commands are periodically
// flushed to SessionBackend and written to a file. Every so often
// SessionService rebuilds the contents of the file from the open state
// of the browser.
class SessionService : public BaseSessionService,
                       public NotificationObserver {
  friend class SessionServiceTestHelper;
 public:
  // Creates a SessionService for the specified profile.
  explicit SessionService(Profile* profile);
  // For testing.
  explicit SessionService(const FilePath& save_path);

  virtual ~SessionService();

  // Returns true if a new window opening should really be treated like the
  // start of a session (with potential session restore, startup URLs, etc.).
  // In particular, this is true if there are no tabbed browsers running
  // currently (eg. because only background or other app pages are running).
  bool ShouldNewWindowStartSession();

  // Invoke at a point when you think session restore might occur. For example,
  // during startup and window creation this is invoked to see if a session
  // needs to be restored. If a session needs to be restored it is done so
  // asynchronously and true is returned. If false is returned the session was
  // not restored and the caller needs to create a new window.
  bool RestoreIfNecessary(const std::vector<GURL>& urls_to_open);

  // Resets the contents of the file from the current state of all open
  // browsers whose profile matches our profile.
  void ResetFromCurrentBrowsers();

  // Moves the current session to the last session. This is useful when a
  // checkpoint occurs, such as when the user launches the app and no tabbed
  // browsers are running.
  void MoveCurrentSessionToLastSession();

  // Associates a tab with a window.
  void SetTabWindow(const SessionID& window_id,
                    const SessionID& tab_id);

  // Sets the bounds of a window.
  void SetWindowBounds(const SessionID& window_id,
                       const gfx::Rect& bounds,
                       ui::WindowShowState show_state);

  // Sets the visual index of the tab in its parent window.
  void SetTabIndexInWindow(const SessionID& window_id,
                           const SessionID& tab_id,
                           int new_index);

  // Sets the pinned state of the tab.
  void SetPinnedState(const SessionID& window_id,
                      const SessionID& tab_id,
                      bool is_pinned);

  // Notification that a tab has been closed. |closed_by_user_gesture| comes
  // from |TabContents::closed_by_user_gesture|; see it for details.
  //
  // Note: this is invoked from the NavigationController's destructor, which is
  // after the actual tab has been removed.
  void TabClosed(const SessionID& window_id,
                 const SessionID& tab_id,
                 bool closed_by_user_gesture);

  // Notification the window is about to close.
  void WindowClosing(const SessionID& window_id);

  // Notification a window has finished closing.
  void WindowClosed(const SessionID& window_id);

  // Sets the type of window. In order for the contents of a window to be
  // tracked SetWindowType must be invoked with a type we track
  // (should_track_changes_for_browser_type returns true).
  void SetWindowType(const SessionID& window_id, Browser::Type type);

  // Invoked when the NavigationController has removed entries from the back of
  // the list. |count| gives the number of entries in the navigation controller.
  void TabNavigationPathPrunedFromBack(const SessionID& window_id,
                                       const SessionID& tab_id,
                                       int count);

  // Invoked when the NavigationController has removed entries from the front of
  // the list. |count| gives the number of entries that were removed.
  void TabNavigationPathPrunedFromFront(const SessionID& window_id,
                                        const SessionID& tab_id,
                                        int count);

  // Updates the navigation entry for the specified tab.
  void UpdateTabNavigation(const SessionID& window_id,
                           const SessionID& tab_id,
                           int index,
                           const NavigationEntry& entry);

  // Notification that a tab has restored its entries or a closed tab is being
  // reused.
  void TabRestored(TabContentsWrapper* tab, bool pinned);

  // Sets the index of the selected entry in the navigation controller for the
  // specified tab.
  void SetSelectedNavigationIndex(const SessionID& window_id,
                                  const SessionID& tab_id,
                                  int index);

  // Sets the index of the selected tab in the specified window.
  void SetSelectedTabInWindow(const SessionID& window_id, int index);

  // Callback from GetSavedSession of GetLastSession.
  //
  // The contents of the supplied vector are deleted after the callback is
  // notified. To take ownership of the vector clear it before returning.
  //
  // The time gives the time the session was closed.
  typedef Callback2<Handle, std::vector<SessionWindow*>*>::Type
      SessionCallback;

  // Fetches the contents of the last session, notifying the callback when
  // done. If the callback is supplied an empty vector of SessionWindows
  // it means the session could not be restored.
  //
  // The created request does NOT directly invoke the callback, rather the
  // callback invokes OnGotSessionCommands from which we map the
  // SessionCommands to browser state, then notify the callback.
  Handle GetLastSession(CancelableRequestConsumerBase* consumer,
                        SessionCallback* callback);

  // Fetches the contents of the current session, notifying the callback when
  // done. If the callback is supplied an empty vector of SessionWindows
  // it means the session could not be restored.
  //
  // The created request does NOT directly invoke the callback, rather the
  // callback invokes OnGotSessionCommands from which we map the
  // SessionCommands to browser state, then notify the callback.
  Handle GetCurrentSession(CancelableRequestConsumerBase* consumer,
                           SessionCallback* callback);

  // Overridden from BaseSessionService because we want some UMA reporting on
  // session update activities.
  virtual void Save();

 private:
  typedef std::map<SessionID::id_type, std::pair<int, int> > IdToRange;
  typedef std::map<SessionID::id_type, SessionTab*> IdToSessionTab;
  typedef std::map<SessionID::id_type, SessionWindow*> IdToSessionWindow;


  // These types mirror Browser::Type, but are re-defined here because these
  // specific enumeration _values_ are written into the session database and
  // are needed to maintain forward compatibility.
  // Note that we only store browsers of type TYPE_TABBED and TYPE_POPUP.
  enum WindowType {
    TYPE_TABBED = 0,
    TYPE_POPUP = 1
  };

  void Init();

  // Implementation of RestoreIfNecessary. If |browser| is non-null and we need
  // to restore, the tabs are added to it, otherwise a new browser is created.
  bool RestoreIfNecessary(const std::vector<GURL>& urls_to_open,
                          Browser* browser);

  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Sets the application extension id of the specified tab.
  void SetTabExtensionAppID(const SessionID& window_id,
                            const SessionID& tab_id,
                            const std::string& extension_app_id);

  // Methods to create the various commands. It is up to the caller to delete
  // the returned the SessionCommand* object.
  SessionCommand* CreateSetSelectedTabInWindow(const SessionID& window_id,
                                               int index);

  SessionCommand* CreateSetTabWindowCommand(const SessionID& window_id,
                                            const SessionID& tab_id);

  SessionCommand* CreateSetWindowBoundsCommand(const SessionID& window_id,
                                               const gfx::Rect& bounds,
                                               ui::WindowShowState show_state);

  SessionCommand* CreateSetTabIndexInWindowCommand(const SessionID& tab_id,
                                                   int new_index);

  SessionCommand* CreateTabClosedCommand(SessionID::id_type tab_id);

  SessionCommand* CreateWindowClosedCommand(SessionID::id_type tab_id);

  SessionCommand* CreateSetSelectedNavigationIndexCommand(
      const SessionID& tab_id,
      int index);

  SessionCommand* CreateSetWindowTypeCommand(const SessionID& window_id,
                                             WindowType type);

  SessionCommand* CreatePinnedStateCommand(const SessionID& tab_id,
                                           bool is_pinned);

  // Callback from the backend for getting the commands from the save file.
  // Converts the commands in SessionWindows and notifies the real callback.
  void OnGotSessionCommands(
      Handle handle,
      scoped_refptr<InternalGetCommandsRequest> request);

  // Converts the commands into SessionWindows. On return any valid
  // windows are added to valid_windows. It is up to the caller to delete
  // the windows added to valid_windows.
  //
  // If ignore_recent_closes is true, any window/tab closes within in a certain
  // time frame are ignored.
  void RestoreSessionFromCommands(const std::vector<SessionCommand*>& commands,
                                  std::vector<SessionWindow*>* valid_windows);

  // Iterates through the vector updating the selected_tab_index of each
  // SessionWindow based on the actual tabs that were restored.
  void UpdateSelectedTabIndex(std::vector<SessionWindow*>* windows);

  // Returns the window in windows with the specified id. If a window does
  // not exist, one is created.
  SessionWindow* GetWindow(SessionID::id_type window_id,
                           IdToSessionWindow* windows);

  // Returns the tab with the specified id in tabs. If a tab does not exist,
  // it is created.
  SessionTab* GetTab(SessionID::id_type tab_id,
                     IdToSessionTab* tabs);

  // Returns an iterator into navigations pointing to the navigation whose
  // index matches |index|. If no navigation index matches |index|, the first
  // navigation with an index > |index| is returned.
  //
  // This assumes the navigations are ordered by index in ascending order.
  std::vector<TabNavigation>::iterator FindClosestNavigationWithIndex(
      std::vector<TabNavigation>* navigations,
      int index);

  // Does the following:
  // . Deletes and removes any windows with no tabs or windows with types other
  //   than tabbed_browser or browser. NOTE: constrained windows that have
  //   been dragged out are of type browser. As such, this preserves any dragged
  //   out constrained windows (aka popups that have been dragged out).
  // . Sorts the tabs in windows with valid tabs based on the tabs
  //   visual order, and adds the valid windows to windows.
  void SortTabsBasedOnVisualOrderAndPrune(
      std::map<int, SessionWindow*>* windows,
      std::vector<SessionWindow*>* valid_windows);

  // Adds tabs to their parent window based on the tab's window_id. This
  // ignores tabs with no navigations.
  void AddTabsToWindows(std::map<int, SessionTab*>* tabs,
                        std::map<int, SessionWindow*>* windows);

  // Creates tabs and windows from the specified commands. The created tabs
  // and windows are added to |tabs| and |windows| respectively. It is up to
  // the caller to delete the tabs and windows added to |tabs| and |windows|.
  //
  // This does NOT add any created SessionTabs to SessionWindow.tabs, that is
  // done by AddTabsToWindows.
  bool CreateTabsAndWindows(const std::vector<SessionCommand*>& data,
                            std::map<int, SessionTab*>* tabs,
                            std::map<int, SessionWindow*>* windows);

  // Adds commands to commands that will recreate the state of the specified
  // tab. This adds at most kMaxNavigationCountToPersist navigations (in each
  // direction from the current navigation index).
  // A pair is added to tab_to_available_range indicating the range of
  // indices that were written.
  void BuildCommandsForTab(
      const SessionID& window_id,
      TabContentsWrapper* tab,
      int index_in_window,
      bool is_pinned,
      std::vector<SessionCommand*>* commands,
      IdToRange* tab_to_available_range);

  // Adds commands to create the specified browser, and invokes
  // BuildCommandsForTab for each of the tabs in the browser. This ignores
  // any tabs not in the profile we were created with.
  void BuildCommandsForBrowser(
      Browser* browser,
      std::vector<SessionCommand*>* commands,
      IdToRange* tab_to_available_range,
      std::set<SessionID::id_type>* windows_to_track);

  // Iterates over all the known browsers invoking BuildCommandsForBrowser.
  // This only adds browsers that should be tracked
  // (should_track_changes_for_browser_type returns true). All browsers that
  // are tracked are added to windows_to_track (as long as it is non-null).
  void BuildCommandsFromBrowsers(
      std::vector<SessionCommand*>* commands,
      IdToRange* tab_to_available_range,
      std::set<SessionID::id_type>* windows_to_track);

  // Schedules a reset. A reset means the contents of the file are recreated
  // from the state of the browser.
  void ScheduleReset();

  // Searches for a pending command that can be replaced with command.
  // If one is found, pending command is removed, command is added to
  // the pending commands and true is returned.
  bool ReplacePendingCommand(SessionCommand* command);

  // Schedules the specified command. This method takes ownership of the
  // command.
  virtual void ScheduleCommand(SessionCommand* command);

  // Converts all pending tab/window closes to commands and schedules them.
  void CommitPendingCloses();

  // Returns true if there is only one window open with a single tab that shares
  // our profile.
  bool IsOnlyOneTabLeft();

  // Returns true if there are open trackable browser windows whose ids do
  // match |window_id| with our profile. A trackable window is a window from
  // which |should_track_changes_for_browser_type| returns true. See
  // |should_track_changes_for_browser_type| for details.
  bool HasOpenTrackableBrowsers(const SessionID& window_id);

  // Returns true if changes to tabs in the specified window should be tracked.
  bool ShouldTrackChangesToWindow(const SessionID& window_id);

  // Returns true if we track changes to the specified browser type.
  static bool should_track_changes_for_browser_type(Browser::Type type);

  // Returns true if we should record a window close as pending.
  // |has_open_trackable_browsers_| must be up-to-date before calling this.
  bool should_record_close_as_pending() const {
    // When this is called, the browser window being closed is still open, hence
    // still in the browser list. If there is a browser window other than the
    // one being closed but no trackable windows, then the others must be App
    // windows or similar. In this case, we record the close as pending.
    return !has_open_trackable_browsers_ &&
        (!browser_defaults::kBrowserAliveWithNoWindows ||
         BrowserList::size() > 1);
  }

  // Call when certain session relevant notifications
  // (tab_closed, nav_list_pruned) occur.  In addition, this is
  // currently called when Save() is called to compare how often the
  // session data is currently saved verses when we may want to save it.
  // It records the data in UMA stats.
  void RecordSessionUpdateHistogramData(int type,
    base::TimeTicks* last_updated_time);

  // Helper methods to record the histogram data
  void RecordUpdatedTabClosed(base::TimeDelta delta, bool use_long_period);
  void RecordUpdatedNavListPruned(base::TimeDelta delta, bool use_long_period);
  void RecordUpdatedNavEntryCommit(base::TimeDelta delta, bool use_long_period);
  void RecordUpdatedSaveTime(base::TimeDelta delta, bool use_long_period);
  void RecordUpdatedSessionNavigationOrTab(base::TimeDelta delta,
                                           bool use_long_period);

  // Convert back/forward between the Browser and SessionService DB window
  // types.
  static WindowType WindowTypeForBrowserType(Browser::Type type);
  static Browser::Type BrowserTypeForWindowType(WindowType type);

  NotificationRegistrar registrar_;

  // Maps from session tab id to the range of navigation entries that has
  // been written to disk.
  //
  // This is only used if not all the navigation entries have been
  // written.
  IdToRange tab_to_available_range_;

  // When the user closes the last window, where the last window is the
  // last tabbed browser and no more tabbed browsers are open with the same
  // profile, the window ID is added here. These IDs are only committed (which
  // marks them as closed) if the user creates a new tabbed browser.
  typedef std::set<SessionID::id_type> PendingWindowCloseIDs;
  PendingWindowCloseIDs pending_window_close_ids_;

  // Set of tabs that have been closed by way of the last window or last tab
  // closing, but not yet committed.
  typedef std::set<SessionID::id_type> PendingTabCloseIDs;
  PendingTabCloseIDs pending_tab_close_ids_;

  // When a window other than the last window (see description of
  // pending_window_close_ids) is closed, the id is added to this set.
  typedef std::set<SessionID::id_type> WindowClosingIDs;
  WindowClosingIDs window_closing_ids_;

  // Set of windows we're tracking changes to. This is only browsers that
  // return true from should_track_changes_for_browser_type.
  typedef std::set<SessionID::id_type> WindowsTracking;
  WindowsTracking windows_tracking_;

  // Are there any open trackable browsers?
  bool has_open_trackable_browsers_;

  // If true and a new tabbed browser is created and there are no opened tabbed
  // browser (has_open_trackable_browsers_ is false), then the current session
  // is made the last session. See description above class for details on
  // current/last session.
  bool move_on_new_browser_;

  // Used for reporting frequency of session altering operations.
  base::TimeTicks last_updated_tab_closed_time_;
  base::TimeTicks last_updated_nav_list_pruned_time_;
  base::TimeTicks last_updated_nav_entry_commit_time_;
  base::TimeTicks last_updated_save_time_;

  // Constants used in calculating histogram data.
  const base::TimeDelta save_delay_in_millis_;
  const base::TimeDelta save_delay_in_mins_;
  const base::TimeDelta save_delay_in_hrs_;

  DISALLOW_COPY_AND_ASSIGN(SessionService);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_SERVICE_H_
