// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_modal_dialogs/js_modal_dialog.h"

#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"
#include "ui/base/text/text_elider.h"

namespace {

// Control maximum sizes of various texts passed to us from javascript.
#if defined(OS_POSIX) && !defined(OS_MACOSX)
// Two-dimensional eliding.  Reformat the text of the message dialog
// inserting line breaks because otherwise a single long line can overflow
// the message dialog (and crash/hang the GTK, depending on the version).
const int kMessageTextMaxRows = 32;
const int kMessageTextMaxCols = 132;
const int kDefaultPromptMaxRows = 24;
const int kDefaultPromptMaxCols = 132;
void EnforceMaxTextSize(const string16& in_string, string16* out_string) {
  ui::ElideRectangleString(in_string, kMessageTextMaxRows,
                           kMessageTextMaxCols, false, out_string);
}
void EnforceMaxPromptSize(const string16& in_string, string16* out_string) {
  ui::ElideRectangleString(in_string, kDefaultPromptMaxRows,
                           kDefaultPromptMaxCols, false, out_string);
}
#else
// One-dimensional eliding.  Trust the window system to break the string
// appropriately, but limit its overall length to something reasonable.
const int kMessageTextMaxSize = 3000;
const int kDefaultPromptMaxSize = 2000;
void EnforceMaxTextSize(const string16& in_string, string16* out_string) {
  ui::ElideString(in_string, kMessageTextMaxSize, out_string);
}
void EnforceMaxPromptSize(const string16& in_string, string16* out_string) {
  ui::ElideString(in_string, kDefaultPromptMaxSize, out_string);
}
#endif

}  // namespace

ChromeJavaScriptDialogExtraData::ChromeJavaScriptDialogExtraData()
    : suppress_javascript_messages_(false) {
}

JavaScriptAppModalDialog::JavaScriptAppModalDialog(
    content::JavaScriptDialogDelegate* delegate,
    ChromeJavaScriptDialogExtraData* extra_data,
    const string16& title,
    int dialog_flags,
    const string16& message_text,
    const string16& default_prompt_text,
    bool display_suppress_checkbox,
    bool is_before_unload_dialog,
    IPC::Message* reply_msg)
    : AppModalDialog(delegate, title),
      extra_data_(extra_data),
      dialog_flags_(dialog_flags),
      display_suppress_checkbox_(display_suppress_checkbox),
      is_before_unload_dialog_(is_before_unload_dialog),
      reply_msg_(reply_msg),
      use_override_prompt_text_(false) {
  EnforceMaxTextSize(message_text, &message_text_);
  EnforceMaxPromptSize(default_prompt_text, &default_prompt_text_);
}

JavaScriptAppModalDialog::~JavaScriptAppModalDialog() {
}

NativeAppModalDialog* JavaScriptAppModalDialog::CreateNativeDialog() {
  gfx::NativeWindow parent_window = delegate_->GetDialogRootWindow();
  return NativeAppModalDialog::CreateNativeJavaScriptPrompt(this,
                                                            parent_window);
}

bool JavaScriptAppModalDialog::IsJavaScriptModalDialog() {
  return true;
}

void JavaScriptAppModalDialog::Invalidate() {
  if (!valid_)
    return;

  valid_ = false;
  delegate_ = NULL;
  if (native_dialog_)
    CloseModalDialog();
}

content::JavaScriptDialogDelegate* JavaScriptAppModalDialog::delegate() const {
  return static_cast<content::JavaScriptDialogDelegate*>(delegate_);
}

void JavaScriptAppModalDialog::OnCancel(bool suppress_js_messages) {
  // If we are shutting down and this is an onbeforeunload dialog, cancel the
  // shutdown.
  if (is_before_unload_dialog_)
    browser_shutdown::SetTryingToQuit(false);

  // We need to do this before WM_DESTROY (WindowClosing()) as any parent frame
  // will receive its activation messages before this dialog receives
  // WM_DESTROY. The parent frame would then try to activate any modal dialogs
  // that were still open in the ModalDialogQueue, which would send activation
  // back to this one. The framework should be improved to handle this, so this
  // is a temporary workaround.
  CompleteDialog();

  NotifyDelegate(false, string16(), suppress_js_messages);
}

void JavaScriptAppModalDialog::OnAccept(const string16& prompt_text,
                                        bool suppress_js_messages) {
  string16 prompt_text_to_use = prompt_text;
  // This is only for testing.
  if (use_override_prompt_text_)
    prompt_text_to_use = override_prompt_text_;

  CompleteDialog();
  NotifyDelegate(true, prompt_text_to_use, suppress_js_messages);
}

void JavaScriptAppModalDialog::OnClose() {
  NotifyDelegate(false, string16(), false);
}

void JavaScriptAppModalDialog::SetOverridePromptText(
    const string16& override_prompt_text) {
  override_prompt_text_ = override_prompt_text;
  use_override_prompt_text_ = true;
}

void JavaScriptAppModalDialog::NotifyDelegate(bool success,
                                              const string16& user_input,
                                              bool suppress_js_messages) {
  if (!valid_)
    return;

  delegate()->OnDialogClosed(reply_msg_, success, user_input);

  extra_data_->last_javascript_message_dismissal_ = base::TimeTicks::Now();
  extra_data_->suppress_javascript_messages_ = suppress_js_messages;

  // On Views, we can end up coming through this code path twice :(.
  // See crbug.com/63732.
  valid_ = false;
}
