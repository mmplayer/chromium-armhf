// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_CHROME_SHELL_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_CHROME_SHELL_DELEGATE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura_shell/shell_delegate.h"

class ChromeShellDelegate : public aura_shell::ShellDelegate {
 public:
  ChromeShellDelegate();
  virtual ~ChromeShellDelegate();

  // aura_shell::ShellDelegate overrides;
  virtual void CreateNewWindow() OVERRIDE;
  virtual void ShowApps() OVERRIDE;
  virtual void LauncherItemClicked(
      const aura_shell::LauncherItem& item) OVERRIDE;
  virtual bool ConfigureLauncherItem(aura_shell::LauncherItem* item) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeShellDelegate);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_CHROME_SHELL_DELEGATE_H_
