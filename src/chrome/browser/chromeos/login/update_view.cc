// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/update_view.h"

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "views/border.h"
#include "views/controls/label.h"
#include "views/controls/progress_bar.h"
#include "views/controls/throbber.h"
#include "views/focus/focus_manager.h"
#include "views/widget/widget.h"

using std::max;
using views::Background;
using views::Label;
using views::View;
using views::Widget;

namespace {

// TODO(nkostylev): Switch to GridLayout.

// Y offset for the 'installing updates' label.
const int kInstallingUpdatesLabelYBottomFromProgressBar = 18;
// Y offset for the progress bar.
const int kProgressBarY = 130;
// Y offset for the 'computer will restart' label.
const int kRebootLabelYFromProgressBar = 55;
// Y offset for the 'ESCAPE to skip' label.
const int kEscapeToSkipLabelY = 48;
// Progress bar width.
const int kProgressBarWidth = 420;
// Progress bar height.
const int kProgressBarHeight = 18;
// Horizontal spacing (ex. min left and right margins for label on the screen).
const int kHorizontalSpacing = 65;
// Horizontal spacing between spinner and label on the curtain screen.
const int kBetweenSpacing = 25;

// Label color.
const SkColor kLabelColor = 0xFF000000;
const SkColor kSkipLabelColor = 0xFFAA0000;
const SkColor kManualRebootLabelColor = 0xFFAA0000;

}  // namespace

