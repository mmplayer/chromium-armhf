// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "ui/aura/desktop.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/examples/example_factory.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/shell_window_ids.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "views/widget/widget.h"
#include "views/widget/widget_delegate.h"

namespace aura_shell {
namespace examples {

class LockView : public views::WidgetDelegateView {
 public:
  LockView() {
  }
  virtual ~LockView() {}

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(500, 400);
  }

 private:
  // Overridden from View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRectInt(SK_ColorYELLOW, 0, 0, width(), height());
    string16 text = ASCIIToUTF16("LOCKED!");
    int string_width = font_.GetStringWidth(text);
    canvas->DrawStringInt(text, font_, SK_ColorRED, (width() - string_width)/ 2,
                          (height() - font_.GetHeight()) / 2,
                          string_width, font_.GetHeight());
  }
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE {
    return true;
  }
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE {
    GetWidget()->Close();
  }

  gfx::Font font_;

  DISALLOW_COPY_AND_ASSIGN(LockView);
};

void CreateLock() {
  LockView* lock_view = new LockView;
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  gfx::Size ps = lock_view->GetPreferredSize();

  gfx::Rect desktop_bounds = aura::Desktop::GetInstance()->window()->bounds();
  params.bounds = gfx::Rect((desktop_bounds.width() - ps.width()) / 2,
                            (desktop_bounds.height() - ps.height()) / 2,
                            ps.width(), ps.height());
  params.delegate = lock_view;
  params.parent = Shell::GetInstance()->GetContainer(
      aura_shell::internal::kShellWindowId_LockScreenContainer);
  widget->Init(params);
  widget->SetContentsView(lock_view);
  widget->Show();
  widget->GetNativeView()->set_name("LockView");
}

}  // namespace examples
}  // namespace aura_shell
