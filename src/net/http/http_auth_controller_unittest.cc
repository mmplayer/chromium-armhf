// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_controller.h"

#include "base/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_auth_handler_mock.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

enum HandlerRunMode {
  RUN_HANDLER_SYNC,
  RUN_HANDLER_ASYNC
};

enum SchemeState {
  SCHEME_IS_DISABLED,
  SCHEME_IS_ENABLED
};

scoped_refptr<HttpResponseHeaders> HeadersFromString(const char* string) {
  std::string raw_string(string);
  std::string headers_string = HttpUtil::AssembleRawHeaders(
      raw_string.c_str(), raw_string.length());
  scoped_refptr<HttpResponseHeaders> headers(
      new HttpResponseHeaders(headers_string));
  return headers;
}

// Runs an HttpAuthController with a single round mock auth handler
// that returns |handler_rv| on token generation.  The handler runs in
// async if |run_mode| is RUN_HANDLER_ASYNC.  Upon completion, the
// return value of the controller is tested against
// |expected_controller_rv|.  |scheme_state| indicates whether the
// auth scheme used should be disabled after this run.
void RunSingleRoundAuthTest(HandlerRunMode run_mode,
                            int handler_rv,
                            int expected_controller_rv,
                            SchemeState scheme_state) {
  BoundNetLog dummy_log;
  HttpAuthCache dummy_auth_cache;

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://example.com");

  scoped_refptr<HttpResponseHeaders> headers(HeadersFromString(
      "HTTP/1.1 407\r\n"
      "Proxy-Authenticate: MOCK foo\r\n"
      "\r\n"));

  HttpAuthHandlerMock::Factory auth_handler_factory;
  HttpAuthHandlerMock* auth_handler = new HttpAuthHandlerMock();
  auth_handler->SetGenerateExpectation((run_mode == RUN_HANDLER_ASYNC),
                                       handler_rv);
  auth_handler_factory.AddMockHandler(auth_handler, HttpAuth::AUTH_PROXY);
  auth_handler_factory.set_do_init_from_challenge(true);

  scoped_refptr<HttpAuthController> controller(
      new HttpAuthController(HttpAuth::AUTH_PROXY,
                             GURL("http://example.com"),
                             &dummy_auth_cache, &auth_handler_factory));
  ASSERT_EQ(OK,
            controller->HandleAuthChallenge(headers, false, false, dummy_log));
  ASSERT_TRUE(controller->HaveAuthHandler());
  controller->ResetAuth(string16(), string16());
  EXPECT_TRUE(controller->HaveAuth());

  TestOldCompletionCallback callback;
  EXPECT_EQ((run_mode == RUN_HANDLER_ASYNC)? ERR_IO_PENDING:
            expected_controller_rv,
            controller->MaybeGenerateAuthToken(&request, &callback,
                                               dummy_log));
  if (run_mode == RUN_HANDLER_ASYNC)
    EXPECT_EQ(expected_controller_rv, callback.WaitForResult());
  EXPECT_EQ((scheme_state == SCHEME_IS_DISABLED),
            controller->IsAuthSchemeDisabled(HttpAuth::AUTH_SCHEME_MOCK));
}

}  // namespace

// If an HttpAuthHandler returns an error code that indicates a
// permanent error, the HttpAuthController should disable the scheme
// used and retry the request.
TEST(HttpAuthControllerTest, PermanentErrors) {

  // Run a synchronous handler that returns
  // ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS.  We expect a return value
  // of OK from the controller so we can retry the request.
  RunSingleRoundAuthTest(RUN_HANDLER_SYNC,
                         ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS,
                         OK, SCHEME_IS_DISABLED);

  // Now try an async handler that returns
  // ERR_MISSING_AUTH_CREDENTIALS.  Async and sync handlers invoke
  // different code paths in HttpAuthController when generating
  // tokens.
  RunSingleRoundAuthTest(RUN_HANDLER_ASYNC, ERR_MISSING_AUTH_CREDENTIALS, OK,
                         SCHEME_IS_DISABLED);

  // If a non-permanent error is returned by the handler, then the
  // controller should report it unchanged.
  RunSingleRoundAuthTest(RUN_HANDLER_ASYNC, ERR_INVALID_AUTH_CREDENTIALS,
                         ERR_INVALID_AUTH_CREDENTIALS, SCHEME_IS_ENABLED);
}

