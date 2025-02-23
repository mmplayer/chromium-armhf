// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/eula_view.h"

#include <signal.h>
#include <sys/types.h>

#include <string>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/views_eula_screen_actor.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/browser/ui/views/handle_web_keyboard_event.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/common/url_constants.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/button/text_button.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/controls/throbber.h"
#include "views/events/event.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/layout/layout_manager.h"
#include "views/window/dialog_delegate.h"

namespace chromeos {

namespace {

const int kBorderSize = 10;
const int kCheckboxWidth = 26;
const int kLastButtonHorizontalMargin = 10;
const int kMargin = 20;
const int kTextMargin = 10;

const char kGoogleEulaUrl[] = "about:terms";

enum kLayoutColumnsets {
  SINGLE_CONTROL_ROW,
  SINGLE_CONTROL_WITH_SHIFT_ROW,
  SINGLE_LINK_WITH_SHIFT_ROW,
  LAST_ROW
};

// A simple LayoutManager that causes the associated view's one child to be
// sized to match the bounds of its parent except the bounds, if set.
struct FillLayoutWithBorder : public views::LayoutManager {
  // Overridden from LayoutManager:
  virtual void Layout(views::View* host) {
    DCHECK(host->has_children());
    host->child_at(0)->SetBoundsRect(host->GetContentsBounds());
  }
  virtual gfx::Size GetPreferredSize(views::View* host) {
    return gfx::Size(host->width(), host->height());
  }
};

}  // namespace

// System security setting dialog.
class TpmInfoView : public views::DialogDelegateView {
 public:
  explicit TpmInfoView(EulaView* eula_view);
  virtual ~TpmInfoView();

  void Init();

  void ShowTpmPassword(const std::string& tpm_password);

 protected:
  // views::DialogDelegateView overrides:
  virtual bool Accept() OVERRIDE { return true; }
  virtual bool IsModal() const OVERRIDE { return true; }
  virtual views::View* GetContentsView() OVERRIDE { return this; }
  virtual int GetDialogButtons() const OVERRIDE {
    return MessageBoxFlags::DIALOGBUTTON_OK;
  }

  // views::View overrides:
  virtual string16 GetWindowTitle() const OVERRIDE {
    return l10n_util::GetStringUTF16(IDS_EULA_SYSTEM_SECURITY_SETTING);
  }

  gfx::Size GetPreferredSize() {
    return gfx::Size(views::Widget::GetLocalizedContentsSize(
        IDS_TPM_INFO_DIALOG_WIDTH_CHARS,
        IDS_TPM_INFO_DIALOG_HEIGHT_LINES));
  }

 private:
  chromeos::EulaView* eula_view_;

  views::Label* busy_label_;
  views::Label* password_label_;
  views::Throbber* throbber_;

  DISALLOW_COPY_AND_ASSIGN(TpmInfoView);
};

TpmInfoView::TpmInfoView(EulaView* eula_view)
    : eula_view_(eula_view) {
  DCHECK(eula_view_);
}

TpmInfoView::~TpmInfoView() {
  eula_view_->OnTpmInfoViewClosed(this);
}

void TpmInfoView::Init() {
  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_EULA_SYSTEM_SECURITY_SETTING_DESCRIPTION));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(label);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, 0);
  label = new views::Label(l10n_util::GetStringUTF16(
      IDS_EULA_SYSTEM_SECURITY_SETTING_DESCRIPTION_KEY));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(label);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  column_set = layout->AddColumnSet(1);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 1);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font password_font =
      rb.GetFont(ResourceBundle::MediumFont).DeriveFont(0, gfx::Font::BOLD);
  // Password will be set later.
  password_label_ = new views::Label(string16(), password_font);
  password_label_->SetVisible(false);
  layout->AddView(password_label_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  column_set = layout->AddColumnSet(2);
  column_set->AddPaddingColumn(1, 0);
  // Resize of the throbber and label is not allowed, since we want they to be
  // placed in the center.
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(1, 0);
  // Border padding columns should have the same width. It guaranties that
  // throbber and label will be placed in the center.
  column_set->LinkColumnSizes(0, 4, -1);

  layout->StartRow(0, 2);
  throbber_ = chromeos::CreateDefaultThrobber();
  throbber_->Start();
  layout->AddView(throbber_);
  busy_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_EULA_TPM_BUSY));
  layout->AddView(busy_label_);
  layout->AddPaddingRow(0, views::kRelatedControlHorizontalSpacing);
}

