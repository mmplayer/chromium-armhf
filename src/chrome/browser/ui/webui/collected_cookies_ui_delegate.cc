// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/collected_cookies_ui_delegate.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/cookies_tree_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/constrained_html_ui.h"
#include "chrome/browser/ui/webui/cookies_tree_model_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/size.h"

namespace {

// TODO(xiyuan): Localize this.
const int kDialogWidth = 480;
const int kDialogHeight = 470;

CookieTreeOriginNode* GetOriginNode(CookiesTreeModel* model,
                                    const std::string& node_path) {
  CookieTreeNode* node = cookies_tree_model_util::GetTreeNodeFromPath(
      model->GetRoot(), node_path);

  while (node && node->GetDetailedInfo().node_type !=
                 CookieTreeNode::DetailedInfo::TYPE_ORIGIN) {
    node = node->parent();
  }

  return static_cast<CookieTreeOriginNode*>(node);
}

std::string GetInfobarLabel(ContentSetting setting,
                            const string16& domain_name) {
  switch (setting) {
    case CONTENT_SETTING_BLOCK:
      return l10n_util::GetStringFUTF8(
          IDS_COLLECTED_COOKIES_BLOCK_RULE_CREATED, domain_name);

    case CONTENT_SETTING_ALLOW:
      return l10n_util::GetStringFUTF8(
          IDS_COLLECTED_COOKIES_ALLOW_RULE_CREATED, domain_name);

    case CONTENT_SETTING_SESSION_ONLY:
      return l10n_util::GetStringFUTF8(
          IDS_COLLECTED_COOKIES_SESSION_RULE_CREATED, domain_name);

    default:
      NOTREACHED() << "Unknown ContentSetting, " << setting;
      return std::string();
  }
}

class CollectedCookiesSource : public ChromeURLDataManager::DataSource {
 public:
  explicit CollectedCookiesSource(bool block_third_party_cookies)
      : DataSource(chrome::kChromeUICollectedCookiesHost,
                   MessageLoop::current()),
        block_third_party_cookies_(block_third_party_cookies) {
  }

  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);

  virtual std::string GetMimeType(const std::string& path) const {
    return "text/html";
  }

 private:
  virtual ~CollectedCookiesSource() {}

  bool block_third_party_cookies_;

  DISALLOW_COPY_AND_ASSIGN(CollectedCookiesSource);
};

void CollectedCookiesSource::StartDataRequest(const std::string& path,
                                              bool is_incognito,
                                              int request_id) {
  DictionaryValue localized_strings;
  localized_strings.SetString("title",
      l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_DIALOG_TITLE));

  localized_strings.SetString("title",
      l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_DIALOG_TITLE));
  localized_strings.SetString("allowedCookies",
      l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_ALLOWED_COOKIES_LABEL));
  localized_strings.SetString("blockButton",
      l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_BLOCK_BUTTON));

  localized_strings.SetString("blockedCookies",
      l10n_util::GetStringUTF16(block_third_party_cookies_ ?
          IDS_COLLECTED_COOKIES_BLOCKED_THIRD_PARTY_BLOCKING_ENABLED :
          IDS_COLLECTED_COOKIES_BLOCKED_COOKIES_LABEL));

  localized_strings.SetString("allowButton",
      l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_ALLOW_BUTTON));
  localized_strings.SetString("allowThisSessionButton",
      l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_SESSION_ONLY_BUTTON));
  localized_strings.SetString("closeButton",
      l10n_util::GetStringUTF16(IDS_CLOSE));

  SetFontAndTextDirection(&localized_strings);

  static const base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_COLLECTED_COOKIES_HTML));
  std::string response = jstemplate_builder::GetI18nTemplateHtml(
      html, &localized_strings);

  SendResponse(request_id, base::RefCountedString::TakeString(&response));
}

}  // namespace

namespace browser {

// Declared in browser_dialogs.h so others don't have to depend on our header.
void ShowCollectedCookiesDialog(gfx::NativeWindow parent_window,
                                TabContentsWrapper* wrapper) {
  CollectedCookiesUIDelegate::Show(wrapper);
}

}  // namespace browser

// static
void CollectedCookiesUIDelegate::Show(TabContentsWrapper* wrapper) {
  CollectedCookiesUIDelegate* delegate =
      new CollectedCookiesUIDelegate(wrapper);
  Profile* profile = wrapper->profile();
  ConstrainedHtmlUI::CreateConstrainedHtmlDialog(profile,
                                                 delegate,
                                                 wrapper);
}

