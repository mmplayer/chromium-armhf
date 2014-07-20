// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/login_library.h"

#include "base/message_loop.h"
#include "base/task.h"
#include "base/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/signed_settings.h"
#include "chrome/browser/chromeos/login/signed_settings_temp_storage.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"

namespace em = enterprise_management;
namespace chromeos {

LoginLibrary::~LoginLibrary() {}

class LoginLibraryImpl : public LoginLibrary {
 public:
  LoginLibraryImpl() {
  }

  virtual ~LoginLibraryImpl() {
    if (session_connection_) {
      DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
      chromeos::DisconnectSession(session_connection_);
    }
  }

  virtual void Init() OVERRIDE {
    DCHECK(CrosLibrary::Get()->libcros_loaded());
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    session_connection_ = chromeos::MonitorSession(&Handler, this);
  }

  virtual void EmitLoginPromptReady() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    chromeos::EmitLoginPromptReady();
  }

  virtual void EmitLoginPromptVisible() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    chromeos::EmitLoginPromptVisible();
  }

  virtual void RequestRetrievePolicy(
      RetrievePolicyCallback callback, void* delegate) OVERRIDE {
    DCHECK(callback) << "must provide a callback to RequestRetrievePolicy()";
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    chromeos::RetrievePolicy(callback, delegate);
  }

  virtual void RequestStorePolicy(const std::string& policy,
                                  StorePolicyCallback callback,
                                  void* delegate) OVERRIDE {
    DCHECK(callback) << "must provide a callback to StorePolicy()";
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    chromeos::StorePolicy(policy.c_str(), policy.length(), callback, delegate);
  }

  virtual void StartSession(
      const std::string& user_email,
      const std::string& unique_id /* unused */) OVERRIDE {
    // only pass unique_id through once we use it for something.
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    chromeos::StartSession(user_email.c_str(), "");
  }

  virtual void StopSession(const std::string& unique_id /* unused */) OVERRIDE {
    // only pass unique_id through once we use it for something.
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    chromeos::StopSession("");
  }

  virtual void RestartEntd() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    chromeos::RestartEntd();
  }

  virtual void RestartJob(int pid, const std::string& command_line) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    chromeos::RestartJob(pid, command_line.c_str());
  }

  class StubDelegate
      : public SignedSettings::Delegate<const em::PolicyFetchResponse&> {
   public:
    StubDelegate() : polfetcher_(NULL) {}
    virtual ~StubDelegate() {}
    void set_fetcher(SignedSettings* s) { polfetcher_ = s; }
    SignedSettings* fetcher() { return polfetcher_.get(); }
    // Implementation of SignedSettings::Delegate
    virtual void OnSettingsOpCompleted(SignedSettings::ReturnCode code,
                                       const em::PolicyFetchResponse& value) {
      VLOG(2) << "Done Fetching Policy";
      delete this;
    }
   private:
    scoped_refptr<SignedSettings> polfetcher_;
    DISALLOW_COPY_AND_ASSIGN(StubDelegate);
  };

  static void Handler(void* object, const OwnershipEvent& event) {
    LoginLibraryImpl* self = static_cast<LoginLibraryImpl*>(object);
    switch (event) {
      case SetKeySuccess:
        self->CompleteSetOwnerKey(true);
        break;
      case SetKeyFailure:
        self->CompleteSetOwnerKey(false);
        break;
      case WhitelistOpSuccess:
        self->CompleteWhitelistOp(true);
        break;
      case WhitelistOpFailure:
        self->CompleteWhitelistOp(false);
        break;
      case PropertyOpSuccess:
        self->CompletePropertyOp(true);
        break;
      case PropertyOpFailure:
        self->CompletePropertyOp(false);
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  void CompleteSetOwnerKey(bool value) {
    VLOG(1) << "Owner key generation: " << (value ? "success" : "fail");
    int result =
        chrome::NOTIFICATION_OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED;
    if (!value)
      result = chrome::NOTIFICATION_OWNER_KEY_FETCH_ATTEMPT_FAILED;

    // Whether we exported the public key or not, send a notification indicating
    // that we're done with this attempt.
    NotificationService::current()->Notify(result,
                                           NotificationService::AllSources(),
                                           NotificationService::NoDetails());

    // We stored some settings in transient storage before owner was assigned.
    // Now owner is assigned and key is generated and we should persist
    // those settings into signed storage.
    if (g_browser_process && g_browser_process->local_state()) {
      SignedSettingsTempStorage::Finalize(g_browser_process->local_state());
    }
  }

  void CompleteWhitelistOp(bool result) {
    // DEPRECATED.
  }

  void CompletePropertyOp(bool result) {
    if (result) {
      StubDelegate* stub = new StubDelegate();  // Manages its own lifetime.
      stub->set_fetcher(SignedSettings::CreateRetrievePolicyOp(stub));
      stub->fetcher()->Execute();
    }
  }

  chromeos::SessionConnection session_connection_;

  DISALLOW_COPY_AND_ASSIGN(LoginLibraryImpl);
};

class LoginLibraryStubImpl : public LoginLibrary {
 public:
  LoginLibraryStubImpl() {}
  virtual ~LoginLibraryStubImpl() {}

  virtual void Init() OVERRIDE {}

  virtual void EmitLoginPromptReady() OVERRIDE {}
  virtual void EmitLoginPromptVisible() OVERRIDE {}
  virtual void RequestRetrievePolicy(
      RetrievePolicyCallback callback, void* delegate) OVERRIDE {
    callback(delegate, "", 0);
  }
  virtual void RequestStorePolicy(const std::string& policy,
                                  StorePolicyCallback callback,
                                  void* delegate) OVERRIDE {
    callback(delegate, true);
  }
  virtual void StartSession(
      const std::string& user_email,
      const std::string& unique_id /* unused */) OVERRIDE {
  }
  virtual void StopSession(const std::string& unique_id /* unused */) OVERRIDE {
  }
  virtual void RestartJob(int pid, const std::string& command_line) OVERRIDE {
  }
  virtual void RestartEntd() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginLibraryStubImpl);
};

// static
LoginLibrary* LoginLibrary::GetImpl(bool stub) {
  LoginLibrary* impl;
  if (stub)
    impl = new LoginLibraryStubImpl();
  else
    impl = new LoginLibraryImpl();
  impl->Init();
  return impl;
}

}  // namespace chromeos
