// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_HTML_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_HTML_DIALOG_H_
#pragma once

#include <string>

#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

namespace chromeos {

class BubbleFrameView;

// Launches html dialog during OOBE/Login with specified URL and title.
class LoginHtmlDialog : public HtmlDialogUIDelegate,
                        public NotificationObserver {
 public:
  // Delegate class to get notifications from the dialog.
  class Delegate {
  public:
    virtual ~Delegate() {}

    // Called when dialog has been closed.
    virtual void OnDialogClosed() = 0;
  };

  enum Style {
    STYLE_GENERIC, // Use generic CreateChromeWindow as a host.
    STYLE_BUBBLE   // Use chromeos::BubbleWindow as a host.
  };

  LoginHtmlDialog(Delegate* delegate,
                  gfx::NativeWindow parent_window,
                  const std::wstring& title,
                  const GURL& url,
                  Style style);
  virtual ~LoginHtmlDialog();

  // Shows created dialog.
  void Show();

  // Overrides default width/height for dialog.
  void SetDialogSize(int width, int height);

  void set_url(const GURL& url) { url_ = url; }

  bool is_open() const { return is_open_; }

 protected:
  // HtmlDialogUIDelegate implementation.
  virtual bool IsDialogModal() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(
      TabContents* source, bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;
  virtual bool HandleContextMenu(const ContextMenuParams& params) OVERRIDE;

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Notifications receiver.
  Delegate* delegate_;

  gfx::NativeWindow parent_window_;
  string16 title_;
  GURL url_;
  Style style_;
  NotificationRegistrar notification_registrar_;
  BubbleFrameView* bubble_frame_view_;
  bool is_open_;

  // Dialog display size.
  int width_;
  int height_;

  DISALLOW_COPY_AND_ASSIGN(LoginHtmlDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_HTML_DIALOG_H_
