// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EULA_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EULA_VIEW_H_
#pragma once

#include "chrome/browser/chromeos/login/message_bubble.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "ui/gfx/native_widget_types.h"
#include "views/controls/button/button.h"
#include "views/controls/link_listener.h"
#include "views/view.h"

namespace views {

class Checkbox;
class Label;
class Link;
class NativeTextButton;

}  // namespace views

class DOMView;

namespace chromeos {

class HelpAppLauncher;
class TpmInfoView;
class ViewsEulaScreenActor;

// Delegate for TabContents that will show EULA.
// Blocks context menu and other actions.
class EULATabContentsDelegate : public TabContentsDelegate {
 public:
  EULATabContentsDelegate() {}
  virtual ~EULATabContentsDelegate() {}

 protected:
  // TabContentsDelegate implementation:
  virtual bool IsPopup(TabContents* source);
  virtual bool ShouldAddNavigationToHistory(
      const history::HistoryAddPageArgs& add_page_args,
      content::NavigationType navigation_type);
  virtual bool HandleContextMenu(const ContextMenuParams& params);

 private:
  DISALLOW_COPY_AND_ASSIGN(EULATabContentsDelegate);
};

class EulaView
    : public views::View,
      public views::ButtonListener,
      public views::LinkListener,
      public MessageBubbleDelegate,
      public EULATabContentsDelegate {
 public:
  explicit EulaView(ViewsEulaScreenActor* actor);
  virtual ~EulaView();

  // Initialize view controls and layout.
  void Init();

  // Update strings from the resources. Executed on language change.
  void UpdateLocalizedStrings();

  // Returns the state of usage stats checkbox.
  bool IsUsageStatsChecked() const;

  // Shows given TPM password.
  void ShowTpmPassword(const std::string& tpm_password);

  // This method is called by the TpmInfoView dialog when it is closed.
  void OnTpmInfoViewClosed(TpmInfoView* tpm_info_view);

 protected:
  // views::View implementation.
  virtual void OnLocaleChanged() OVERRIDE;

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // views::LinkListener implementation.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

 private:
  // views::View implementation.
  virtual bool SkipDefaultKeyEventProcessing(const views::KeyEvent& e) OVERRIDE;
  virtual bool OnKeyPressed(const views::KeyEvent& e) OVERRIDE;

  // TabContentsDelegate implementation.
  virtual void NavigationStateChanged(const TabContents* contents,
                                      unsigned changed_flags) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;

  // Loads specified URL to the specified DOMView and updates specified
  // label with its title.
  void LoadEulaView(DOMView* eula_view,
                    views::Label* eula_label,
                    const GURL& eula_url);

  // Overridden from views::BubbleDelegate.
  virtual void BubbleClosing(Bubble* bubble, bool closed_by_escape) OVERRIDE;
  virtual bool CloseOnEscape() OVERRIDE;
  virtual bool FadeInOnShow() OVERRIDE;
  virtual void OnLinkActivated(size_t index) OVERRIDE;

  // Dialog controls.
  views::Label* google_eula_label_;
  DOMView* google_eula_view_;
  views::Checkbox* usage_statistics_checkbox_;
  views::Link* learn_more_link_;
  views::Label* oem_eula_label_;
  DOMView* oem_eula_view_;
  views::Link* system_security_settings_link_;
  views::NativeTextButton* back_button_;
  views::NativeTextButton* continue_button_;

  // Keeps pointer to the active TpmInfoView if it is opened, NULL if is not.
  TpmInfoView* tpm_info_view_;

  ViewsEulaScreenActor* actor_;

  // URL of the OEM EULA page (on disk).
  GURL oem_eula_page_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  // Pointer to shown message bubble. We don't need to delete it because
  // it will be deleted on bubble closing.
  MessageBubble* bubble_;

  DISALLOW_COPY_AND_ASSIGN(EulaView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EULA_VIEW_H_
