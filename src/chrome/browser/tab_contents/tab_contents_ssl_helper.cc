// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_contents_ssl_helper.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_add_cert_handler.h"
#include "chrome/browser/ssl_client_certificate_selector.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/tab_contents/simple_alert_infobar_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/ssl/ssl_client_auth_handler.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

gfx::Image* GetCertIcon() {
  // TODO(davidben): use a more appropriate icon.
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_SAVE_PASSWORD);
}

bool CertMatchesFilter(const net::X509Certificate& cert,
                       const base::DictionaryValue& filter) {
  // TODO(markusheintz): This is the minimal required filter implementation.
  // Implement a better matcher.

  // An empty filter matches any client certificate since no requirements are
  // specified at all.
  if (filter.empty())
    return true;

  std::string common_name;
  if (filter.GetString("ISSUER.CN", &common_name) &&
      (cert.issuer().common_name == common_name)) {
    return true;
  }
  return false;
}

// SSLCertAddedInfoBarDelegate ------------------------------------------------

class SSLCertAddedInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  SSLCertAddedInfoBarDelegate(InfoBarTabHelper* infobar_helper,
                              net::X509Certificate* cert);

 private:
  virtual ~SSLCertAddedInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;

  scoped_refptr<net::X509Certificate> cert_;  // The cert we added.
};

SSLCertAddedInfoBarDelegate::SSLCertAddedInfoBarDelegate(
    InfoBarTabHelper* infobar_helper,
    net::X509Certificate* cert)
    : ConfirmInfoBarDelegate(infobar_helper),
      cert_(cert) {
}

SSLCertAddedInfoBarDelegate::~SSLCertAddedInfoBarDelegate() {
}

gfx::Image* SSLCertAddedInfoBarDelegate::GetIcon() const {
  return GetCertIcon();
}

InfoBarDelegate::Type SSLCertAddedInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 SSLCertAddedInfoBarDelegate::GetMessageText() const {
  // TODO(evanm): GetDisplayName should return UTF-16.
  return l10n_util::GetStringFUTF16(IDS_ADD_CERT_SUCCESS_INFOBAR_LABEL,
      UTF8ToUTF16(cert_->issuer().GetDisplayName()));
}

int SSLCertAddedInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

string16 SSLCertAddedInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_ADD_CERT_SUCCESS_INFOBAR_BUTTON);
}

bool SSLCertAddedInfoBarDelegate::Accept() {
  ShowCertificateViewer(owner()->tab_contents()->GetDialogRootWindow(), cert_);
  return false;  // Hiding the infobar just as the dialog opens looks weird.
}

}  // namespace


// TabContentsSSLHelper::SSLAddCertData ---------------------------------------

class TabContentsSSLHelper::SSLAddCertData : public NotificationObserver {
 public:
  explicit SSLAddCertData(TabContentsWrapper* tab_contents);
  virtual ~SSLAddCertData();

  // Displays |delegate| as an infobar in |tab_|, replacing our current one if
  // still active.
  void ShowInfoBar(InfoBarDelegate* delegate);

  // Same as above, for the common case of wanting to show a simple alert
  // message.
  void ShowErrorInfoBar(const string16& message);

 private:
  // NotificationObserver:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  TabContentsWrapper* tab_contents_;
  InfoBarDelegate* infobar_delegate_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(SSLAddCertData);
};

TabContentsSSLHelper::SSLAddCertData::SSLAddCertData(
    TabContentsWrapper* tab_contents)
    : tab_contents_(tab_contents),
      infobar_delegate_(NULL) {
  Source<InfoBarTabHelper> source(tab_contents_->infobar_tab_helper());
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                 source);
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REPLACED,
                 source);
}

TabContentsSSLHelper::SSLAddCertData::~SSLAddCertData() {
}

void TabContentsSSLHelper::SSLAddCertData::ShowInfoBar(
    InfoBarDelegate* delegate) {
  if (infobar_delegate_)
    tab_contents_->infobar_tab_helper()->ReplaceInfoBar(infobar_delegate_,
                                                        delegate);
  else
    tab_contents_->infobar_tab_helper()->AddInfoBar(delegate);
  infobar_delegate_ = delegate;
}

void TabContentsSSLHelper::SSLAddCertData::ShowErrorInfoBar(
    const string16& message) {
  ShowInfoBar(new SimpleAlertInfoBarDelegate(
      tab_contents_->infobar_tab_helper(), GetCertIcon(), message, true));
}

void TabContentsSSLHelper::SSLAddCertData::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED ||
         type == chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REPLACED);
  if (infobar_delegate_ ==
      ((type == chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED) ?
          Details<InfoBarRemovedDetails>(details)->first :
          Details<InfoBarReplacedDetails>(details)->first))
    infobar_delegate_ = NULL;
}


