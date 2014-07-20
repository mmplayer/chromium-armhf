// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/animation/slide_animation.h"
#include "views/bubble/bubble_border.h"
#include "views/bubble/bubble_delegate.h"
#include "views/bubble/bubble_view.h"
#include "views/test/views_test_base.h"
#include "views/widget/widget.h"

namespace views {

namespace {

typedef ViewsTestBase BubbleDelegateTest;

class TestBubbleDelegate : public BubbleDelegateView {
 public:
  explicit TestBubbleDelegate(Widget *frame): BubbleDelegateView(frame) {}
  SkColor GetFrameBackgroundColor() { return SK_ColorGREEN; }
  gfx::Rect GetBounds() { return gfx::Rect(10, 10, 200, 200); }
  BubbleBorder::ArrowLocation GetFrameArrowLocation() {
    return BubbleBorder::LEFT_BOTTOM;
  }
  View* GetContentsView() { return &view_; }

  View view_;
};

TEST_F(BubbleDelegateTest, CreateDelegate) {
  scoped_ptr<Widget> bubble_widget(new Widget());
  Widget::InitParams params(Widget::InitParams::TYPE_BUBBLE);
  TestBubbleDelegate delegate(bubble_widget.get());
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.delegate = &delegate;
  bubble_widget->Init(params);
  EXPECT_EQ(&delegate, bubble_widget->widget_delegate());
  EXPECT_EQ(bubble_widget, delegate.GetWidget());
  bubble_widget->CloseNow();
  bubble_widget.reset(NULL);
  RunPendingMessages();
}

}  // namespace
}  // namespace views
