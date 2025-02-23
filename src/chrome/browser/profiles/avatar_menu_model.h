// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_AVATAR_MENU_MODEL_H_
#define CHROME_BROWSER_PROFILES_AVATAR_MENU_MODEL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class AvatarMenuModelObserver;
class Browser;
class ProfileInfoInterface;

namespace gfx {
class Image;
}

// This class is the model for the menu-like interface that appears when the
// avatar icon is clicked in the browser window frame. This class will notify
// its observer when the backend data changes, and the controller/view for this
// model should forward actions back to it in response to user events.
class AvatarMenuModel : public NotificationObserver {
 public:
  // Represents an item in the menu.
  struct Item {
    Item(size_t model_index, const gfx::Image& icon);
    ~Item();

    // The icon to be displayed next to the item.
    const gfx::Image& icon;

    // Whether or not the current browser is using this profile.
    bool active;

    // The name of this profile.
    string16 name;

    // A string representing the sync state of the profile.
    string16 sync_state;

    // The index in the |profile_cache| that this Item represents.
    size_t model_index;
  };

  // Constructor. No parameters can be NULL in practice. |browser| can be NULL
  // and a new one will be created if an action requires it.
  AvatarMenuModel(ProfileInfoInterface* profile_cache,
                  AvatarMenuModelObserver* observer,
                  Browser* browser);
  virtual ~AvatarMenuModel();

  // Actions performed by the view that the controller forwards back to the
  // model:
  // Opens a Browser with the specified profile in response to the user
  // selecting an item.
  void SwitchToProfile(size_t index);
  // Opens the profile settings in response to clicking the edit button next to
  // an item.
  void EditProfile(size_t index);
  // Creates a new profile.
  void AddNewProfile();

  // Gets the number of profiles.
  size_t GetNumberOfItems();

  // Gets the an Item at a specified index.
  const Item& GetItemAt(size_t index);

  // This model is also used for the always-present Mac system menubar. As the
  // last active browser changes, the model needs to update accordingly.
  void set_browser(Browser* browser) { browser_ = browser; }

  // NotificationObserver:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // True if avatar menu should be displayed.
  static bool ShouldShowAvatarMenu();

 private:
  // Rebuilds the menu from the cache and notifies the |observer_|.
  void RebuildMenu();

  // Clears the list of items, deleting each.
  void ClearMenu();

  // The cache that provides the profile information. Weak.
  ProfileInfoInterface* profile_info_;

  // The observer of this model, which is notified of changes. Weak.
  AvatarMenuModelObserver* observer_;

  // Browser in which this avatar menu resides. Weak.
  Browser* browser_;

  // List of built "menu items."
  std::vector<Item*> items_;

  // Listens for notifications from the ProfileInfoCache.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AvatarMenuModel);
};

#endif  // CHROME_BROWSER_PROFILES_AVATAR_MENU_MODEL_H_