// TabContentsSSLHelper -------------------------------------------------------

TabContentsSSLHelper::TabContentsSSLHelper(TabContentsWrapper* tab_contents)
    : tab_contents_(tab_contents) {
}

TabContentsSSLHelper::~TabContentsSSLHelper() {
}

void TabContentsSSLHelper::SelectClientCertificate(
    scoped_refptr<SSLClientAuthHandler> handler) {
  net::SSLCertRequestInfo* cert_request_info = handler->cert_request_info();
  GURL requesting_url("https://" + cert_request_info->host_and_port);
  DCHECK(requesting_url.is_valid()) << "Invalid URL string: https://"
                                    << cert_request_info->host_and_port;

  HostContentSettingsMap* map =
      tab_contents_->profile()->GetHostContentSettingsMap();
  scoped_ptr<Value> filter(map->GetContentSettingValue(
      requesting_url, requesting_url,
      CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
      std::string(),
      NULL, NULL));

  scoped_refptr<net::X509Certificate> selected_cert;
  if (filter.get()) {
    // Try to automatically select a client certificate.
    if (filter->IsType(Value::TYPE_DICTIONARY)) {
      DictionaryValue* filter_dict =
          static_cast<DictionaryValue*>(filter.get());

      const std::vector<scoped_refptr<net::X509Certificate> >&
          all_client_certs = cert_request_info->client_certs;
      for (size_t i = 0; i < all_client_certs.size(); ++i) {
        if (CertMatchesFilter(*all_client_certs[i], *filter_dict)) {
          selected_cert = all_client_certs[i];
          // Use the first certificate that is matched by the filter.
          break;
        }
      }
    } else {
      NOTREACHED();
    }
  }

  if (selected_cert) {
    handler->CertificateSelected(selected_cert);
  } else {
    ShowClientCertificateRequestDialog(handler);
  }
}

void TabContentsSSLHelper::ShowClientCertificateRequestDialog(
    scoped_refptr<SSLClientAuthHandler> handler) {
  // TODO(rhashimoto): WebUI certificate selector for touch.
#if !defined(TOUCH_UI)
  browser::ShowSSLClientCertificateSelector(
      tab_contents_, handler->cert_request_info(), handler);
#endif
}

void TabContentsSSLHelper::OnVerifyClientCertificateError(
    scoped_refptr<SSLAddCertHandler> handler, int error_code) {
  SSLAddCertData* add_cert_data = GetAddCertData(handler);
  // Display an infobar with the error message.
  // TODO(davidben): Display a more user-friendly error string.
  add_cert_data->ShowErrorInfoBar(
      l10n_util::GetStringFUTF16(IDS_ADD_CERT_ERR_INVALID_CERT,
                                 base::IntToString16(-error_code),
                                 ASCIIToUTF16(net::ErrorToString(error_code))));
}

void TabContentsSSLHelper::AskToAddClientCertificate(
    scoped_refptr<SSLAddCertHandler> handler) {
  NOTREACHED();  // Not implemented yet.
}

void TabContentsSSLHelper::OnAddClientCertificateSuccess(
    scoped_refptr<SSLAddCertHandler> handler) {
  SSLAddCertData* add_cert_data = GetAddCertData(handler);
  // Display an infobar to inform the user.
  add_cert_data->ShowInfoBar(new SSLCertAddedInfoBarDelegate(
      tab_contents_->infobar_tab_helper(), handler->cert()));
}

void TabContentsSSLHelper::OnAddClientCertificateError(
    scoped_refptr<SSLAddCertHandler> handler, int error_code) {
  SSLAddCertData* add_cert_data = GetAddCertData(handler);
  // Display an infobar with the error message.
  // TODO(davidben): Display a more user-friendly error string.
  add_cert_data->ShowErrorInfoBar(
      l10n_util::GetStringFUTF16(IDS_ADD_CERT_ERR_FAILED,
                                 base::IntToString16(-error_code),
                                 ASCIIToUTF16(net::ErrorToString(error_code))));
}

void TabContentsSSLHelper::OnAddClientCertificateFinished(
    scoped_refptr<SSLAddCertHandler> handler) {
  // Clean up.
  request_id_to_add_cert_data_.erase(handler->network_request_id());
}

TabContentsSSLHelper::SSLAddCertData* TabContentsSSLHelper::GetAddCertData(
    SSLAddCertHandler* handler) {
  // Find/create the slot.
  linked_ptr<SSLAddCertData>& ptr_ref =
      request_id_to_add_cert_data_[handler->network_request_id()];
  // Fill it if necessary.
  if (!ptr_ref.get())
    ptr_ref.reset(new SSLAddCertData(tab_contents_));
  return ptr_ref.get();
}
