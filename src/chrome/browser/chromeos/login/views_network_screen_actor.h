// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_NETWORK_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_NETWORK_SCREEN_ACTOR_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "base/task.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/keyboard_switch_menu.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/login/network_screen_actor.h"
#include "chrome/browser/chromeos/login/network_selection_view.h"
#include "chrome/browser/chromeos/login/view_screen.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "views/controls/button/button.h"

namespace chromeos {

class HelpAppLauncher;
class ViewScreenDelegate;

// Views-specific implementation of NetworkScreenActor. Hosts
// NetworkSelectionView.
class ViewsNetworkScreenActor : public ViewScreen<NetworkSelectionView>,
                                public MessageBubbleDelegate,
                                public NetworkScreenActor,
                                public views::ButtonListener {
 public:
  explicit ViewsNetworkScreenActor(ViewScreenDelegate* delegate);
  virtual ~ViewsNetworkScreenActor();

  // NetworkScreenActor implementation:
  virtual void SetDelegate(Delegate* screen) OVERRIDE;
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual gfx::Size GetScreenSize() const OVERRIDE;
  virtual void ShowError(const string16& message) OVERRIDE;
  virtual void ClearErrors() OVERRIDE;
  virtual void ShowConnectingStatus(
      bool connecting,
      const string16& network_id) OVERRIDE;
  virtual void EnableContinue(bool enabled) OVERRIDE;

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // Returns true if a bubble with error is shown currently to the user.
  bool IsErrorShown() const;

  // TODO(avayvod): Get rid of the dependency on the menus by moving it
  // either to the view or here.
  LanguageSwitchMenu* language_switch_menu() {
    return &language_switch_menu_;
  }

  KeyboardSwitchMenu* keyboard_switch_menu() {
    return &keyboard_switch_menu_;
  }

  // For tests.
  virtual bool IsContinueEnabled() const;
  virtual bool IsConnecting() const;

 private:
  // Overridden views::BubbleDelegate.
  virtual void BubbleClosing(Bubble* bubble, bool closed_by_escape) OVERRIDE;
  virtual bool CloseOnEscape() OVERRIDE;
  virtual bool FadeInOnShow() OVERRIDE;
  virtual void OnLinkActivated(size_t index) OVERRIDE;

  // ViewScreen implementation:
  virtual void CreateView() OVERRIDE;
  virtual NetworkSelectionView* AllocateView() OVERRIDE;

  LanguageSwitchMenu language_switch_menu_;
  KeyboardSwitchMenu keyboard_switch_menu_;

  // Pointer to shown message bubble. We don't need to delete it because
  // it will be deleted on bubble closing.
  MessageBubble* bubble_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  // Listener for continue button to be pressed.
  Delegate* screen_;

  DISALLOW_COPY_AND_ASSIGN(ViewsNetworkScreenActor);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_NETWORK_SCREEN_ACTOR_H_
