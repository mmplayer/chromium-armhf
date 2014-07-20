// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/fullscreen_exit_bubble_views.h"

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/views/bubble/bubble.h"
#include "grit/generated_resources.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/screen.h"
#include "views/bubble/bubble_border.h"
#include "views/controls/button/text_button.h"
#include "views/controls/link.h"
#include "views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/base/l10n/l10n_util_win.h"
#endif

// FullscreenExitView ----------------------------------------------------------

namespace {
// Space between the site info label and the buttons / link.
const int kMiddlePaddingPx = 30;
}  // namespace

class FullscreenExitBubbleViews::FullscreenExitView
    : public views::View,
      public views::ButtonListener {
 public:
  FullscreenExitView(FullscreenExitBubbleViews* bubble,
                     const string16& accelerator,
                     const GURL& url,
                     FullscreenExitBubbleType bubble_type);
  virtual ~FullscreenExitView();

  // views::View
  virtual gfx::Size GetPreferredSize();

  // views::ButtonListener
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  void UpdateContent(const GURL& url, FullscreenExitBubbleType bubble_type);

 private:
  // views::View
  virtual void Layout();

  FullscreenExitBubbleViews* bubble_;

  // Clickable hint text for exiting browser fullscreen mode.
  views::Link link_;
  // Instruction for exiting tab fullscreen mode.
  views::Label instruction_label_;
  views::Label message_label_;
  views::NativeTextButton* accept_button_;
  views::NativeTextButton* deny_button_;
};

FullscreenExitBubbleViews::FullscreenExitView::FullscreenExitView(
    FullscreenExitBubbleViews* bubble,
    const string16& accelerator,
    const GURL& url,
    FullscreenExitBubbleType bubble_type)
    : bubble_(bubble),
      accept_button_(NULL),
      deny_button_(NULL) {
  views::BubbleBorder* bubble_border =
      new views::BubbleBorder(views::BubbleBorder::NONE);
  bubble_border->set_background_color(Bubble::kBackgroundColor);
  set_background(new views::BubbleBackground(bubble_border));
  set_border(bubble_border);
  set_focusable(false);

  message_label_.set_parent_owned(false);
  message_label_.SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::MediumFont));

  instruction_label_.set_parent_owned(false);
  instruction_label_.SetText(bubble_->GetInstructionText());
  instruction_label_.SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::MediumFont));

  link_.set_parent_owned(false);
  link_.set_collapse_when_hidden(false);
  link_.set_focusable(false);
#if !defined(OS_CHROMEOS)
  link_.SetText(
      l10n_util::GetStringFUTF16(IDS_EXIT_FULLSCREEN_MODE,
                                 accelerator));
#else
  link_.SetText(l10n_util::GetStringUTF16(IDS_EXIT_FULLSCREEN_MODE));
#endif
  link_.set_listener(bubble);
  link_.SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::MediumFont));
  link_.SetPressedColor(message_label_.enabled_color());
  link_.SetEnabledColor(message_label_.enabled_color());
  link_.SetVisible(false);

  link_.SetBackgroundColor(background()->get_color());
  message_label_.SetBackgroundColor(background()->get_color());
  instruction_label_.SetBackgroundColor(background()->get_color());
  AddChildView(&message_label_);
  AddChildView(&instruction_label_);
  AddChildView(&link_);

  accept_button_ = new views::NativeTextButton(this,
      UTF16ToWideHack(bubble->GetAllowButtonText()));
  accept_button_->set_focusable(false);
  AddChildView(accept_button_);

  deny_button_ = new views::NativeTextButton(this);
  deny_button_->set_focusable(false);
  AddChildView(deny_button_);

  UpdateContent(url, bubble_type);
}

FullscreenExitBubbleViews::FullscreenExitView::~FullscreenExitView() {
}

void FullscreenExitBubbleViews::FullscreenExitView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == accept_button_)
    bubble_->Accept();
  else
    bubble_->Cancel();
}

