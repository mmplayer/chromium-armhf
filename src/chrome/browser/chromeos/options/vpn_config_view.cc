// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/vpn_config_view.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/cert_database.h"
#include "net/base/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"

namespace {

// Root CA certificates that are built into Chrome use this token name.
const char* const kRootCertificateTokenName = "Builtin Object Token";

string16 ProviderTypeToString(chromeos::ProviderType type) {
  switch (type) {
    case chromeos::PROVIDER_TYPE_L2TP_IPSEC_PSK:
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_L2TP_IPSEC_PSK);
    case chromeos::PROVIDER_TYPE_L2TP_IPSEC_USER_CERT:
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_L2TP_IPSEC_USER_CERT);
    case chromeos::PROVIDER_TYPE_OPEN_VPN:
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_OPEN_VPN);
    case chromeos::PROVIDER_TYPE_MAX:
      break;
  }
  NOTREACHED();
  return string16();
}

}  // namespace

namespace chromeos {

class ProviderTypeComboboxModel : public ui::ComboboxModel {
 public:
  ProviderTypeComboboxModel() {}
  virtual ~ProviderTypeComboboxModel() {}
  virtual int GetItemCount() {
    // Exclude OpenVPN temporarily. See crosbug.com/23489
    return chromeos::PROVIDER_TYPE_MAX - 1;
  }
  virtual string16 GetItemAt(int index) {
    ProviderType type = static_cast<ProviderType>(index);
    return ProviderTypeToString(type);
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(ProviderTypeComboboxModel);
};

class ServerCACertComboboxModel : public ui::ComboboxModel {
 public:
  explicit ServerCACertComboboxModel(CertLibrary* cert_library)
      : cert_library_(cert_library) {
  }
  virtual ~ServerCACertComboboxModel() {}
  virtual int GetItemCount() {
    if (!cert_library_->CertificatesLoaded())
      return 1;  // "Loading"
    // "Default" + certs.
    return cert_library_->GetCACertificates().Size() + 1;
  }
  virtual string16 GetItemAt(int combo_index) {
    if (!cert_library_->CertificatesLoaded())
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_LOADING);
    if (combo_index == 0)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_SERVER_CA_DEFAULT);
    int cert_index = combo_index - 1;
    return cert_library_->GetCACertificates().GetDisplayStringAt(cert_index);
  }

 private:
  CertLibrary* cert_library_;
  DISALLOW_COPY_AND_ASSIGN(ServerCACertComboboxModel);
};

class UserCertComboboxModel : public ui::ComboboxModel {
 public:
  explicit UserCertComboboxModel(CertLibrary* cert_library)
      : cert_library_(cert_library) {
  }
  virtual ~UserCertComboboxModel() {}
  virtual int GetItemCount() {
    if (!cert_library_->CertificatesLoaded())
      return 1;  // "Loading"
    int num_certs = cert_library_->GetUserCertificates().Size();
    if (num_certs == 0)
      return 1;  // "None installed"
    return num_certs;
  }
  virtual string16 GetItemAt(int combo_index) {
    if (!cert_library_->CertificatesLoaded())
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_LOADING);
    if (cert_library_->GetUserCertificates().Size() == 0)
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_USER_CERT_NONE_INSTALLED);
    return cert_library_->GetUserCertificates().GetDisplayStringAt(combo_index);
  }

 private:
  CertLibrary* cert_library_;
  DISALLOW_COPY_AND_ASSIGN(UserCertComboboxModel);
};

VPNConfigView::VPNConfigView(NetworkConfigView* parent, VirtualNetwork* vpn)
    : ChildNetworkConfigView(parent, vpn),
      cert_library_(NULL) {
  Init(vpn);
}

VPNConfigView::VPNConfigView(NetworkConfigView* parent)
    : ChildNetworkConfigView(parent),
      cert_library_(NULL) {
  Init(NULL);
}

VPNConfigView::~VPNConfigView() {
  if (cert_library_)
    cert_library_->RemoveObserver(this);
}

string16 VPNConfigView::GetTitle() {
  if (service_path_.empty())
    return l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_ADD_VPN);
  else
    return l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_JOIN_VPN);
}

