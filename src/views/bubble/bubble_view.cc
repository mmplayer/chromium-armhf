// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/bubble/bubble_view.h"

#include "ui/base/animation/slide_animation.h"
#include "views/bubble/bubble_border.h"
#include "views/controls/label.h"
#include "views/layout/box_layout.h"
#include "views/layout/layout_constants.h"
#include "views/views_delegate.h"
#include "views/window/client_view.h"
#include "views/widget/widget.h"

// How long the fade should last for.
static const int kHideFadeDurationMS = 1000;

namespace views {

BubbleView::BubbleView(Widget* owner, View* contents_view)
    : ClientView(owner, contents_view),
      animation_delegate_(NULL),
      registered_accelerator_(false),
      should_fade_(false) {
  ResetLayoutManager();
  InitAnimation();
}

void BubbleView::InitAnimation() {
  fade_animation_.reset(new ui::SlideAnimation(this));
  fade_animation_->SetSlideDuration(kHideFadeDurationMS);
  fade_animation_->Reset(1.0);
}

BubbleView* BubbleView::AsBubbleView() { return this; }
const BubbleView* BubbleView::AsBubbleView() const { return this; }

BubbleView::~BubbleView() {}

void BubbleView::Show() {
  if (!registered_accelerator_)
    registered_accelerator_ = true;
  GetWidget()->Show();
  GetWidget()->Activate();
  SchedulePaint();
}

void BubbleView::StartFade() {
  should_fade_ = true;
  fade_animation_->Hide();
}

void BubbleView::ResetLayoutManager() {
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, 1));
}

void BubbleView::set_animation_delegate(ui::AnimationDelegate* delegate) {
  animation_delegate_ = delegate;
}

void BubbleView::ViewHierarchyChanged(bool is_add,
                                      views::View* parent,
                                      views::View* child) {
  if (is_add && parent == this)
    child->SetBoundsRect(bounds());
}

void BubbleView::Layout() {
  gfx::Rect lb = GetContentsBounds();
  contents_view()->SetBoundsRect(lb);
  contents_view()->Layout();
}

gfx::Size BubbleView::GetPreferredSize() {
  return bounds().size();
}

bool BubbleView::AcceleratorPressed(const Accelerator& accelerator) {
  if (registered_accelerator_) {
    GetWidget()->GetFocusManager()->UnregisterAccelerator(
        views::Accelerator(ui::VKEY_ESCAPE, false, false, false), this);
    registered_accelerator_ = false;
  }
  // Turn off animation, if any.
  if (should_fade_ && fade_animation_->is_animating()) {
    fade_animation_->Reset(1.0);
    fade_animation_->Show();
  }
  GetWidget()->Close();
  return true;
}

void BubbleView::AnimationEnded(const ui::Animation* animation) {
  if (animation_delegate_)
    animation_delegate_->AnimationEnded(animation);

  fade_animation_->Reset(0.0);
  // Close the widget.
  registered_accelerator_ = false;
  GetWidget()->Close();
}

void BubbleView::AnimationProgressed(const ui::Animation* animation) {
  if (fade_animation_->is_animating()) {
    if (animation_delegate_)
      animation_delegate_->AnimationProgressed(animation);

    GetWidget()->SetOpacity(animation->GetCurrentValue() * 255);
    SchedulePaint();
  }
}

}  // namespace views