gfx::Size FullscreenExitBubbleViews::FullscreenExitView::GetPreferredSize() {
  gfx::Size message_size(message_label_.GetPreferredSize());

  gfx::Size button_instruction_area;
  if (instruction_label_.IsVisible()) {
    button_instruction_area = instruction_label_.GetPreferredSize();
  } else if (link_.IsVisible()) {
    button_instruction_area = link_.GetPreferredSize();
  } else {
    gfx::Size accept_size(accept_button_->GetPreferredSize());
    gfx::Size deny_size(deny_button_->GetPreferredSize());
    button_instruction_area.set_height(accept_size.height());
    button_instruction_area.set_width(
        accept_size.width() + kPaddingPx + deny_size.width());
  }

  gfx::Insets insets(GetInsets());
  gfx::Size result(
      message_size.width() + kMiddlePaddingPx + button_instruction_area.width(),
      std::max(message_size.height(), button_instruction_area.height()));
  result.Enlarge(insets.width() + 2 * kPaddingPx,
                 insets.height() + 2 * kPaddingPx);
  return result;
}

void FullscreenExitBubbleViews::FullscreenExitView::UpdateContent(
    const GURL& url,
    FullscreenExitBubbleType bubble_type) {
  DCHECK_NE(FEB_TYPE_NONE, bubble_type);

  message_label_.SetText(bubble_->GetCurrentMessageText());
  if (fullscreen_bubble::ShowButtonsForType(bubble_type)) {
    link_.SetVisible(false);
    instruction_label_.SetVisible(false);
    accept_button_->SetVisible(true);
    deny_button_->SetText(bubble_->GetCurrentDenyButtonText());
    deny_button_->SetVisible(true);
    deny_button_->ClearMaxTextSize();
  } else {
    bool link_visible =
        bubble_type == FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION;
    link_.SetVisible(link_visible);
    instruction_label_.SetVisible(!link_visible);
    accept_button_->SetVisible(false);
    deny_button_->SetVisible(false);
  }
}

void FullscreenExitBubbleViews::FullscreenExitView::Layout() {
  // TODO(thakis): Use a LayoutManager instead of doing manual layout.
  gfx::Size message_size(message_label_.GetPreferredSize());
  gfx::Insets insets(GetInsets());
  int x = insets.left() + kPaddingPx;
  int inner_height = height() - insets.height();

  message_label_.SetBounds(
      x, insets.top() + (inner_height - message_size.height()) / 2,
      message_size.width(), message_size.height());
  x += message_size.width() + kMiddlePaddingPx;

  if (instruction_label_.IsVisible()) {
    gfx::Size instruction_size(instruction_label_.GetPreferredSize());
    instruction_label_.SetBounds(
        x, insets.top() + (inner_height - instruction_size.height()) / 2,
        instruction_size.width(), instruction_size.height());
  } else if (link_.IsVisible()) {
    gfx::Size link_size(link_.GetPreferredSize());
    link_.SetBounds(x, insets.top() + (inner_height - link_size.height()) / 2,
                    link_size.width(), link_size.height());
  } else {
    gfx::Size accept_size(accept_button_->GetPreferredSize());
    gfx::Size deny_size(deny_button_->GetPreferredSize());
    int button_y = insets.top() + (inner_height - accept_size.height()) / 2;
    accept_button_->SetBounds(
        x, button_y, accept_size.width(), accept_size.height());
    x += accept_size.width() + kPaddingPx;
    deny_button_->SetBounds(
        x, button_y, deny_size.width(), deny_size.height());
  }
}

// FullscreenExitBubbleViews ---------------------------------------------------

FullscreenExitBubbleViews::FullscreenExitBubbleViews(
    views::Widget* frame,
    Browser* browser,
    const GURL& url,
    FullscreenExitBubbleType bubble_type)
    : FullscreenExitBubble(browser, url, bubble_type),
      root_view_(frame->GetRootView()),
      popup_(NULL),
      size_animation_(new ui::SlideAnimation(this)) {
  size_animation_->Reset(1);

  // Create the contents view.
  views::Accelerator accelerator(ui::VKEY_UNKNOWN, false, false, false);
  bool got_accelerator = frame->GetAccelerator(IDC_FULLSCREEN, &accelerator);
  DCHECK(got_accelerator);
  view_ = new FullscreenExitView(
      this, accelerator.GetShortcutText(), url, bubble_type_);

  // Initialize the popup.
  popup_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.transparent = true;
  params.can_activate = false;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = frame->GetNativeView();
  params.bounds = GetPopupRect(false);
  popup_->Init(params);
  gfx::Size size = GetPopupRect(true).size();
  popup_->SetContentsView(view_);
  // We set layout manager to NULL to prevent the widget from sizing its
  // contents to the same size as itself. This prevents the widget contents from
  // shrinking while we animate the height of the popup to give the impression
  // that it is sliding off the top of the screen.
  popup_->GetRootView()->SetLayoutManager(NULL);
  view_->SetBounds(0, 0, size.width(), size.height());
  popup_->Show();  // This does not activate the popup.

  StartWatchingMouseIfNecessary();
}

