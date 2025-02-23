// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_MODAL_DIALOGS_JS_MODAL_DIALOG_H_
#define CHROME_BROWSER_UI_APP_MODAL_DIALOGS_JS_MODAL_DIALOG_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"
#include "content/browser/javascript_dialogs.h"

namespace content {
class JavaScriptDialogDelegate;
}

namespace IPC {
class Message;
}

// Extra data for JavaScript dialogs to add Chrome-only features.
class ChromeJavaScriptDialogExtraData {
 public:
  ChromeJavaScriptDialogExtraData();

  // The time that the last JavaScript dialog was dismissed.
  base::TimeTicks last_javascript_message_dismissal_;

  // True if the user has decided to block future JavaScript dialogs.
  bool suppress_javascript_messages_;
};

// A controller + model class for JavaScript alert, confirm, prompt, and
// onbeforeunload dialog boxes.
class JavaScriptAppModalDialog : public AppModalDialog {
 public:
  JavaScriptAppModalDialog(content::JavaScriptDialogDelegate* delegate,
                           ChromeJavaScriptDialogExtraData* extra_data,
                           const string16& title,
                           int dialog_flags,
                           const string16& message_text,
                           const string16& default_prompt_text,
                           bool display_suppress_checkbox,
                           bool is_before_unload_dialog,
                           IPC::Message* reply_msg);
  virtual ~JavaScriptAppModalDialog();

  // Overridden from AppModalDialog:
  virtual NativeAppModalDialog* CreateNativeDialog() OVERRIDE;
  virtual bool IsJavaScriptModalDialog() OVERRIDE;
  virtual void Invalidate() OVERRIDE;
  virtual content::JavaScriptDialogDelegate* delegate() const OVERRIDE;

  // Callbacks from NativeDialog when the user accepts or cancels the dialog.
  void OnCancel(bool suppress_js_messages);
  void OnAccept(const string16& prompt_text, bool suppress_js_messages);

  // NOTE: This is only called under Views, and should be removed. Any critical
  // work should be done in OnCancel or OnAccept. See crbug.com/63732 for more.
  void OnClose();

  // Used only for testing. The dialog will use the given text when notifying
  // its delegate instead of whatever the UI reports.
  void SetOverridePromptText(const string16& prompt_text);

  // Accessors
  int dialog_flags() const { return dialog_flags_; }
  string16 message_text() const { return message_text_; }
  string16 default_prompt_text() const { return default_prompt_text_; }
  bool display_suppress_checkbox() const { return display_suppress_checkbox_; }
  bool is_before_unload_dialog() const { return is_before_unload_dialog_; }

 private:
  // Notifies the delegate with the result of the dialog.
  void NotifyDelegate(bool success, const string16& prompt_text,
                      bool suppress_js_messages);

  // The extra Chrome-only data associated with the delegate_.
  ChromeJavaScriptDialogExtraData* extra_data_;

  // Information about the message box is held in the following variables.
  int dialog_flags_;
  string16 message_text_;
  string16 default_prompt_text_;
  bool display_suppress_checkbox_;
  bool is_before_unload_dialog_;
  IPC::Message* reply_msg_;

  // Used only for testing. Specifies alternative prompt text that should be
  // used when notifying the delegate, if |use_override_prompt_text_| is true.
  string16 override_prompt_text_;
  bool use_override_prompt_text_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptAppModalDialog);
};

#endif  // CHROME_BROWSER_UI_APP_MODAL_DIALOGS_JS_MODAL_DIALOG_H_
