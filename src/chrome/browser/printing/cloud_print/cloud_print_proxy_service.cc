// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"

#include <stack>
#include <vector>

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_flow.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/service/service_process_control.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/cloud_print/cloud_print_proxy_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/service_messages.h"
#include "content/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

// TODO(sanjeevr): Localize the product name?
const char kCloudPrintProductName[] = "Google Cloud Print";

class CloudPrintProxyService::TokenExpiredNotificationDelegate
    : public NotificationDelegate {
 public:
  explicit TokenExpiredNotificationDelegate(
      CloudPrintProxyService* cloud_print_service)
          : cloud_print_service_(cloud_print_service) {
  }
  void Display() {}
  void Error() {
    cloud_print_service_->OnTokenExpiredNotificationError();
  }
  void Close(bool by_user) {
    cloud_print_service_->OnTokenExpiredNotificationClosed(by_user);
  }
  void Click() {
    cloud_print_service_->OnTokenExpiredNotificationClick();
  }
  std::string id() const { return "cloudprint.tokenexpired"; }

 private:
  CloudPrintProxyService* cloud_print_service_;
  DISALLOW_COPY_AND_ASSIGN(TokenExpiredNotificationDelegate);
};

CloudPrintProxyService::CloudPrintProxyService(Profile* profile)
    : profile_(profile),
      token_expired_delegate_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(service_task_factory_(this)) {
}

CloudPrintProxyService::~CloudPrintProxyService() {
}

void CloudPrintProxyService::Initialize() {
  if (profile_->GetPrefs()->HasPrefPath(prefs::kCloudPrintEmail) &&
      !profile_->GetPrefs()->GetString(prefs::kCloudPrintEmail).empty()) {
    // If the cloud print proxy is enabled, establish a channel with the
    // service process and update the status.
    RefreshStatusFromService();
  }
}

void CloudPrintProxyService::RefreshStatusFromService() {
  InvokeServiceTask(
      service_task_factory_.NewRunnableMethod(
          &CloudPrintProxyService::RefreshCloudPrintProxyStatus));
}

void CloudPrintProxyService::EnableForUser(const std::string& lsid,
                                           const std::string& email) {
  InvokeServiceTask(
      service_task_factory_.NewRunnableMethod(
          &CloudPrintProxyService::EnableCloudPrintProxy, lsid, email));
}

void CloudPrintProxyService::EnableForUserWithRobot(
    const std::string& robot_auth_code,
    const std::string& robot_email,
    const std::string& user_email) {
  InvokeServiceTask(
      service_task_factory_.NewRunnableMethod(
          &CloudPrintProxyService::EnableCloudPrintProxyWithRobot,
          robot_auth_code,
          robot_email,
          user_email));
}


void CloudPrintProxyService::DisableForUser() {
  InvokeServiceTask(
      service_task_factory_.NewRunnableMethod(
          &CloudPrintProxyService::DisableCloudPrintProxy));
}

bool CloudPrintProxyService::ShowTokenExpiredNotification() {
  // If we already have a pending notification, don't show another one.
  if (token_expired_delegate_.get())
    return false;

  // TODO(sanjeevr): Get icon for this notification.
  GURL icon_url;

  string16 title = UTF8ToUTF16(kCloudPrintProductName);
  string16 message =
      l10n_util::GetStringUTF16(IDS_CLOUD_PRINT_TOKEN_EXPIRED_MESSAGE);
  string16 content_url = DesktopNotificationService::CreateDataUrl(
      icon_url, title, message, WebKit::WebTextDirectionDefault);
  token_expired_delegate_ = new TokenExpiredNotificationDelegate(this);
  Notification notification(GURL(), GURL(content_url), string16(), string16(),
                            token_expired_delegate_.get());
  g_browser_process->notification_ui_manager()->Add(notification, profile_);
  // Keep the browser alive while we are showing the notification.
  BrowserList::StartKeepAlive();
  return true;
}

void CloudPrintProxyService::OnTokenExpiredNotificationError() {
  TokenExpiredNotificationDone(false);
}

void CloudPrintProxyService::OnTokenExpiredNotificationClosed(bool by_user) {
  TokenExpiredNotificationDone(false);
}

void CloudPrintProxyService::OnTokenExpiredNotificationClick() {
  TokenExpiredNotificationDone(true);
  // Clear the cached cloud print email pref so that the cloud print setup
  // flow happens.
  profile_->GetPrefs()->SetString(prefs::kCloudPrintEmail, std::string());
  cloud_print_setup_handler_.reset(new CloudPrintSetupHandler(this));
  CloudPrintSetupFlow::OpenDialog(
      profile_, cloud_print_setup_handler_->AsWeakPtr(), NULL);
}

void CloudPrintProxyService::TokenExpiredNotificationDone(bool keep_alive) {
  if (token_expired_delegate_.get()) {
    g_browser_process->notification_ui_manager()->CancelById(
        token_expired_delegate_->id());
    token_expired_delegate_ = NULL;
    if (!keep_alive)
      BrowserList::EndKeepAlive();
  }
}

void CloudPrintProxyService::OnCloudPrintSetupClosed() {
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableFunction(&BrowserList::EndKeepAlive));
}

void CloudPrintProxyService::RefreshCloudPrintProxyStatus() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ServiceProcessControl* process_control = ServiceProcessControl::GetInstance();
  DCHECK(process_control->is_connected());
  ServiceProcessControl::CloudPrintProxyInfoHandler* callback =
       NewCallback(this, &CloudPrintProxyService::ProxyInfoCallback);
  // GetCloudPrintProxyInfo takes ownership of callback.
  process_control->GetCloudPrintProxyInfo(callback);
}

void CloudPrintProxyService::EnableCloudPrintProxy(const std::string& lsid,
                                                   const std::string& email) {
  ServiceProcessControl* process_control = ServiceProcessControl::GetInstance();
  DCHECK(process_control->is_connected());
  process_control->Send(new ServiceMsg_EnableCloudPrintProxy(lsid));
  // Assume the IPC worked.
  profile_->GetPrefs()->SetString(prefs::kCloudPrintEmail, email);
}

void CloudPrintProxyService::EnableCloudPrintProxyWithRobot(
    const std::string& robot_auth_code,
    const std::string& robot_email,
    const std::string& user_email) {
  ServiceProcessControl* process_control = ServiceProcessControl::GetInstance();
  DCHECK(process_control->is_connected());
  process_control->Send(new ServiceMsg_EnableCloudPrintProxyWithRobot(
      robot_auth_code,
      robot_email,
      user_email));
  // Assume the IPC worked.
  profile_->GetPrefs()->SetString(prefs::kCloudPrintEmail, user_email);
}


void CloudPrintProxyService::DisableCloudPrintProxy() {
  ServiceProcessControl* process_control = ServiceProcessControl::GetInstance();
  DCHECK(process_control->is_connected());
  process_control->Send(new ServiceMsg_DisableCloudPrintProxy);
  // Assume the IPC worked.
  profile_->GetPrefs()->SetString(prefs::kCloudPrintEmail, std::string());
}

void CloudPrintProxyService::ProxyInfoCallback(
    const cloud_print::CloudPrintProxyInfo& proxy_info) {
  proxy_id_ = proxy_info.proxy_id;
  profile_->GetPrefs()->SetString(
      prefs::kCloudPrintEmail,
      proxy_info.enabled ? proxy_info.email : std::string());
}

bool CloudPrintProxyService::InvokeServiceTask(Task* task) {
  ServiceProcessControl::GetInstance()->Launch(task, NULL);
  return true;
}