void TpmInfoView::ShowTpmPassword(const std::string& tpm_password) {
  password_label_->SetText(ASCIIToUTF16(tpm_password));
  password_label_->SetVisible(true);
  busy_label_->SetVisible(false);
  throbber_->Stop();
  throbber_->SetVisible(false);
}

////////////////////////////////////////////////////////////////////////////////
// EULATabContentsDelegate, public:

bool EULATabContentsDelegate::IsPopup(TabContents* source) {
  return false;
}

bool EULATabContentsDelegate::ShouldAddNavigationToHistory(
    const history::HistoryAddPageArgs& add_page_args,
    content::NavigationType navigation_type) {
  return false;
}

bool EULATabContentsDelegate::HandleContextMenu(
    const ContextMenuParams& params) {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// EulaView, public:

EulaView::EulaView(ViewsEulaScreenActor* actor)
    : google_eula_label_(NULL),
      google_eula_view_(NULL),
      usage_statistics_checkbox_(NULL),
      learn_more_link_(NULL),
      oem_eula_label_(NULL),
      oem_eula_view_(NULL),
      system_security_settings_link_(NULL),
      back_button_(NULL),
      continue_button_(NULL),
      tpm_info_view_(NULL),
      actor_(actor),
      bubble_(NULL) {
}

EulaView::~EulaView() {
  // bubble_ will be set to NULL in callback.
  if (bubble_)
    bubble_->Close();
}

// Convenience function to set layout's columnsets for this screen.
static void SetUpGridLayout(views::GridLayout* layout) {
  static const int kPadding = kBorderSize + kMargin;
  views::ColumnSet* column_set = layout->AddColumnSet(SINGLE_CONTROL_ROW);
  column_set->AddPaddingColumn(0, kPadding);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kPadding);

  column_set = layout->AddColumnSet(SINGLE_CONTROL_WITH_SHIFT_ROW);
  column_set->AddPaddingColumn(0, kPadding + kTextMargin);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kPadding);

  column_set = layout->AddColumnSet(SINGLE_LINK_WITH_SHIFT_ROW);
  column_set->AddPaddingColumn(0, kPadding + kTextMargin + kCheckboxWidth);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kPadding);

  column_set = layout->AddColumnSet(LAST_ROW);
  column_set->AddPaddingColumn(0, kPadding + kTextMargin);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kLastButtonHorizontalMargin + kBorderSize);
}

