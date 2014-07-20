// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_TEST_TEST_VIEWS_DELEGATE_H_
#define VIEWS_TEST_TEST_VIEWS_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "ui/base/accessibility/accessibility_types.h"
#include "views/views_delegate.h"

namespace ui {
class Clipboard;
}

namespace views {
class View;
class Widget;

class TestViewsDelegate : public ViewsDelegate {
 public:
  TestViewsDelegate();
  virtual ~TestViewsDelegate();

  void set_default_parent_view(View* view) {
    default_parent_view_ = view;
  }

  // Overridden from ViewsDelegate:
  virtual ui::Clipboard* GetClipboard() const OVERRIDE;
  virtual View* GetDefaultParentView() OVERRIDE;
  virtual void SaveWindowPlacement(const Widget* window,
                                   const std::string& window_name,
                                   const gfx::Rect& bounds,
                                   ui::WindowShowState show_state) OVERRIDE;
  virtual bool GetSavedWindowPlacement(
      const std::string& window_name,
      gfx::Rect* bounds,
      ui::WindowShowState* show_state) const OVERRIDE;

  virtual void NotifyAccessibilityEvent(
      View* view, ui::AccessibilityTypes::Event event_type) OVERRIDE {}

  virtual void NotifyMenuItemFocused(const string16& menu_name,
                                     const string16& menu_item_name,
                                     int item_index,
                                     int item_count,
                                     bool has_submenu) OVERRIDE {}
#if defined(OS_WIN)
  virtual HICON GetDefaultWindowIcon() const OVERRIDE {
    return NULL;
  }
#endif

  virtual void AddRef() OVERRIDE {}
  virtual void ReleaseRef() OVERRIDE {}

  virtual int GetDispositionForEvent(int event_flags) OVERRIDE;

 private:
  View* default_parent_view_;
  mutable scoped_ptr<ui::Clipboard> clipboard_;

  DISALLOW_COPY_AND_ASSIGN(TestViewsDelegate);
};

}  // namespace views

#endif  // VIEWS_TEST_TEST_VIEWS_DELEGATE_H_