namespace chromeos {

UpdateView::UpdateView(chromeos::ScreenObserver* observer)
    : installing_updates_label_(NULL),
      preparing_updates_label_(NULL),
      reboot_label_(NULL),
      manual_reboot_label_(NULL),
      progress_bar_(NULL),
      show_curtain_(true),
      show_manual_reboot_label_(false),
      show_preparing_updates_label_(false),
      observer_(observer) {
}

UpdateView::~UpdateView() {
}

void UpdateView::Init() {
  // Use rounded-rect background.
  views::Painter* painter = chromeos::CreateWizardPainter(
      &chromeos::BorderDefinition::kScreenBorder);
  set_background(views::Background::CreateBackgroundPainter(true, painter));
  SkColor background_color = color_utils::AlphaBlend(
      BorderDefinition::kScreenBorder.top_color,
      BorderDefinition::kScreenBorder.bottom_color, 128);

  installing_updates_label_ = InitLabel(background_color);
  preparing_updates_label_ = InitLabel(background_color);
  preparing_updates_label_->SetVisible(false);
  reboot_label_ = InitLabel(background_color);
  manual_reboot_label_ = InitLabel(background_color);
  manual_reboot_label_->SetVisible(false);
  manual_reboot_label_->SetEnabledColor(kManualRebootLabelColor);

  progress_bar_ = new views::ProgressBar();
  AddChildView(progress_bar_);
  progress_bar_->SetDisplayRange(0.0, 100.0);

  // Curtain view.
  checking_label_ = InitLabel(background_color);
  throbber_ = CreateDefaultThrobber();
  AddChildView(throbber_);

#if !defined(OFFICIAL_BUILD)
  escape_to_skip_label_ = InitLabel(background_color);
  escape_to_skip_label_->SetEnabledColor(kSkipLabelColor);
  escape_to_skip_label_->SetText(
      ASCIIToUTF16("Press ESCAPE to skip (Non-official builds only)"));
#endif

  UpdateLocalizedStrings();
  UpdateVisibility();
}

void UpdateView::Reset() {
  progress_bar_->SetValue(0.0);
}

void UpdateView::UpdateLocalizedStrings() {
  installing_updates_label_->SetText(l10n_util::GetStringFUTF16(
      IDS_INSTALLING_UPDATE,
      l10n_util::GetStringUTF16(IDS_PRODUCT_OS_NAME)));
  preparing_updates_label_->SetText(
      l10n_util::GetStringUTF16(IDS_UPDATE_AVAILABLE));
  reboot_label_->SetText(
      l10n_util::GetStringUTF16(IDS_INSTALLING_UPDATE_DESC));
  manual_reboot_label_->SetText(
      l10n_util::GetStringUTF16(IDS_UPDATE_COMPLETED));
  checking_label_->SetText(
      l10n_util::GetStringUTF16(IDS_CHECKING_FOR_UPDATES));
}

void UpdateView::AddProgress(int ticks) {
  progress_bar_->SetValue(
      max(progress_bar_->current_value() + ticks, 100.0));
}

void UpdateView::SetProgress(int progress) {
  progress_bar_->SetValue(progress);
}

void UpdateView::ShowManualRebootInfo() {
  show_manual_reboot_label_ = true;
  UpdateVisibility();
}

void UpdateView::ShowPreparingUpdatesInfo(bool visible) {
  show_preparing_updates_label_ = visible;
  UpdateVisibility();
}

void UpdateView::ShowCurtain(bool show_curtain) {
  if (show_curtain_ != show_curtain) {
    show_curtain_ = show_curtain;
    UpdateVisibility();
  }
}

// Sets the bounds of the view, placing center of the view at the given
// coordinates (|x| and |y|).
static void setViewBounds(views::View* view, int x, int y) {
  int preferred_width = view->GetPreferredSize().width();
  int preferred_height = view->GetPreferredSize().height();
  view->SetBounds(
      x - preferred_width / 2,
      y - preferred_height / 2,
      preferred_width,
      preferred_height);
}

void UpdateView::Layout() {
  int max_width = width() - GetInsets().width() - 2 * kHorizontalSpacing;
  int right_margin = GetInsets().right() + kHorizontalSpacing;
  int max_height = height() - GetInsets().height();
  int vertical_center = GetInsets().top() + max_height / 2;

  installing_updates_label_->SizeToFit(max_width);
  preparing_updates_label_->SizeToFit(max_width);
  reboot_label_->SizeToFit(max_width);
  manual_reboot_label_->SizeToFit(max_width);

  progress_bar_->SetBounds(right_margin,
                           vertical_center - kProgressBarHeight / 2,
                           max_width,
                           kProgressBarHeight);

  installing_updates_label_->SetX(right_margin);
  installing_updates_label_->SetY(
      progress_bar_->y() -
      kInstallingUpdatesLabelYBottomFromProgressBar -
      installing_updates_label_->height());
  preparing_updates_label_->SetX(installing_updates_label_->x());
  preparing_updates_label_->SetY(installing_updates_label_->y());
  reboot_label_->SetX(right_margin);
  reboot_label_->SetY(
      progress_bar_->y() +
      progress_bar_->height() +
      kRebootLabelYFromProgressBar);
  manual_reboot_label_->SetX(reboot_label_->x());
  manual_reboot_label_->SetY(reboot_label_->y());
  // Curtain layout is independed.
  int x_center = width() / 2;
  int throbber_width = throbber_->GetPreferredSize().width();
  checking_label_->SizeToFit(max_width - throbber_width - kBetweenSpacing);
  int checking_label_width = checking_label_->GetPreferredSize().width();
  int space_half = (kBetweenSpacing + 1) / 2;
  setViewBounds(
      throbber_, x_center - checking_label_width / 2 - space_half,
      vertical_center);
  setViewBounds(
      checking_label_, x_center + (throbber_width + 1) / 2 + space_half,
      vertical_center);
#if !defined(OFFICIAL_BUILD)
  escape_to_skip_label_->SizeToFit(max_width);
  escape_to_skip_label_->SetX(right_margin);
  escape_to_skip_label_->SetY(kEscapeToSkipLabelY);
#endif
  SchedulePaint();
}

views::Label* UpdateView::InitLabel(SkColor background_color) {
  views::Label* label = new views::Label();
  label->SetBackgroundColor(background_color);
  label->SetEnabledColor(kLabelColor);
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label->SetMultiLine(true);

  ResourceBundle& res_bundle = ResourceBundle::GetSharedInstance();
  gfx::Font label_font = res_bundle.GetFont(ResourceBundle::MediumFont);
  label->SetFont(label_font);

  AddChildView(label);
  return label;
}

void UpdateView::UpdateVisibility() {
  installing_updates_label_->SetVisible(!show_curtain_ &&
                                        !show_manual_reboot_label_ &&
                                        !show_preparing_updates_label_);
  preparing_updates_label_->SetVisible(!show_curtain_ &&
                                       !show_manual_reboot_label_ &&
                                       show_preparing_updates_label_);
  reboot_label_->SetVisible(!show_curtain_&& !show_manual_reboot_label_);
  manual_reboot_label_->SetVisible(!show_curtain_ && show_manual_reboot_label_);
  progress_bar_->SetVisible(!show_curtain_);

  checking_label_->SetVisible(show_curtain_);
  throbber_->SetVisible(show_curtain_);
  if (show_curtain_) {
    throbber_->Start();
  } else {
    throbber_->Stop();
  }

  // Speak the shown label when accessibility is enabled.
  const Label* label_spoken(NULL);
  if (checking_label_->IsVisible()) {
    label_spoken = checking_label_;
  } else if (manual_reboot_label_->IsVisible()) {
    label_spoken = manual_reboot_label_;
  } else if (preparing_updates_label_->IsVisible()) {
    label_spoken = preparing_updates_label_;
  } else if (installing_updates_label_->IsVisible()) {
    label_spoken = installing_updates_label_;
  } else {
    NOTREACHED();
  }
  const std::string text =
      label_spoken ? UTF16ToUTF8(label_spoken->GetText()) : std::string();
  WizardAccessibilityHelper::GetInstance()->MaybeSpeak(text.c_str(), false,
                                                       true);
}

}  // namespace chromeos