void EulaView::Init() {
  // Use rounded rect background.
  views::Painter* painter = CreateWizardPainter(
      &BorderDefinition::kScreenBorder);
  set_background(
      views::Background::CreateBackgroundPainter(true, painter));

  // Layout created controls.
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  SetUpGridLayout(layout);

  static const int kPadding = kBorderSize + kMargin;
  layout->AddPaddingRow(0, kPadding);
  layout->StartRow(0, SINGLE_CONTROL_ROW);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font label_font =
      rb.GetFont(ResourceBundle::MediumFont).DeriveFont(0, gfx::Font::NORMAL);
  google_eula_label_ = new views::Label(string16(), label_font);
  layout->AddView(google_eula_label_, 1, 1,
                  views::GridLayout::LEADING, views::GridLayout::FILL);

  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
  layout->StartRow(1, SINGLE_CONTROL_ROW);
  views::View* box_view = new views::View();
  box_view->set_border(views::Border::CreateSolidBorder(1, SK_ColorBLACK));
  box_view->SetLayoutManager(new FillLayoutWithBorder());
  layout->AddView(box_view);

  google_eula_view_ = new DOMView();
  box_view->AddChildView(google_eula_view_);

  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, SINGLE_CONTROL_WITH_SHIFT_ROW);
  usage_statistics_checkbox_ = new views::Checkbox(string16());
  usage_statistics_checkbox_->SetMultiLine(true);
  usage_statistics_checkbox_->SetChecked(
      actor_->screen()->IsUsageStatsEnabled());
  layout->AddView(usage_statistics_checkbox_);

  layout->StartRow(0, SINGLE_LINK_WITH_SHIFT_ROW);
  learn_more_link_ = new views::Link();
  learn_more_link_->set_listener(this);
  layout->AddView(learn_more_link_);

  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, SINGLE_CONTROL_ROW);
  oem_eula_label_ = new views::Label(string16(), label_font);
  layout->AddView(oem_eula_label_, 1, 1,
                  views::GridLayout::LEADING, views::GridLayout::FILL);

  oem_eula_page_ = actor_->screen()->GetOemEulaUrl();
  if (!oem_eula_page_.is_empty()) {
    layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
    layout->StartRow(1, SINGLE_CONTROL_ROW);
    box_view = new views::View();
    box_view->SetLayoutManager(new FillLayoutWithBorder());
    box_view->set_border(views::Border::CreateSolidBorder(1, SK_ColorBLACK));
    layout->AddView(box_view);

    oem_eula_view_ = new DOMView();
    box_view->AddChildView(oem_eula_view_);
  }

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  layout->StartRow(0, LAST_ROW);
  system_security_settings_link_ = new views::Link();
  system_security_settings_link_->set_listener(this);

  if (!actor_->screen()->IsTpmEnabled()) {
    system_security_settings_link_->SetEnabled(false);
  }

  layout->AddView(system_security_settings_link_);

  back_button_ = new login::WideButton(this, std::wstring());
  layout->AddView(back_button_);

  continue_button_ = new login::WideButton(this, std::wstring());
  layout->AddView(continue_button_);
  layout->AddPaddingRow(0, kPadding);

  UpdateLocalizedStrings();
}

void EulaView::UpdateLocalizedStrings() {
  // Load Google EULA and its title.
  LoadEulaView(google_eula_view_,
               google_eula_label_,
               GURL(kGoogleEulaUrl));

  // Load OEM EULA and its title.
  if (!oem_eula_page_.is_empty())
    LoadEulaView(oem_eula_view_, oem_eula_label_, oem_eula_page_);

  // Set tooltip for usage statistics checkbox if the metric is unmanaged.
  if (!usage_statistics_checkbox_->IsEnabled()) {
    usage_statistics_checkbox_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_OPTION_DISABLED_BY_POLICY));
  }

  // Set tooltip for system security settings link if TPM is disabled.
  if (!system_security_settings_link_->IsEnabled()) {
    system_security_settings_link_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_EULA_TPM_DISABLED));
  }

  // Load other labels from resources.
  usage_statistics_checkbox_->SetText(
      l10n_util::GetStringUTF16(IDS_EULA_CHECKBOX_ENABLE_LOGGING));
  learn_more_link_->SetText(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  system_security_settings_link_->SetText(
      l10n_util::GetStringUTF16(IDS_EULA_SYSTEM_SECURITY_SETTING));
  continue_button_->SetText(
      l10n_util::GetStringUTF16(IDS_EULA_ACCEPT_AND_CONTINUE_BUTTON));
  back_button_->SetText(l10n_util::GetStringUTF16(IDS_EULA_BACK_BUTTON));
}

bool EulaView::IsUsageStatsChecked() const {
  return usage_statistics_checkbox_ && usage_statistics_checkbox_->checked();
}

void EulaView::ShowTpmPassword(const std::string& tpm_password) {
  if (tpm_info_view_)
    tpm_info_view_->ShowTpmPassword(tpm_password);
}