CollectedCookiesUIDelegate::CollectedCookiesUIDelegate(
    TabContentsWrapper* wrapper)
    : wrapper_(wrapper),
      closed_(false) {
  TabSpecificContentSettings* content_settings = wrapper->content_settings();
  HostContentSettingsMap* host_content_settings_map =
      wrapper->profile()->GetHostContentSettingsMap();

  registrar_.Add(this, chrome::NOTIFICATION_COLLECTED_COOKIES_SHOWN,
                 Source<TabSpecificContentSettings>(content_settings));

  allowed_cookies_tree_model_.reset(
      content_settings->GetAllowedCookiesTreeModel());
  blocked_cookies_tree_model_.reset(
      content_settings->GetBlockedCookiesTreeModel());

  CollectedCookiesSource* source = new CollectedCookiesSource(
      host_content_settings_map->BlockThirdPartyCookies());
  wrapper->profile()->GetChromeURLDataManager()->AddDataSource(source);
}

CollectedCookiesUIDelegate::~CollectedCookiesUIDelegate() {
}

bool CollectedCookiesUIDelegate::IsDialogModal() const {
  return false;
}

string16 CollectedCookiesUIDelegate::GetDialogTitle() const {
  return string16();
}

GURL CollectedCookiesUIDelegate::GetDialogContentURL() const {
  return GURL(chrome::kChromeUICollectedCookiesURL);
}

void CollectedCookiesUIDelegate::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  const WebUIMessageHandler* handler = this;
  handlers->push_back(const_cast<WebUIMessageHandler*>(handler));
}

void CollectedCookiesUIDelegate::GetDialogSize(gfx::Size* size) const {
  size->set_width(kDialogWidth);
  size->set_height(kDialogHeight);
}

std::string CollectedCookiesUIDelegate::GetDialogArgs() const {
  return std::string();
}

void CollectedCookiesUIDelegate::OnDialogClosed(
    const std::string& json_retval) {
  closed_ = true;
}

bool CollectedCookiesUIDelegate::ShouldShowDialogTitle() const {
  return true;
}

void CollectedCookiesUIDelegate::RegisterMessages() {
  web_ui_->RegisterMessageCallback("BindCookiesTreeModel",
      base::Bind(&CollectedCookiesUIDelegate::BindCookiesTreeModel,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("Block",
      base::Bind(&CollectedCookiesUIDelegate::Block,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("Allow",
      base::Bind(&CollectedCookiesUIDelegate::Allow,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("AllowThisSession",
      base::Bind(&CollectedCookiesUIDelegate::AllowThisSession,
                 base::Unretained(this)));

  allowed_cookies_adapter_.Init(web_ui_);
  blocked_cookies_adapter_.Init(web_ui_);
}

void CollectedCookiesUIDelegate::CloseDialog() {
  if (!closed_ && web_ui_)
    web_ui_->CallJavascriptFunction("closeDialog");
}

void CollectedCookiesUIDelegate::SetInfobarLabel(const std::string& text) {
  StringValue string(text);
  web_ui_->CallJavascriptFunction("setInfobarLabel", string);
}

void CollectedCookiesUIDelegate::AddContentException(
    CookieTreeOriginNode* origin_node, ContentSetting setting) {
  if (origin_node->CanCreateContentException()) {
    Profile* profile = wrapper_->profile();
    origin_node->CreateContentException(profile->GetHostContentSettingsMap(),
                                        setting);

    SetInfobarLabel(GetInfobarLabel(setting, origin_node->GetTitle()));
  }
}

void CollectedCookiesUIDelegate::Observe(int type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_COLLECTED_COOKIES_SHOWN);
  CloseDialog();
}

void CollectedCookiesUIDelegate::BindCookiesTreeModel(const ListValue* args) {
  allowed_cookies_adapter_.Bind("allowed-cookies",
      allowed_cookies_tree_model_.get());
  blocked_cookies_adapter_.Bind("blocked-cookies",
      blocked_cookies_tree_model_.get());
}

void CollectedCookiesUIDelegate::Block(const ListValue* args) {
  std::string node_path;
  if (!args->GetString(0, &node_path))
    return;

  CookieTreeOriginNode* origin_node = GetOriginNode(
      allowed_cookies_tree_model_.get(), node_path);
  if (!origin_node)
    return;

  AddContentException(origin_node, CONTENT_SETTING_BLOCK);
}

void CollectedCookiesUIDelegate::Allow(const ListValue* args) {
  std::string node_path;
  if (!args->GetString(0, &node_path))
    return;

  CookieTreeOriginNode* origin_node = GetOriginNode(
      blocked_cookies_tree_model_.get(), node_path);
  if (!origin_node)
    return;

  AddContentException(origin_node, CONTENT_SETTING_ALLOW);
}

void CollectedCookiesUIDelegate::AllowThisSession(const ListValue* args) {
  std::string node_path;
  if (!args->GetString(0, &node_path))
    return;

  CookieTreeOriginNode* origin_node = GetOriginNode(
      blocked_cookies_tree_model_.get(), node_path);
  if (!origin_node)
    return;

  AddContentException(origin_node, CONTENT_SETTING_SESSION_ONLY);
}