bool VPNConfigView::CanLogin() {
  // Username is always required.
  if (GetUsername().empty())
    return false;

  // TODO(stevenjb): min kMinPassphraseLen length?
  if (service_path_.empty() &&
      (GetService().empty() || GetServer().empty()))
    return false;

  // Block login if certs are required but user has none.
  if (UserCertRequired() && (!HaveUserCerts() || !IsUserCertValid()))
    return false;

  return true;
}

void VPNConfigView::ContentsChanged(views::Textfield* sender,
                                    const string16& new_contents) {
  if (sender == server_textfield_ && !service_text_modified_) {
    // Set the service name to the server name up to '.', unless it has
    // been explicitly set by the user.
    string16 server = server_textfield_->text();
    string16::size_type n = server.find_first_of(L'.');
    service_name_from_server_ = server.substr(0, n);
    service_textfield_->SetText(service_name_from_server_);
  }
  if (sender == service_textfield_) {
    if (new_contents.empty())
      service_text_modified_ = false;
    else if (new_contents != service_name_from_server_)
      service_text_modified_ = true;
  }
  UpdateCanLogin();
}

bool VPNConfigView::HandleKeyEvent(views::Textfield* sender,
                                   const views::KeyEvent& key_event) {
  if ((sender == psk_passphrase_textfield_ ||
       sender == user_passphrase_textfield_) &&
      key_event.key_code() == ui::VKEY_RETURN) {
    parent_->GetDialogClientView()->AcceptWindow();
  }
  return false;
}

void VPNConfigView::ButtonPressed(views::Button* sender,
                                  const views::Event& event) {
}

void VPNConfigView::ItemChanged(views::Combobox* combo_box,
                                int prev_index, int new_index) {
  if (prev_index == new_index)
    return;
  if (combo_box == provider_type_combobox_) {
    provider_type_ = static_cast<ProviderType>(new_index);
    UpdateControls();
  } else if (combo_box == user_cert_combobox_ ||
             combo_box == server_ca_cert_combobox_) {
    // Do nothing.
  } else {
    NOTREACHED();
  }
  UpdateErrorLabel();
  UpdateCanLogin();
}

void VPNConfigView::OnCertificatesLoaded(bool initial_load) {
  Refresh();
}

bool VPNConfigView::Login() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  if (service_path_.empty()) {
    NetworkLibrary::VPNConfigData config_data;
    switch (provider_type_) {
      case PROVIDER_TYPE_L2TP_IPSEC_PSK:
        config_data.psk = GetPSKPassphrase();
        config_data.username = GetUsername();
        config_data.user_passphrase = GetUserPassphrase();
        config_data.group_name = GetGroupName();
        break;
      case PROVIDER_TYPE_L2TP_IPSEC_USER_CERT: {
        config_data.server_ca_cert_nss_nickname = GetServerCACertNssNickname();
        config_data.client_cert_pkcs11_id = GetUserCertID();
        config_data.username = GetUsername();
        config_data.user_passphrase = GetUserPassphrase();
        config_data.group_name = GetGroupName();
        break;
      }
      case PROVIDER_TYPE_OPEN_VPN:
        config_data.server_ca_cert_nss_nickname = GetServerCACertNssNickname();
        config_data.client_cert_pkcs11_id = GetUserCertID();
        config_data.username = GetUsername();
        config_data.user_passphrase = GetUserPassphrase();
        config_data.otp = GetOTP();
        break;
      case PROVIDER_TYPE_MAX:
        break;
    }
    cros->ConnectToUnconfiguredVirtualNetwork(
        GetService(), GetServer(), provider_type_, config_data);
  } else {
    VirtualNetwork* vpn = cros->FindVirtualNetworkByPath(service_path_);
    if (!vpn) {
      // TODO(stevenjb): Add notification for this.
      LOG(WARNING) << "VPN no longer exists: " << service_path_;
      return true;  // Close dialog.
    }
    switch (provider_type_) {
      case PROVIDER_TYPE_L2TP_IPSEC_PSK:
        vpn->SetL2TPIPsecPSKCredentials(GetPSKPassphrase(),
                                        GetUsername(),
                                        GetUserPassphrase(),
                                        GetGroupName());
        break;
      case PROVIDER_TYPE_L2TP_IPSEC_USER_CERT: {
        vpn->SetL2TPIPsecCertCredentials(GetUserCertID(),
                                         GetUsername(),
                                         GetUserPassphrase(),
                                         GetGroupName());
        break;
      }
      case PROVIDER_TYPE_OPEN_VPN: {
        vpn->SetOpenVPNCredentials(GetUserCertID(),
                                   GetUsername(),
                                   GetUserPassphrase(),
                                   GetOTP());
        break;
      }
      case PROVIDER_TYPE_MAX:
        break;
    }
    cros->ConnectToVirtualNetwork(vpn);
  }
  // Connection failures are responsible for updating the UI, including
  // reopening dialogs.
  return true;  // Close dialog.
}

