// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_CAPS_LOCK_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_CAPS_LOCK_MENU_BUTTON_H_
#pragma once

#include <string>

#include "base/timer.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "chrome/browser/chromeos/system_key_event_listener.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/common/notification_observer.h"
#include "views/controls/menu/menu_delegate.h"
#include "views/controls/menu/view_menu_delegate.h"

class Bubble;

namespace views {
class MenuRunner;
}

namespace chromeos {

class StatusAreaHost;

// A class for the button in the status area which alerts the user when caps
// lock is active.
class CapsLockMenuButton : public NotificationObserver,
                           public StatusAreaButton,
                           public views::MenuDelegate,
                           public views::ViewMenuDelegate,
                           public SystemKeyEventListener::CapsLockObserver {
 public:
  explicit CapsLockMenuButton(StatusAreaHost* host);
  virtual ~CapsLockMenuButton();

  // views::View implementation.
  virtual void OnLocaleChanged();

  // views::MenuDelegate implementation.
  virtual string16 GetLabel(int id) const;
  virtual bool IsCommandEnabled(int id) const;

  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* unused_source, const gfx::Point& pt);

  // SystemKeyEventListener::CapsLockObserver implementation
  virtual void OnCapsLockChange(bool enabled);

  // NotificationObserver implementation
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Updates the accessible name.
  void UpdateAccessibleName();

  // Gets the text for the drop-down menu and bubble.
  string16 GetText() const;

  // Updates the button from the current state.
  void UpdateUIFromCurrentCapsLock(bool enabled);

 private:
  class StatusView;

  // Returns true if the Search key is assigned to Caps Lock.
  bool HasCapsLock() const;

  bool IsMenuShown() const;
  void HideMenu();

  bool IsBubbleShown() const;
  void MaybeShowBubble();
  void CreateAndShowBubble();
  void HideBubble();

  PrefService* prefs_;
  IntegerPrefMember remap_search_key_to_;

  // The currently showing status view. NULL if menu is not being displayed.
  StatusView* status_;
  // If non-null the menu is showing.
  scoped_ptr<views::MenuRunner> menu_runner_;

  // The currently showing bubble. NULL if bubble is not being displayed.
  Bubble* bubble_;
  // If true, |bubble_| is shown when both shift keys are pressed.
  bool should_show_bubble_;
  // # of times |bubble_| is shown.
  size_t bubble_count_;
  // TODO(yusukes): Save should_show_bubble_ and bubble_count_ in Preferences.

  // The timer for hiding the bubble.
  base::OneShotTimer<CapsLockMenuButton> bubble_timer_;

  DISALLOW_COPY_AND_ASSIGN(CapsLockMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_CAPS_LOCK_MENU_BUTTON_H_
