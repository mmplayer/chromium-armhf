// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_VIEWS_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_VIEWS_DELEGATE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "ui/base/accessibility/accessibility_types.h"
#include "views/views_delegate.h"

class ChromeViewsDelegate : public views::ViewsDelegate {
 public:
  static views::View* default_parent_view;

  ChromeViewsDelegate() {}
  virtual ~ChromeViewsDelegate() {}

  // Overridden from views::ViewsDelegate:
  virtual ui::Clipboard* GetClipboard() const OVERRIDE;
  virtual views::View* GetDefaultParentView() OVERRIDE;
  virtual void SaveWindowPlacement(const views::Widget* window,
                                   const std::string& window_name,
                                   const gfx::Rect& bounds,
                                   ui::WindowShowState show_state) OVERRIDE;
  virtual bool GetSavedWindowPlacement(
      const std::string& window_name,
      gfx::Rect* bounds,
      ui::WindowShowState* show_state) const OVERRIDE;
  virtual void NotifyAccessibilityEvent(
      views::View* view, ui::AccessibilityTypes::Event event_type) OVERRIDE;
  virtual void NotifyMenuItemFocused(const string16& menu_name,
                                     const string16& menu_item_name,
                                     int item_index,
                                     int item_count,
                                     bool has_submenu) OVERRIDE;

#if defined(OS_WIN)
  virtual HICON GetDefaultWindowIcon() const OVERRIDE;
#endif
  virtual void AddRef() OVERRIDE;
  virtual void ReleaseRef() OVERRIDE;

  virtual int GetDispositionForEvent(int event_flags) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeViewsDelegate);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_VIEWS_DELEGATE_H_