void VPNConfigView::Cancel() {
}

void VPNConfigView::InitFocus() {
  // Put focus in the first editable field.
  if (server_textfield_)
    server_textfield_->RequestFocus();
  else if (service_textfield_)
    service_textfield_->RequestFocus();
  else if (provider_type_combobox_)
    provider_type_combobox_->RequestFocus();
  else if (psk_passphrase_textfield_ && psk_passphrase_textfield_->IsEnabled())
    psk_passphrase_textfield_->RequestFocus();
  else if (user_cert_combobox_ && user_cert_combobox_->IsEnabled())
    user_cert_combobox_->RequestFocus();
  else if (server_ca_cert_combobox_ && server_ca_cert_combobox_->IsEnabled())
    server_ca_cert_combobox_->RequestFocus();
}

const std::string VPNConfigView::GetService() const {
  if (service_textfield_ != NULL)
    return GetTextFromField(service_textfield_, true);
  return service_path_;
}

const std::string VPNConfigView::GetServer() const {
  if (server_textfield_ != NULL)
    return GetTextFromField(server_textfield_, true);
  return server_hostname_;
}

const std::string VPNConfigView::GetPSKPassphrase() const {
  if (psk_passphrase_textfield_ &&
      psk_passphrase_textfield_->IsEnabled() &&
      psk_passphrase_textfield_->IsVisible())
    return GetTextFromField(psk_passphrase_textfield_, false);
  return std::string();
}

const std::string VPNConfigView::GetUsername() const {
  return GetTextFromField(username_textfield_, true);
}

const std::string VPNConfigView::GetUserPassphrase() const {
  return GetTextFromField(user_passphrase_textfield_, false);
}

const std::string VPNConfigView::GetGroupName() const {
  return GetTextFromField(group_name_textfield_, false);
}

const std::string VPNConfigView::GetOTP() const {
  return GetTextFromField(otp_textfield_, true);
}

const std::string VPNConfigView::GetServerCACertNssNickname() const {
  DCHECK(cert_library_);
  int selected =
      server_ca_cert_combobox_ ? server_ca_cert_combobox_->selected_item() : 0;
  if (selected == 0) {
    // First item is "Default".
    return std::string();
  } else {
    DCHECK(cert_library_);
    DCHECK_GT(cert_library_->GetCACertificates().Size(), 0);
    int cert_index = selected - 1;
    return cert_library_->GetCACertificates().GetNicknameAt(cert_index);
  }
}

const std::string VPNConfigView::GetUserCertID() const {
  DCHECK(cert_library_);
  if (!HaveUserCerts()) {
    return std::string();  // "None installed"
  } else {
    // Certificates are listed in the order they appear in the model.
    int selected =
        user_cert_combobox_ ? user_cert_combobox_->selected_item() : 0;
    return cert_library_->GetUserCertificates().GetPkcs11IdAt(selected);
  }
}

