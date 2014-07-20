// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REPOST_FORM_WARNING_CONTROLLER_H_
#define CHROME_BROWSER_REPOST_FORM_WARNING_CONTROLLER_H_
#pragma once

#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class ConstrainedWindow;
class TabContents;

// This class is used to continue or cancel a pending reload when the
// repost form warning is shown. It is owned by the platform-dependent
// |RepostFormWarning{Gtk,Mac,View}| classes.
class RepostFormWarningController : public NotificationObserver {
 public:
  explicit RepostFormWarningController(TabContents* tab_contents);
  virtual ~RepostFormWarningController();

  // Cancel the reload.
  void Cancel();

  // Continue the reload.
  void Continue();

  void set_window(ConstrainedWindow* window) { window_ = window; }

 private:
  // NotificationObserver implementation.
  // Watch for a new load or a closed tab and dismiss the dialog if they occur.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Close the warning dialog.
  void CloseDialog();

  NotificationRegistrar registrar_;

  // Tab contents, used to continue the reload.
  TabContents* tab_contents_;

  ConstrainedWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(RepostFormWarningController);
};

#endif  // CHROME_BROWSER_REPOST_FORM_WARNING_CONTROLLER_H_
