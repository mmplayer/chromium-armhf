// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for GaiaOAuthFetcher.
// Originally ported from GaiaAuthFetcher tests.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/net/chrome_cookie_notification_details.h"
#include "chrome/browser/net/gaia/gaia_oauth_consumer.h"
#include "chrome/browser/net/gaia/gaia_oauth_fetcher.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/net/http_return.h"
#include "chrome/test/base/testing_profile.h"
#include "content/common/net/url_fetcher.h"
#include "content/common/notification_service.h"
#include "content/test/test_url_fetcher_factory.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

class MockGaiaOAuthConsumer : public GaiaOAuthConsumer {
 public:
  MockGaiaOAuthConsumer() {}
  ~MockGaiaOAuthConsumer() {}

  MOCK_METHOD1(OnGetOAuthTokenSuccess, void(const std::string& oauth_token));
  MOCK_METHOD1(OnGetOAuthTokenFailure,
               void(const GoogleServiceAuthError& error));

  MOCK_METHOD2(OnOAuthGetAccessTokenSuccess, void(const std::string& token,
                                                  const std::string& secret));
  MOCK_METHOD1(OnOAuthGetAccessTokenFailure,
               void(const GoogleServiceAuthError& error));

  MOCK_METHOD3(OnOAuthWrapBridgeSuccess,
               void(const std::string& service_scope,
                    const std::string& token,
                    const std::string& expires_in));
  MOCK_METHOD2(OnOAuthWrapBridgeFailure,
               void(const std::string& service_scope,
                    const GoogleServiceAuthError& error));

  MOCK_METHOD1(OnUserInfoSuccess, void(const std::string& email));
  MOCK_METHOD1(OnUserInfoFailure, void(const GoogleServiceAuthError& error));

  MOCK_METHOD0(OnOAuthRevokeTokenSuccess, void());
  MOCK_METHOD1(OnOAuthRevokeTokenFailure,
               void(const GoogleServiceAuthError& error));
};

class MockGaiaOAuthFetcher : public GaiaOAuthFetcher {
 public:
  MockGaiaOAuthFetcher(GaiaOAuthConsumer* consumer,
                       net::URLRequestContextGetter* getter,
                       Profile* profile,
                       const std::string& service_scope)
      : GaiaOAuthFetcher(
          consumer, getter, profile, service_scope) {}

  ~MockGaiaOAuthFetcher() {}

  MOCK_METHOD1(StartOAuthGetAccessToken,
               void(const std::string& oauth1_request_token));

  MOCK_METHOD4(StartOAuthWrapBridge,
               void(const std::string& oauth1_access_token,
                    const std::string& oauth1_access_token_secret,
                    const std::string& wrap_token_duration,
                    const std::string& oauth2_scope));

  MOCK_METHOD1(StartUserInfo, void(const std::string& oauth2_access_token));
};

#if 0  // Suppressing for now
TEST(GaiaOAuthFetcherTest, GetOAuthToken) {
  const std::string oauth_token = "4/OAuth1-Request_Token-1234567";
  base::Time creation = base::Time::Now();
  base::Time expiration = base::Time::Time();

  scoped_ptr<net::CookieMonster::CanonicalCookie> canonical_cookie;
  canonical_cookie.reset(
      new net::CookieMonster::CanonicalCookie(
          GURL("http://www.google.com/"),  // url
          "oauth_token",                   // name
          oauth_token,                     // value
          "www.google.com",                // domain
          "/accounts/o8/GetOAuthToken",    // path
          "",                              // mac_key
          "",                              // mac_algorithm
          creation,                        // creation
          expiration,                      // expiration
          creation,                        // last_access
          true,                            // secure
          true,                            // httponly
          false));                         // has_expires

  scoped_ptr<ChromeCookieDetails::ChromeCookieDetails> cookie_details;
  cookie_details.reset(
      new ChromeCookieDetails::ChromeCookieDetails(
          canonical_cookie.get(),
          false,
          net::CookieMonster::Delegate::CHANGE_COOKIE_EXPLICIT));

  MockGaiaOAuthConsumer consumer;
  EXPECT_CALL(consumer, OnGetOAuthTokenSuccess(oauth_token)).Times(1);

  TestingProfile profile;

  MockGaiaOAuthFetcher oauth_fetcher(&consumer,
                                     profile.GetRequestContext(),
                                     &profile,
                                     std::string());
  EXPECT_CALL(oauth_fetcher, StartOAuthGetAccessToken(oauth_token)).Times(1);

  oauth_fetcher.Observe(
      chrome::NOTIFICATION_COOKIE_CHANGED,
      Source<Profile>(&profile),
      Details<ChromeCookieDetails>(cookie_details.get()));
}
#endif  // 0  // Suppressing for now

