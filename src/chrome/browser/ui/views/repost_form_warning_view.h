// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_REPOST_FORM_WARNING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_REPOST_FORM_WARNING_VIEW_H_
#pragma once

#include "ui/gfx/native_widget_types.h"
#include "views/window/dialog_delegate.h"

class ConstrainedWindow;
class NavigationController;
class RepostFormWarningController;
class TabContents;

namespace views {
class MessageBoxView;
}

// Displays a dialog that warns the user that they are about to resubmit
// a form.
// To display the dialog, allocate this object on the heap. It will open the
// dialog from its constructor and then delete itself when the user dismisses
// the dialog.
class RepostFormWarningView : public views::DialogDelegate {
 public:
  // Use BrowserWindow::ShowRepostFormWarningDialog to use.
  RepostFormWarningView(gfx::NativeWindow parent_window,
                        TabContents* tab_contents);

  // views::DialogDelegate Methods:
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual string16 GetDialogButtonLabel(
      ui::MessageBoxFlags::DialogButton button) const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;

  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // views::WidgetDelegate Methods:
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

 private:
  virtual ~RepostFormWarningView();

  scoped_ptr<RepostFormWarningController> controller_;

  // The message box view whose commands we handle.
  views::MessageBoxView* message_box_view_;

  DISALLOW_COPY_AND_ASSIGN(RepostFormWarningView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_REPOST_FORM_WARNING_VIEW_H_