FullscreenExitBubbleViews::~FullscreenExitBubbleViews() {
  // This is tricky.  We may be in an ATL message handler stack, in which case
  // the popup cannot be deleted yet.  We also can't set the popup's ownership
  // model to NATIVE_WIDGET_OWNS_WIDGET because if the user closed the last tab
  // while in fullscreen mode, Windows has already destroyed the popup HWND by
  // the time we get here, and thus either the popup will already have been
  // deleted (if we set this in our constructor) or the popup will never get
  // another OnFinalMessage() call (if not, as currently).  So instead, we tell
  // the popup to synchronously hide, and then asynchronously close and delete
  // itself.
  popup_->Close();
  MessageLoop::current()->DeleteSoon(FROM_HERE, popup_);
}

void FullscreenExitBubbleViews::LinkClicked(
    views::Link* source, int event_flags) {
  ToggleFullscreen();
}

void FullscreenExitBubbleViews::UpdateContent(
    const GURL& url,
    FullscreenExitBubbleType bubble_type) {
  DCHECK_NE(FEB_TYPE_NONE, bubble_type);
  if (bubble_type_ == bubble_type && url_ == url)
    return;

  url_ = url;
  bubble_type_ = bubble_type;
  view_->UpdateContent(url_, bubble_type_);

  gfx::Size size = GetPopupRect(true).size();
  view_->SetSize(size);
  popup_->SetBounds(GetPopupRect(false));
  Show();

  StopWatchingMouse();
  StartWatchingMouseIfNecessary();
}

void FullscreenExitBubbleViews::AnimationProgressed(
    const ui::Animation* animation) {
  gfx::Rect popup_rect(GetPopupRect(false));
  if (popup_rect.IsEmpty()) {
    popup_->Hide();
  } else {
    popup_->SetBounds(popup_rect);
    view_->SetY(popup_rect.height() - view_->height());
    popup_->Show();
  }
}

void FullscreenExitBubbleViews::AnimationEnded(
    const ui::Animation* animation) {
  AnimationProgressed(animation);
}

void FullscreenExitBubbleViews::Hide() {
  size_animation_->SetSlideDuration(kSlideOutDurationMs);
  size_animation_->Hide();
}

void FullscreenExitBubbleViews::Show() {
  size_animation_->SetSlideDuration(kSlideInDurationMs);
  size_animation_->Show();
}

bool FullscreenExitBubbleViews::IsAnimating() {
  return size_animation_->GetCurrentValue() != 0;
}

bool FullscreenExitBubbleViews::IsWindowActive() {
  return root_view_->GetWidget()->IsActive();
}

bool FullscreenExitBubbleViews::WindowContainsPoint(gfx::Point pos) {
  return root_view_->HitTest(pos);
}

gfx::Point FullscreenExitBubbleViews::GetCursorScreenPoint() {
  gfx::Point cursor_pos = gfx::Screen::GetCursorScreenPoint();
  gfx::Point transformed_pos(cursor_pos);
  views::View::ConvertPointToView(NULL, root_view_, &transformed_pos);
  return transformed_pos;
}

gfx::Rect FullscreenExitBubbleViews::GetPopupRect(
    bool ignore_animation_state) const {
  gfx::Size size(view_->GetPreferredSize());
  // NOTE: don't use the bounds of the root_view_. On linux changing window
  // size is async. Instead we use the size of the screen.
  gfx::Rect screen_bounds = gfx::Screen::GetMonitorAreaNearestWindow(
      root_view_->GetWidget()->GetNativeView());
  gfx::Point origin(screen_bounds.x() +
                    (screen_bounds.width() - size.width()) / 2,
                    kPopupTopPx + screen_bounds.y());
  if (!ignore_animation_state) {
    int total_height = size.height() + kPopupTopPx;
    int popup_bottom = size_animation_->CurrentValueBetween(
        static_cast<double>(total_height), 0.0f);
    int y_offset = std::min(popup_bottom, kPopupTopPx);
    size.set_height(size.height() - popup_bottom + y_offset);
    origin.set_y(origin.y() - y_offset);
  }
  return gfx::Rect(origin, size);
}

void FullscreenExitBubbleViews::StartWatchingMouseIfNecessary() {
  if (!fullscreen_bubble::ShowButtonsForType(bubble_type_))
    StartWatchingMouse();
}
