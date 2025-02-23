// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_MODAL_DIALOGS_APP_MODAL_DIALOG_H_
#define CHROME_BROWSER_UI_APP_MODAL_DIALOGS_APP_MODAL_DIALOG_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/string16.h"
#include "build/build_config.h"

class NativeAppModalDialog;

namespace content {
class DialogDelegate;
}

// A controller+model base class for modal dialogs.
class AppModalDialog {
 public:
  // A union of data necessary to determine the type of message box to
  // show. |tab_contents| parameter is optional, if provided that tab will be
  // activated before the modal dialog is displayed.
  AppModalDialog(content::DialogDelegate* delegate, const string16& title);
  virtual ~AppModalDialog();

  // Called by the AppModalDialogQueue to show this dialog.
  void ShowModalDialog();

  // Called by the AppModalDialogQueue to activate the dialog.
  void ActivateModalDialog();

  // Closes the dialog if it is showing.
  void CloseModalDialog();

  // Completes dialog handling, shows next modal dialog from the queue.
  // TODO(beng): Get rid of this method.
  void CompleteDialog();

  // Dialog window title.
  string16 title() const { return title_; }

  NativeAppModalDialog* native_dialog() const { return native_dialog_; }

  // Creates an implementation of NativeAppModalDialog and shows it.
  // When the native dialog is closed, the implementation of
  // NativeAppModalDialog should call OnAccept or OnCancel to notify the
  // renderer of the user's action. The NativeAppModalDialog is also
  // expected to delete the AppModalDialog associated with it.
  void CreateAndShowDialog();

  // Returns true if the dialog is still valid. As dialogs are created they are
  // added to the AppModalDialogQueue. When the current modal dialog finishes
  // and it's time to show the next dialog in the queue IsValid is invoked.
  // If IsValid returns false the dialog is deleted and not shown.
  bool IsValid();

  // Methods overridable by AppModalDialog subclasses:

  // Invalidates the dialog, therefore causing it to not be shown when its turn
  // to be shown comes around.
  virtual void Invalidate();

  // Used only for testing. Returns whether the dialog is a JavaScript modal
  // dialog.
  virtual bool IsJavaScriptModalDialog();

  virtual content::DialogDelegate* delegate() const;

 protected:
  // Overridden by subclasses to create the feature-specific native dialog box.
  virtual NativeAppModalDialog* CreateNativeDialog() = 0;

  // False if the dialog should no longer be shown, e.g. because the underlying
  // tab navigated away while the dialog was queued.
  bool valid_;

  // The owner of this dialog.
  content::DialogDelegate* delegate_;

  // The toolkit-specific implementation of the app modal dialog box.
  NativeAppModalDialog* native_dialog_;

 private:
  // Information about the message box is held in the following variables.
  string16 title_;

  DISALLOW_COPY_AND_ASSIGN(AppModalDialog);
};

#endif  // CHROME_BROWSER_UI_APP_MODAL_DIALOGS_APP_MODAL_DIALOG_H_