typedef testing::Test GaiaOAuthFetcherTest;

TEST_F(GaiaOAuthFetcherTest, OAuthGetAccessToken) {
  const std::string oauth_token =
      "1/OAuth1-Access_Token-1234567890abcdefghijklm";
  const std::string oauth_token_secret = "Dont_tell_the_secret-123";
  const std::string data("oauth_token="
                         "1%2FOAuth1-Access_Token-1234567890abcdefghijklm"
                         "&oauth_token_secret=Dont_tell_the_secret-123");

  MockGaiaOAuthConsumer consumer;
  EXPECT_CALL(consumer,
              OnOAuthGetAccessTokenSuccess(oauth_token,
                                           oauth_token_secret)).Times(1);

  TestingProfile profile;
  MockGaiaOAuthFetcher oauth_fetcher(&consumer,
                                     profile.GetRequestContext(),
                                     &profile,
                                     "service_scope-JnG18MEE");
  EXPECT_CALL(oauth_fetcher,
              StartOAuthWrapBridge(oauth_token,
                                   oauth_token_secret,
                                   "3600",
                                   "service_scope-JnG18MEE")).Times(1);

  net::ResponseCookies cookies;
  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, 0);
  GURL url(GaiaUrls::GetInstance()->oauth_get_access_token_url());
  oauth_fetcher.OnURLFetchComplete(NULL, url, status, RC_REQUEST_OK,
                                   cookies, data);
}

TEST_F(GaiaOAuthFetcherTest, OAuthWrapBridge) {
  const std::string wrap_token =
      "1/OAuth2-Access_Token-nopqrstuvwxyz1234567890";
  const std::string expires_in = "3600";

  const std::string data("wrap_access_token="
                         "1%2FOAuth2-Access_Token-nopqrstuvwxyz1234567890"
                         "&wrap_access_token_expires_in=3600");

  MockGaiaOAuthConsumer consumer;
  EXPECT_CALL(consumer,
              OnOAuthWrapBridgeSuccess("service_scope-0fL85iOi",
                                       wrap_token,
                                       expires_in)).Times(1);

  TestingProfile profile;
  MockGaiaOAuthFetcher oauth_fetcher(&consumer,
                                     profile.GetRequestContext(),
                                     &profile,
                                     "service_scope-0fL85iOi");
  EXPECT_CALL(oauth_fetcher, StartUserInfo(wrap_token)).Times(1);

  net::ResponseCookies cookies;
  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, 0);
  GURL url(GaiaUrls::GetInstance()->oauth_wrap_bridge_url());
  oauth_fetcher.OnURLFetchComplete(NULL, url, status, RC_REQUEST_OK,
                                   cookies, data);
}

TEST_F(GaiaOAuthFetcherTest, UserInfo) {
  const std::string email_address = "someone@somewhere.net";
  const std::string wrap_token =
      "1/OAuth2-Access_Token-nopqrstuvwxyz1234567890";
  const std::string expires_in = "3600";
  const std::string data("{\n \"email\": \"someone@somewhere.net\",\n"
                         " \"verified_email\": true\n}\n");
  MockGaiaOAuthConsumer consumer;
  EXPECT_CALL(consumer,
              OnUserInfoSuccess(email_address)).Times(1);

  TestingProfile profile;
  MockGaiaOAuthFetcher oauth_fetcher(&consumer,
                                     profile.GetRequestContext(),
                                     &profile,
                                     "service_scope-Nrj4LmgU");

  net::ResponseCookies cookies;
  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, 0);
  GURL url(GaiaUrls::GetInstance()->oauth_user_info_url());
  oauth_fetcher.OnURLFetchComplete(NULL, url, status,
                                   RC_REQUEST_OK, cookies, data);
}

TEST_F(GaiaOAuthFetcherTest, OAuthRevokeToken) {
  const std::string token = "1/OAuth2-Access_Token-nopqrstuvwxyz1234567890";
  MockGaiaOAuthConsumer consumer;
  EXPECT_CALL(consumer,
              OnOAuthRevokeTokenSuccess()).Times(1);

  TestingProfile profile;
  MockGaiaOAuthFetcher oauth_fetcher(&consumer,
                                     profile.GetRequestContext(),
                                     &profile,
                                     "service_scope-Nrj4LmgU");

  net::ResponseCookies cookies;
  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, 0);
  GURL url(GaiaUrls::GetInstance()->oauth_revoke_token_url());
  oauth_fetcher.OnURLFetchComplete(NULL, url, status,
                                   RC_REQUEST_OK, cookies, std::string());
}
