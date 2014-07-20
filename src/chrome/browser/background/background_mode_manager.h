// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_BACKGROUND_MODE_MANAGER_H_
#define CHROME_BROWSER_BACKGROUND_BACKGROUND_MODE_MANAGER_H_
#pragma once

#include <map>

#include "base/gtest_prod_util.h"
#include "chrome/browser/background/background_application_list_model.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;
class CommandLine;
class Extension;
class PrefService;
class Profile;
class ProfileInfoCache;
class StatusIcon;
class StatusTray;

// BackgroundModeManager is responsible for switching Chrome into and out of
// "background mode" and for providing UI for the user to exit Chrome when there
// are no open browser windows.
//
// Chrome enters background mode whenever there is an application with the
// "background" permission installed. This class monitors the set of
// installed/loaded extensions to ensure that Chrome enters/exits background
// mode at the appropriate time.
//
// When Chrome is in background mode, it will continue running even after the
// last browser window is closed, until the user explicitly exits the app.
// Additionally, when in background mode, Chrome will launch on OS login with
// no open windows to allow apps with the "background" permission to run in the
// background.
class BackgroundModeManager
    : public NotificationObserver,
      public BackgroundApplicationListModel::Observer,
      public ProfileKeyedService,
      public ui::SimpleMenuModel::Delegate {
 public:
  BackgroundModeManager(CommandLine* command_line,
                        ProfileInfoCache* profile_cache);
  virtual ~BackgroundModeManager();

  static void RegisterPrefs(PrefService* prefs);

  virtual void RegisterProfile(Profile* profile);

  // Returns true if background mode is permanently disabled for this Chrome
  // session.
  static bool IsBackgroundModePermanentlyDisabled(
      const CommandLine* command_line);

  static void LaunchBackgroundApplication(Profile* profile,
                                          const Extension* extension);

 private:
  friend class TestBackgroundModeManager;
  friend class BackgroundModeManagerTest;
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundAppLoadUnload);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundLaunchOnStartup);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundAppInstallUninstallWhileDisabled);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           EnableAfterBackgroundAppInstall);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           MultiProfile);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           ProfileInfoCacheStorage);

  class BackgroundModeData : public ui::SimpleMenuModel::Delegate {
   public:
    explicit BackgroundModeData(
        int command_id,
        Profile* profile,
        BackgroundModeManager* background_mode_manager);
    virtual ~BackgroundModeData();

    // The cached list of BackgroundApplications.
    scoped_ptr<BackgroundApplicationListModel> applications_;

    // Overrides from SimpleMenuModel::Delegate implementation.
    virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
    virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
    virtual bool GetAcceleratorForCommandId(int command_id,
                                            ui::Accelerator* accelerator)
                                            OVERRIDE;
    virtual void ExecuteCommand(int command_id) OVERRIDE;

    // Returns a browser window, or creates one if none are open. Used by
    // operations (like displaying the preferences dialog) that require a
    // Browser window.
    Browser* GetBrowserWindow();

    // Returns the number of background apps for this profile.
    int GetBackgroundAppCount() const;

    // Builds the profile specific parts of the menu. The menu passed in may
    // be a submenu in the case of multi-profiles or the main menu in the case
    // of the single profile case. If containing_menu is valid, we will add
    // menu as a submenu to it.
    void BuildProfileMenu(ui::SimpleMenuModel* menu,
                          ui::SimpleMenuModel* containing_menu);

    // Used for sorting BackgroundModeData*s.
    static bool BackgroundModeDataCompare(const BackgroundModeData* bmd1,
                                          const BackgroundModeData* bmd2);

   private:
    // Command id for the sub menu for this BackgroundModeData.
    int command_id_;

    // Name associated with this profile which is used to label its submenu.
    string16 name_;

    // The profile associated with this background app data.
    Profile* profile_;

    // The background mode manager which owns this BackgroundModeData.
    BackgroundModeManager* background_mode_manager_;
  };

  // Ideally we would want our BackgroundModeData to be scoped_ptrs,
  // but since maps copy their entries, we can't used scoped_ptrs.
  // Similarly, we can't just have a map of BackgroundModeData objects,
  // since BackgroundModeData contains a scoped_ptr which once again
  // can't be copied. So rather than using BackgroundModeData* which
  // we'd have to remember to delete, we use the ref-counted linked_ptr
  // which is similar to a shared_ptr.
  typedef linked_ptr<BackgroundModeData> BackgroundModeInfo;

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // BackgroundApplicationListModel::Observer implementation.
  virtual void OnApplicationDataChanged(const Extension* extension,
                                        Profile* profile) OVERRIDE;
  virtual void OnApplicationListChanged(Profile* profile) OVERRIDE;

  // Overrides from SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          ui::Accelerator* accelerator)
                                          OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

  // Invoked when an extension is installed so we can ensure that
  // launch-on-startup is enabled if appropriate. |extension| can be NULL when
  // called from unit tests.
  void OnBackgroundAppInstalled(const Extension* extension);

  // Called to make sure that our launch-on-startup mode is properly set.
  // (virtual so we can override for tests).
  virtual void EnableLaunchOnStartup(bool should_launch);

  // Invoked when a background app is installed so we can display a
  // platform-specific notification to the user.
  void DisplayAppInstalledNotification(const Extension* extension);

  // Invoked to put Chrome in KeepAlive mode - chrome runs in the background
  // and has a status bar icon.
  void StartBackgroundMode();

  // Invoked to create the status icon if any profiles currently running
  // background apps so that there is a way to exit Chrome.
  void InitStatusTrayIcon();

  // Invoked to take Chrome out of KeepAlive mode - chrome stops running in
  // the background and removes its status bar icon.
  void EndBackgroundMode();

  // If --no-startup-window is passed, BackgroundModeManager will manually keep
  // chrome running while waiting for apps to load. This is called when we no
  // longer need to do this (either because the user has chosen to exit chrome
  // manually, or all apps have been loaded).
  void EndKeepAliveForStartup();

  // Return an appropriate name for a Preferences menu entry.  Preferences is
  // sometimes called Options or Settings.
  string16 GetPreferencesMenuLabel();

  // Create a status tray icon to allow the user to shutdown Chrome when running
  // in background mode. Virtual to enable testing.
  virtual void CreateStatusTrayIcon();

  // Removes the status tray icon because we are exiting background mode.
  // Virtual to enable testing.
  virtual void RemoveStatusTrayIcon();

  // Create a context menu, or replace/update an existing context menu, for the
  // status tray icon which, among other things, allows the user to shutdown
  // Chrome when running in background mode. All profiles are listed under
  // the one context menu.
  virtual void UpdateStatusTrayIconContextMenu();

  // Returns the BackgroundModeData associated with this profile. If it does
  // not exist, returns NULL.
  BackgroundModeManager::BackgroundModeData* GetBackgroundModeData(
      Profile* const profile) const;

  // Returns true if the "Let chrome run in the background" pref is checked.
  // (virtual to allow overriding in tests).
  virtual bool IsBackgroundModePrefEnabled() const;

  // Turns off background mode if it's currently enabled.
  void DisableBackgroundMode();

  // Turns on background mode if it's currently disabled.
  void EnableBackgroundMode();

  // Returns the number of background apps in the system (virtual to allow
  // overriding in unit tests).
  virtual int GetBackgroundAppCount() const;

  // Returns the number of background apps for a profile.
  virtual int GetBackgroundAppCountForProfile(Profile* const profile) const;

  // Reference to the profile info cache. It is used to update the background
  // app status of profiles when they open/close background apps.
  ProfileInfoCache* profile_cache_;

  // Registrars for managing our change observers.
  NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_registrar_;

  // The profile-keyed data for this background mode manager. Keyed on profile.
  std::map<Profile*, BackgroundModeInfo> background_mode_data_;

  // Reference to our status tray. If null, the platform doesn't support status
  // icons.
  StatusTray* status_tray_;

  // Reference to our status icon (if any) - owned by the StatusTray.
  StatusIcon* status_icon_;

  // Reference to our status icon's context menu (if any) - owned by the
  // status_icon_.
  ui::SimpleMenuModel* context_menu_;

  // Set to true when we are running in background mode. Allows us to track our
  // current background state so we can take the appropriate action when the
  // user disables/enables background mode via preferences.
  bool in_background_mode_;

  // Set when we are keeping chrome running during the startup process - this
  // is required when running with the --no-startup-window flag, as otherwise
  // chrome would immediately exit due to having no open windows.
  bool keep_alive_for_startup_;

  // Set to true when Chrome is running with the --keep-alive-for-test flag
  // (used for testing background mode without having to install a background
  // app).
  bool keep_alive_for_test_;

  // Provides a command id for each profile as they are created.
  int current_command_id_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundModeManager);
};

#endif  // CHROME_BROWSER_BACKGROUND_BACKGROUND_MODE_MANAGER_H_
