// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/native_widget_views_example.h"

#include "ui/gfx/canvas.h"
#include "views/controls/button/text_button.h"
#include "views/examples/example_base.h"
#include "views/test/test_views_delegate.h"
#include "views/view.h"
#include "views/widget/widget.h"
#include "views/widget/native_widget_views.h"

namespace examples {

// A ContentView for our example widget. Contains a variety of controls for
// testing NativeWidgetViews event handling. If any part of the Widget's bounds
// are rendered red, something went wrong.
class TestContentView : public views::View,
                        public views::ButtonListener {
 public:
  TestContentView()
      : click_count_(0),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            button_(new views::TextButton(this, L"Click me!"))) {
    AddChildView(button_);
  }
  virtual ~TestContentView() {
  }

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    SkColor color = click_count_ % 2 == 0 ? SK_ColorGREEN : SK_ColorBLUE;
    canvas->FillRectInt(color, 0, 0, width(), height());
  }
  virtual void Layout() OVERRIDE {
    button_->SetBounds(10, 10, width() - 20, height() - 20);
  }

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE {
    if (sender == button_) {
      ++click_count_;
      SchedulePaint();
    }
  }

 private:
  int click_count_;
  views::TextButton* button_;

  DISALLOW_COPY_AND_ASSIGN(TestContentView);
};

NativeWidgetViewsExample::NativeWidgetViewsExample(ExamplesMain* main)
    : ExampleBase(main, "NativeWidgetViews") {
}

NativeWidgetViewsExample::~NativeWidgetViewsExample() {
}

void NativeWidgetViewsExample::CreateExampleView(views::View* container) {
  views::Widget* widget = new views::Widget;
  views::NativeWidgetViews* nwv = new views::NativeWidgetViews(widget);
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);

  // Set parent View for widget.  Real code should use params.parent_widget
  // but since tabs are implemented with Views instead of Widgets on Windows
  // we have to hack the parenting.
#if defined(OS_WIN)
  views::TestViewsDelegate* test_views_delegate =
      static_cast<views::TestViewsDelegate*>(
          views::ViewsDelegate::views_delegate);
  views::View* old_default_parent_view =
      test_views_delegate->GetDefaultParentView();
  test_views_delegate->set_default_parent_view(container);
#else
  params.parent_widget = container->GetWidget();
#endif

  params.native_widget = nwv;
  widget->Init(params);

#if defined(OS_WIN)
  test_views_delegate->set_default_parent_view(old_default_parent_view);
#endif

  widget->SetContentsView(new TestContentView);
  widget->SetBounds(gfx::Rect(10, 10, 300, 150));
}

}  // namespace examples