// If an HttpAuthHandler indicates that it doesn't allow explicit
// credentials, don't prompt for credentials.
TEST(HttpAuthControllerTest, NoExplicitCredentialsAllowed) {
  // Modified mock HttpAuthHandler for this test.
  class MockHandler : public HttpAuthHandlerMock {
   public:
    MockHandler(int expected_rv, HttpAuth::Scheme scheme)
        : expected_scheme_(scheme) {
      SetGenerateExpectation(false, expected_rv);
    }

   protected:
    virtual bool Init(HttpAuth::ChallengeTokenizer* challenge) OVERRIDE {
      HttpAuthHandlerMock::Init(challenge);
      set_allows_default_credentials(true);
      set_allows_explicit_credentials(false);
      set_connection_based(true);
      // Pretend to be SCHEME_BASIC so we can test failover logic.
      if (challenge->scheme() == "Basic") {
        auth_scheme_ = HttpAuth::AUTH_SCHEME_BASIC;
        --score_;  // Reduce score, so we rank below Mock.
        set_allows_explicit_credentials(true);
      }
      EXPECT_EQ(expected_scheme_, auth_scheme_);
      return true;
    }

    virtual int GenerateAuthTokenImpl(const string16* username,
                                      const string16* password,
                                      const HttpRequestInfo* request,
                                      OldCompletionCallback* callback,
                                      std::string* auth_token) OVERRIDE {
      int result =
          HttpAuthHandlerMock::GenerateAuthTokenImpl(username, password,
                                                     request, callback,
                                                     auth_token);
      EXPECT_TRUE(result != OK ||
                  !AllowsExplicitCredentials() || !username->empty());
      return result;
    }

   private:
    HttpAuth::Scheme expected_scheme_;
  };

  BoundNetLog dummy_log;
  HttpAuthCache dummy_auth_cache;
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://example.com");

  HttpRequestHeaders request_headers;
  scoped_refptr<HttpResponseHeaders> headers(HeadersFromString(
      "HTTP/1.1 401\r\n"
      "WWW-Authenticate: Mock\r\n"
      "WWW-Authenticate: Basic\r\n"
      "\r\n"));

  HttpAuthHandlerMock::Factory auth_handler_factory;

  // Handlers for the first attempt at authentication.  AUTH_SCHEME_MOCK handler
  // accepts the default identity and successfully constructs a token.
  auth_handler_factory.AddMockHandler(
      new MockHandler(OK, HttpAuth::AUTH_SCHEME_MOCK), HttpAuth::AUTH_SERVER);
  auth_handler_factory.AddMockHandler(
      new MockHandler(ERR_UNEXPECTED, HttpAuth::AUTH_SCHEME_BASIC),
      HttpAuth::AUTH_SERVER);

  // Handlers for the second attempt.  Neither should be used to generate a
  // token.  Instead the controller should realize that there are no viable
  // identities to use with the AUTH_SCHEME_MOCK handler and fail.
  auth_handler_factory.AddMockHandler(
      new MockHandler(ERR_UNEXPECTED, HttpAuth::AUTH_SCHEME_MOCK),
      HttpAuth::AUTH_SERVER);
  auth_handler_factory.AddMockHandler(
      new MockHandler(ERR_UNEXPECTED, HttpAuth::AUTH_SCHEME_BASIC),
      HttpAuth::AUTH_SERVER);

  // Fallback handlers for the second attempt.  The AUTH_SCHEME_MOCK handler
  // should be discarded due to the disabled scheme, and the AUTH_SCHEME_BASIC
  // handler should successfully be used to generate a token.
  auth_handler_factory.AddMockHandler(
      new MockHandler(ERR_UNEXPECTED, HttpAuth::AUTH_SCHEME_MOCK),
      HttpAuth::AUTH_SERVER);
  auth_handler_factory.AddMockHandler(
      new MockHandler(OK, HttpAuth::AUTH_SCHEME_BASIC),
      HttpAuth::AUTH_SERVER);
  auth_handler_factory.set_do_init_from_challenge(true);

  scoped_refptr<HttpAuthController> controller(
      new HttpAuthController(HttpAuth::AUTH_SERVER,
                             GURL("http://example.com"),
                             &dummy_auth_cache, &auth_handler_factory));
  ASSERT_EQ(OK,
            controller->HandleAuthChallenge(headers, false, false, dummy_log));
  ASSERT_TRUE(controller->HaveAuthHandler());
  controller->ResetAuth(string16(), string16());
  EXPECT_TRUE(controller->HaveAuth());

  // Should only succeed if we are using the AUTH_SCHEME_MOCK MockHandler.
  EXPECT_EQ(OK, controller->MaybeGenerateAuthToken(&request, NULL, dummy_log));
  controller->AddAuthorizationHeader(&request_headers);

  // Once a token is generated, simulate the receipt of a server response
  // indicating that the authentication attempt was rejected.
  ASSERT_EQ(OK,
            controller->HandleAuthChallenge(headers, false, false, dummy_log));
  ASSERT_TRUE(controller->HaveAuthHandler());
  controller->ResetAuth(ASCIIToUTF16("Hello"), string16());
  EXPECT_TRUE(controller->HaveAuth());
  EXPECT_TRUE(controller->IsAuthSchemeDisabled(HttpAuth::AUTH_SCHEME_MOCK));
  EXPECT_FALSE(controller->IsAuthSchemeDisabled(HttpAuth::AUTH_SCHEME_BASIC));

  // Should only succeed if we are using the AUTH_SCHEME_BASIC MockHandler.
  EXPECT_EQ(OK, controller->MaybeGenerateAuthToken(&request, NULL, dummy_log));
}

}  // namespace net
