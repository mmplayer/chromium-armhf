// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "net/base/capturing_net_log.h"
#include "net/base/net_errors.h"
#include "net/base/net_log_unittest.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_auth_handler_mock.h"
#include "net/http/http_request_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(HttpAuthHandlerTest, NetLog) {
  NetLog::Source source;
  GURL origin("http://www.example.com");
  std::string challenge = "Mock asdf";
  string16 username = ASCIIToUTF16("user");
  string16 password = ASCIIToUTF16("pass");
  std::string auth_token;
  HttpRequestInfo request;

  for (int i = 0; i < 2; ++i) {
    bool async = (i == 0);
    for (int j = 0; j < 2; ++j) {
      int rv = (j == 0) ? OK : ERR_UNEXPECTED;
      for (int k = 0; k < 2; ++k) {
        TestOldCompletionCallback test_callback;
        HttpAuth::Target target =
            (k == 0) ? HttpAuth::AUTH_PROXY : HttpAuth::AUTH_SERVER;
        NetLog::EventType event_type =
            (k == 0) ? NetLog::TYPE_AUTH_PROXY : NetLog::TYPE_AUTH_SERVER;
        HttpAuth::ChallengeTokenizer tokenizer(
            challenge.begin(), challenge.end());
        HttpAuthHandlerMock mock_handler;
        CapturingNetLog capturing_net_log(CapturingNetLog::kUnbounded);
        BoundNetLog bound_net_log(source, &capturing_net_log);

        mock_handler.InitFromChallenge(&tokenizer, target,
                                       origin, bound_net_log);
        mock_handler.SetGenerateExpectation(async, rv);
        mock_handler.GenerateAuthToken(&username, &password, &request,
                                       &test_callback, &auth_token);
        if (async)
          test_callback.WaitForResult();

        net::CapturingNetLog::EntryList entries;
        capturing_net_log.GetEntries(&entries);

        EXPECT_EQ(2u, entries.size());
        EXPECT_TRUE(LogContainsBeginEvent(entries, 0, event_type));
        EXPECT_TRUE(LogContainsEndEvent(entries, 1, event_type));
      }
    }
  }
}

}  // namespace net
