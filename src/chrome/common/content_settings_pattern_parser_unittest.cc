// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/content_settings_pattern_parser.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
typedef ContentSettingsPattern::BuilderInterface BuilderInterface;
}  // namespace

class MockBuilder : public ContentSettingsPattern::BuilderInterface {
 public:
  MOCK_METHOD0(WithSchemeWildcard, BuilderInterface*());
  MOCK_METHOD0(WithDomainWildcard, BuilderInterface*());
  MOCK_METHOD0(WithPortWildcard, BuilderInterface*());
  MOCK_METHOD1(WithScheme, BuilderInterface*(const std::string& scheme));
  MOCK_METHOD1(WithHost, BuilderInterface*(const std::string& host));
  MOCK_METHOD1(WithPort, BuilderInterface*(const std::string& port));
  MOCK_METHOD1(WithPath, BuilderInterface*(const std::string& path));
  MOCK_METHOD0(Invalid, BuilderInterface*());
  MOCK_METHOD0(Build, ContentSettingsPattern());
};

TEST(ContentSettingsPatternParserTest, ParsePatterns) {
  // Test valid patterns
  MockBuilder builder;

  EXPECT_CALL(builder, WithScheme("http")).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("www.youtube.com")).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithPort("8080")).Times(1).WillOnce(
      ::testing::Return(&builder));
  content_settings::PatternParser::Parse(
      "http://www.youtube.com:8080", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithSchemeWildcard()).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("www.gmail.com")).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithPort("80")).Times(1).WillOnce(
      ::testing::Return(&builder));
  content_settings::PatternParser::Parse("*://www.gmail.com:80", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithScheme("http")).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("www.gmail.com")).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithPortWildcard()).Times(1).WillOnce(
      ::testing::Return(&builder));
  content_settings::PatternParser::Parse("http://www.gmail.com:*", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithScheme("http")).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithDomainWildcard()).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("google.com")).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithPort("80")).Times(1).WillOnce(
      ::testing::Return(&builder));
  content_settings::PatternParser::Parse("http://[*.]google.com:80", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithScheme("https")).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("[::1]")).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithPort("8080")).Times(1).WillOnce(
      ::testing::Return(&builder));
  content_settings::PatternParser::Parse("https://[::1]:8080", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithScheme("http")).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("127.0.0.1")).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithPort("8080")).Times(1).WillOnce(
      ::testing::Return(&builder));
  content_settings::PatternParser::Parse("http://127.0.0.1:8080", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithScheme("file")).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithPath("/foo/bar/test.html")).Times(1).WillOnce(
      ::testing::Return(&builder));
  content_settings::PatternParser::Parse(
      "file:///foo/bar/test.html", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  // Test valid pattern short forms
  EXPECT_CALL(builder, WithSchemeWildcard()).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("www.youtube.com")).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithPort("8080")).Times(1).WillOnce(
      ::testing::Return(&builder));
  content_settings::PatternParser::Parse("www.youtube.com:8080", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithSchemeWildcard()).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("www.youtube.com")).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithPortWildcard()).Times(1).WillOnce(
      ::testing::Return(&builder));
  content_settings::PatternParser::Parse("www.youtube.com", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, WithSchemeWildcard()).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithDomainWildcard()).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithHost("youtube.com")).Times(1).WillOnce(
      ::testing::Return(&builder));
  EXPECT_CALL(builder, WithPortWildcard()).Times(1).WillOnce(
      ::testing::Return(&builder));
  content_settings::PatternParser::Parse("[*.]youtube.com", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  // Test invalid patterns
  EXPECT_CALL(builder, Invalid()).Times(1).WillOnce(
      ::testing::Return(&builder));
  content_settings::PatternParser::Parse("*youtube.com", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, Invalid()).Times(1).WillOnce(
      ::testing::Return(&builder));
  content_settings::PatternParser::Parse("*.youtube.com", &builder);
  ::testing::Mock::VerifyAndClear(&builder);

  EXPECT_CALL(builder, Invalid()).Times(1).WillOnce(
      ::testing::Return(&builder));
  content_settings::PatternParser::Parse("www.youtube.com*", &builder);
  ::testing::Mock::VerifyAndClear(&builder);
}

TEST(ContentSettingsPatternParserTest, SerializePatterns) {
  ContentSettingsPattern::PatternParts parts;
  parts.scheme = "http";
  parts.host = "www.youtube.com";
  parts.port = "8080";
  EXPECT_STREQ("http://www.youtube.com:8080",
               content_settings::PatternParser::ToString(parts).c_str());

  parts = ContentSettingsPattern::PatternParts();
  parts.scheme = "file";
  parts.path = "/foo/bar/test.html";
  EXPECT_STREQ("file:///foo/bar/test.html",
               content_settings::PatternParser::ToString(parts).c_str());
}
