// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_AUTHENTICATOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_AUTHENTICATOR_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"

class Profile;

namespace chromeos {

// An interface for objects that will authenticate a Chromium OS user.
// When authentication successfully completes, will call
// consumer_->OnLoginSuccess() on the UI thread.
// On failure, will call consumer_->OnLoginFailure() on the UI thread.
// On password change detected, will call
// consumer_->OnPasswordChangeDetected() on the UI thread.
class Authenticator : public base::RefCountedThreadSafe<Authenticator> {
 public:
  // A domain which requires special-case parsing in canonicalization.
  static const char kSpecialCaseDomain[];

  explicit Authenticator(LoginStatusConsumer* consumer);
  virtual ~Authenticator();

  // Given externally authenticated |username| and |password|, this method
  // attempts to complete authentication process.
  virtual void CompleteLogin(Profile* profile,
                             const std::string& username,
                             const std::string& password) = 0;

  // Given a |username| and |password|, this method attempts to authenticate
  // to login.
  // Optionally |login_token| and |login_captcha| could be provided.
  // Must be called on the UI thread.
  virtual void AuthenticateToLogin(Profile* profile,
                                   const std::string& username,
                                   const std::string& password,
                                   const std::string& login_token,
                                   const std::string& login_captcha) = 0;

  // Given a |username| and |password|, this method attempts to
  // authenticate to unlock the computer.
  // Must be called on the UI thread.
  virtual void AuthenticateToUnlock(const std::string& username,
                                    const std::string& password) = 0;

  // Initiates incognito ("browse without signing in") login.
  virtual void LoginOffTheRecord() = 0;

  // |credentials| are the tokens that we get back from the ClientLogin API.
  // |request_pending| is true if we still plan to call consumer_ with the
  // results of more requests.
  // Must be called on the UI thread.
  virtual void OnLoginSuccess(
      const GaiaAuthConsumer::ClientLoginResult& credentials,
      bool request_pending) = 0;

  // Must be called on the UI thread.
  virtual void OnLoginFailure(const LoginFailure& error) = 0;

  // Call these methods on the UI thread.
  // If a password logs the user in online, but cannot be used to
  // mount his cryptohome, we expect that a password change has
  // occurred.
  // Call this method to migrate the user's encrypted data
  // forward to use his new password.  |old_password| is the password
  // his data was last encrypted with, |result| is the blob of auth
  // data passed back through OnPasswordChangeDetected().
  virtual void RecoverEncryptedData(
      const std::string& old_password,
      const GaiaAuthConsumer::ClientLoginResult& credentials) = 0;

  // Call this method to erase the user's encrypted data
  // and create a new cryptohome.  |result| is the blob of auth
  // data passed back through OnPasswordChangeDetected().
  virtual void ResyncEncryptedData(
      const GaiaAuthConsumer::ClientLoginResult& credentials) = 0;

  // Attempt to authenticate online again.
  virtual void RetryAuth(Profile* profile,
                         const std::string& username,
                         const std::string& password,
                         const std::string& login_token,
                         const std::string& login_captcha) = 0;

  // Kicks of verification of OAuth1 access token.
  virtual void VerifyOAuth1AccessToken(const std::string& oauth1_access_token,
                                       const std::string& oauth1_secret) = 0;

  // OAuth token encryption helpers.
  virtual std::string EncryptToken(const std::string& token) = 0;
  virtual std::string DecryptToken(const std::string& encrypted_token) = 0;
  // TODO(zelidrag): Remove legacy encryption support in R16.
  virtual std::string DecryptLegacyToken(
      const std::string& encrypted_token) = 0;

  // Profile (usually off the record ) that was used to perform the last
  // authentication process.
  Profile* authentication_profile() { return authentication_profile_; }

  // Perform basic canonicalization of |email_address|, taking into account
  // that gmail does not consider '.' or caps inside a username to matter.
  // It also ignores everything after a '+'.
  // For example, c.masone+abc@gmail.com == cMaSone@gmail.com, per
  // http://mail.google.com/support/bin/answer.py?hl=en&ctx=mail&answer=10313#
  static std::string Canonicalize(const std::string& email_address);

 protected:
  LoginStatusConsumer* consumer_;
  Profile* authentication_profile_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Authenticator);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_AUTHENTICATOR_H_