void EulaView::OnTpmInfoViewClosed(TpmInfoView* tpm_info_view) {
  if (tpm_info_view == tpm_info_view_)
    tpm_info_view_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// EulaView, protected, views::View implementation:

void EulaView::OnLocaleChanged() {
  UpdateLocalizedStrings();
  Layout();
}

////////////////////////////////////////////////////////////////////////////////
// views::ButtonListener implementation:

void EulaView::ButtonPressed(views::Button* sender, const views::Event& event) {
  if (sender == continue_button_) {
    actor_->screen()->OnExit(true, IsUsageStatsChecked());
  } else if (sender == back_button_) {
    actor_->screen()->OnExit(false, IsUsageStatsChecked());
  }
}

////////////////////////////////////////////////////////////////////////////////
// views::LinkListener implementation:

void EulaView::LinkClicked(views::Link* source, int event_flags) {
  gfx::NativeWindow parent_window =
      LoginUtils::Get()->GetBackgroundView()->GetNativeWindow();
  if (source == learn_more_link_) {
    if (!help_app_.get())
      help_app_ = new HelpAppLauncher(parent_window);
    help_app_->ShowHelpTopic(HelpAppLauncher::HELP_STATS_USAGE);
  } else if (source == system_security_settings_link_) {
    tpm_info_view_ = new TpmInfoView(this);
    tpm_info_view_->Init();
    views::Widget* window = browser::CreateViewsWindow(parent_window,
                                                       tpm_info_view_);
    window->SetAlwaysOnTop(true);
    window->Show();
    actor_->screen()->InitiatePasswordFetch();
  }
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsDelegate implementation:

// Convenience function. Queries |eula_view| for HTML title and, if it
// is ready, assigns it to |eula_label| and returns true so the caller
// view calls Layout().
static bool PublishTitleIfReady(const TabContents* contents,
                                DOMView* eula_view,
                                views::Label* eula_label) {
  if (!eula_view->dom_contents())
    return false;
  TabContents* tab_contents = eula_view->dom_contents()->tab_contents();
  if (contents != tab_contents)
    return false;
  eula_label->SetText(tab_contents->GetTitle());
  return true;
}

void EulaView::NavigationStateChanged(const TabContents* contents,
                                      unsigned changed_flags) {
  if (changed_flags & TabContents::INVALIDATE_TITLE) {
    if (PublishTitleIfReady(contents, google_eula_view_, google_eula_label_) ||
        PublishTitleIfReady(contents, oem_eula_view_, oem_eula_label_)) {
      Layout();
    }
  }
}

void EulaView::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  HandleWebKeyboardEvent(GetWidget(), event);
}

////////////////////////////////////////////////////////////////////////////////
// EulaView, private:

void EulaView::LoadEulaView(DOMView* eula_view,
                            views::Label* eula_label,
                            const GURL& eula_url) {
  Profile* profile = ProfileManager::GetDefaultProfile();
  eula_view->Init(profile,
                  SiteInstance::CreateSiteInstanceForURL(profile, eula_url));
  eula_view->LoadURL(eula_url);
  eula_view->dom_contents()->tab_contents()->set_delegate(this);
}

////////////////////////////////////////////////////////////////////////////////
// EulaView, private, views::View implementation:

bool EulaView::SkipDefaultKeyEventProcessing(const views::KeyEvent& e) {
  return true;
}

bool EulaView::OnKeyPressed(const views::KeyEvent&) {
  // Close message bubble if shown. bubble_ will be set to NULL in callback.
  if (bubble_) {
    bubble_->Close();
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// EulaView, private, views::BubbleDelegate implementation:

void EulaView::BubbleClosing(Bubble* bubble, bool closed_by_escape) {
  bubble_ = NULL;
}

bool EulaView::CloseOnEscape() {
  return true;
}

bool EulaView::FadeInOnShow() {
  return false;
}

void EulaView::OnLinkActivated(size_t index) {
}

}  // namespace chromeos