void VPNConfigView::Init(VirtualNetwork* vpn) {
  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  // VPN may require certificates, so always set the library and observe.
  cert_library_ = chromeos::CrosLibrary::Get()->GetCertLibrary();

  // Setup a callback if certificates are yet to be loaded.
  if (!cert_library_->CertificatesLoaded())
    cert_library_->AddObserver(this);

  int column_view_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_view_set_id);
  // Label.
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlSmallHorizontalSpacing);
  // Textfield, combobox.
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0,
                        ChildNetworkConfigView::kInputFieldMinWidth);
  column_set->AddPaddingColumn(0, views::kRelatedControlSmallHorizontalSpacing);

  // Initialize members.
  service_text_modified_ = false;
  if (vpn) {
    provider_type_ = vpn->provider_type();
    // Sets enable_* based on the provider type which we use to control
    // which controls to make visible.
    UpdateControlsToEnable();
  } else {
    // Set the default provider type.
    provider_type_ = chromeos::PROVIDER_TYPE_L2TP_IPSEC_PSK;
    // Provider Type is user selectable, so enable all controls during init.
    enable_psk_passphrase_ = true;
    enable_user_cert_ = true;
    enable_server_ca_cert_ = true;
    enable_otp_ = true;
    enable_group_name_ = true;
  }

  // Server label and input.
  // Only provide Server name when configuring a new VPN.
  if (!vpn) {
    layout->StartRow(0, column_view_set_id);
    layout->AddView(new views::Label(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_SERVER_HOSTNAME)));
    server_textfield_ = new views::Textfield(views::Textfield::STYLE_DEFAULT);
    server_textfield_->SetController(this);
    layout->AddView(server_textfield_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  } else {
    server_textfield_ = NULL;
  }

  // Service label and name or input.
  layout->StartRow(0, column_view_set_id);
  layout->AddView(new views::Label(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_SERVICE_NAME)));
  if (!vpn) {
    service_textfield_ = new views::Textfield(views::Textfield::STYLE_DEFAULT);
    service_textfield_->SetController(this);
    layout->AddView(service_textfield_);
    service_text_ = NULL;
  } else {
    service_text_ = new views::Label(ASCIIToUTF16(vpn->name()));
    service_text_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    layout->AddView(service_text_);
    service_textfield_ = NULL;
  }
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Provider type label and select.
  layout->StartRow(0, column_view_set_id);
  layout->AddView(new views::Label(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_PROVIDER_TYPE)));
  if (!vpn) {
    provider_type_combobox_ =
        new views::Combobox(new ProviderTypeComboboxModel());
    provider_type_combobox_->set_listener(this);
    layout->AddView(provider_type_combobox_);
    provider_type_text_label_ = NULL;
  } else {
    provider_type_text_label_ =
        new views::Label(ProviderTypeToString(provider_type_));
    provider_type_text_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    layout->AddView(provider_type_text_label_);
    provider_type_combobox_ = NULL;
  }
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // PSK passphrase label, input and visible button.
  if (enable_psk_passphrase_) {
    layout->StartRow(0, column_view_set_id);
    psk_passphrase_label_ =  new views::Label(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_PSK_PASSPHRASE));
    layout->AddView(psk_passphrase_label_);
    psk_passphrase_textfield_ = new views::Textfield(
        views::Textfield::STYLE_PASSWORD);
    psk_passphrase_textfield_->SetController(this);
    if (vpn && !vpn->psk_passphrase().empty())
      psk_passphrase_textfield_->SetText(UTF8ToUTF16(vpn->psk_passphrase()));
    layout->AddView(psk_passphrase_textfield_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  } else {
    psk_passphrase_label_ = NULL;
    psk_passphrase_textfield_ = NULL;
  }

  // Server CA certificate
  // Only provide Server CA when configuring a new VPN.
  if (!vpn) {
    layout->StartRow(0, column_view_set_id);
    server_ca_cert_label_ =
        new views::Label(l10n_util::GetStringUTF16(
            IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_SERVER_CA));
    layout->AddView(server_ca_cert_label_);
    ServerCACertComboboxModel* server_ca_cert_model =
        new ServerCACertComboboxModel(cert_library_);
    server_ca_cert_combobox_ = new views::Combobox(server_ca_cert_model);
    layout->AddView(server_ca_cert_combobox_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  } else {
    server_ca_cert_label_ = NULL;
    server_ca_cert_combobox_ = NULL;
  }

  // User certificate label and input.
  if (enable_user_cert_) {
    layout->StartRow(0, column_view_set_id);
    user_cert_label_ = new views::Label(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_USER_CERT));
    layout->AddView(user_cert_label_);
    UserCertComboboxModel* user_cert_model =
        new UserCertComboboxModel(cert_library_);
    user_cert_combobox_ = new views::Combobox(user_cert_model);
    user_cert_combobox_->set_listener(this);
    layout->AddView(user_cert_combobox_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  } else {
    user_cert_label_ = NULL;
    user_cert_combobox_ = NULL;
  }

  // Username label and input.
  layout->StartRow(0, column_view_set_id);
  layout->AddView(new views::Label(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_USERNAME)));
  username_textfield_ = new views::Textfield(views::Textfield::STYLE_DEFAULT);
  username_textfield_->SetController(this);
  if (vpn && !vpn->username().empty())
    username_textfield_->SetText(UTF8ToUTF16(vpn->username()));
  layout->AddView(username_textfield_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // User passphrase label, input and visble button.
  layout->StartRow(0, column_view_set_id);
  layout->AddView(new views::Label(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_USER_PASSPHRASE)));
  user_passphrase_textfield_ = new views::Textfield(
      views::Textfield::STYLE_PASSWORD);
  user_passphrase_textfield_->SetController(this);
  if (vpn && !vpn->user_passphrase().empty())
    user_passphrase_textfield_->SetText(UTF8ToUTF16(vpn->user_passphrase()));
  layout->AddView(user_passphrase_textfield_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // OTP label and input.
  if (enable_otp_) {
    layout->StartRow(0, column_view_set_id);
    otp_label_ = new views::Label(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_OTP));
    layout->AddView(otp_label_);
    otp_textfield_ = new views::Textfield(views::Textfield::STYLE_DEFAULT);
    otp_textfield_->SetController(this);
    layout->AddView(otp_textfield_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  } else {
    otp_label_ = NULL;
    otp_textfield_ = NULL;
  }

  // This is not currently implemented with no immediate plans to do so.
  // TODO(stevenjb,kmixter): Remove 'false &&' below if we implement this.
  // See http://crosbug.com/19252.
  const bool show_group_name = false && enable_group_name_;

  // Group Name label and input.
  if (show_group_name) {
    layout->StartRow(0, column_view_set_id);
    group_name_label_ = new views::Label(l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_GROUP_NAME));
    layout->AddView(group_name_label_);
    group_name_textfield_ =
        new views::Textfield(views::Textfield::STYLE_DEFAULT);
    group_name_textfield_->SetController(this);
    if (vpn && !vpn->group_name().empty())
      group_name_textfield_->SetText(UTF8ToUTF16(vpn->group_name()));
    layout->AddView(group_name_textfield_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  } else {
    group_name_label_ = NULL;
    group_name_textfield_ = NULL;
  }

  // Error label.
  layout->StartRow(0, column_view_set_id);
  layout->SkipColumns(1);
  error_label_ = new views::Label();
  error_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  error_label_->SetEnabledColor(SK_ColorRED);
  layout->AddView(error_label_);

  // Set or hide the UI, update comboboxes and error labels.
  Refresh();
}

void VPNConfigView::Refresh() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();

  UpdateControls();

  // Set certificate combo boxes.
  VirtualNetwork* vpn = cros->FindVirtualNetworkByPath(service_path_);
  if (server_ca_cert_combobox_) {
    server_ca_cert_combobox_->ModelChanged();
    if (server_ca_cert_combobox_->IsEnabled() &&
        (vpn && !vpn->ca_cert_nss().empty())) {
      // Select the current server CA certificate in the combobox.
      int cert_index = cert_library_->GetCACertificates().FindCertByNickname(
          vpn->ca_cert_nss());
      if (cert_index >= 0) {
        // Skip item for "Default"
        server_ca_cert_combobox_->SetSelectedItem(1 + cert_index);
      } else {
        server_ca_cert_combobox_->SetSelectedItem(0);
      }
    } else {
      server_ca_cert_combobox_->SetSelectedItem(0);
    }
  }

  if (user_cert_combobox_) {
    user_cert_combobox_->ModelChanged();
    if (user_cert_combobox_->IsEnabled() &&
        (vpn && !vpn->client_cert_id().empty())) {
      int cert_index = cert_library_->GetUserCertificates().FindCertByPkcs11Id(
          vpn->client_cert_id());
      if (cert_index >= 0)
        user_cert_combobox_->SetSelectedItem(cert_index);
      else
        user_cert_combobox_->SetSelectedItem(0);
    } else {
      user_cert_combobox_->SetSelectedItem(0);
    }
  }

  UpdateErrorLabel();
}

void VPNConfigView::UpdateControlsToEnable() {
  // Set which controls are enabled.
  enable_psk_passphrase_ = false;
  enable_user_cert_ = false;
  enable_server_ca_cert_ = false;
  enable_otp_ = false;
  enable_group_name_ = false;
  switch (provider_type_) {
    case PROVIDER_TYPE_L2TP_IPSEC_PSK:
      enable_psk_passphrase_ = true;
      enable_group_name_ = true;
      break;
    case PROVIDER_TYPE_L2TP_IPSEC_USER_CERT:
      enable_server_ca_cert_ = true;
      enable_user_cert_ = HaveUserCerts();
      enable_group_name_ = true;
      break;
    case PROVIDER_TYPE_OPEN_VPN:
      enable_server_ca_cert_ = true;
      enable_user_cert_ = HaveUserCerts();
      enable_otp_ = true;
      break;
    default:
      NOTREACHED();
      break;
  }
}

void VPNConfigView::UpdateControls() {
  UpdateControlsToEnable();

  if (psk_passphrase_label_)
    psk_passphrase_label_->SetEnabled(enable_psk_passphrase_);
  if (psk_passphrase_textfield_)
    psk_passphrase_textfield_->SetEnabled(enable_psk_passphrase_);

  if (user_cert_label_)
    user_cert_label_->SetEnabled(enable_user_cert_);
  if (user_cert_combobox_)
    user_cert_combobox_->SetEnabled(enable_user_cert_);

  if (server_ca_cert_label_)
    server_ca_cert_label_->SetEnabled(enable_server_ca_cert_);
  if (server_ca_cert_combobox_)
    server_ca_cert_combobox_->SetEnabled(enable_server_ca_cert_);

  if (otp_label_)
    otp_label_->SetEnabled(enable_otp_);
  if (otp_textfield_)
    otp_textfield_->SetEnabled(enable_otp_);

  if (group_name_label_)
    group_name_label_->SetEnabled(enable_group_name_);
  if (group_name_textfield_)
    group_name_textfield_->SetEnabled(enable_group_name_);
}

void VPNConfigView::UpdateErrorLabel() {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();

  // Error message.
  std::string error_msg;
  if (UserCertRequired() && cert_library_->CertificatesLoaded()) {
    if (!HaveUserCerts()) {
      error_msg = l10n_util::GetStringUTF8(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PLEASE_INSTALL_USER_CERT);
    } else if (!IsUserCertValid()) {
      error_msg = l10n_util::GetStringUTF8(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_REQUIRE_HARDWARE_BACKED);
    }
  }
  if (error_msg.empty() && !service_path_.empty()) {
    // TODO(kuan): differentiate between bad psk and user passphrases.
    VirtualNetwork* vpn = cros->FindVirtualNetworkByPath(service_path_);
    if (vpn && vpn->failed()) {
      if (vpn->error() == ERROR_BAD_PASSPHRASE) {
        error_msg = l10n_util::GetStringUTF8(
            IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_BAD_PASSPHRASE);
      } else {
        error_msg = vpn->GetErrorString();
      }
    }
  }
  if (!error_msg.empty()) {
    error_label_->SetText(UTF8ToUTF16(error_msg));
    error_label_->SetVisible(true);
  } else {
    error_label_->SetVisible(false);
  }
}

void VPNConfigView::UpdateCanLogin() {
  parent_->GetDialogClientView()->UpdateDialogButtons();
}

bool VPNConfigView::UserCertRequired() const {
  return provider_type_ == PROVIDER_TYPE_L2TP_IPSEC_USER_CERT
      || provider_type_ == PROVIDER_TYPE_OPEN_VPN;
}

bool VPNConfigView::HaveUserCerts() const {
  return cert_library_->GetUserCertificates().Size() > 0;
}

bool VPNConfigView::IsUserCertValid() const {
  if (!user_cert_combobox_ || !user_cert_combobox_->IsEnabled())
    return false;
  int selected = user_cert_combobox_->selected_item();
  if (selected < 0)
    return false;
  // Currently only hardware-backed user certificates are valid.
  if (cert_library_->IsHardwareBacked() &&
      !cert_library_->GetUserCertificates().IsHardwareBackedAt(selected))
    return false;
  return true;
}

const std::string VPNConfigView::GetTextFromField(
    views::Textfield* textfield, bool trim_whitespace) const {
  if (!textfield)
    return std::string();
  std::string untrimmed = UTF16ToUTF8(textfield->text());
  if (!trim_whitespace)
    return untrimmed;
  std::string result;
  TrimWhitespaceASCII(untrimmed, TRIM_ALL, &result);
  return result;
}

}  // namespace chromeos
