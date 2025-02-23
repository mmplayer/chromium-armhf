// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/content_settings_pattern.h"

#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

ContentSettingsPattern Pattern(const std::string& str) {
  return ContentSettingsPattern::FromString(str);
}

}  // namespace

TEST(ContentSettingsPatternTest, RealWorldPatterns) {
  // This is the place for real world patterns that unveiled bugs.
  EXPECT_STREQ("[*.]ikea.com",
               Pattern("[*.]ikea.com").ToString().c_str());
}

TEST(ContentSettingsPatternTest, GURL) {
  // Document and verify GURL behavior.
  GURL url("http://mail.google.com:80");
  EXPECT_EQ(-1, url.IntPort());
  EXPECT_EQ("", url.port());

  url = GURL("http://mail.google.com");
  EXPECT_EQ(-1, url.IntPort());
  EXPECT_EQ("", url.port());

  url = GURL("https://mail.google.com:443");
  EXPECT_EQ(-1, url.IntPort());
  EXPECT_EQ("", url.port());

  url = GURL("https://mail.google.com");
  EXPECT_EQ(-1, url.IntPort());
  EXPECT_EQ("", url.port());

  url = GURL("http://mail.google.com");
  EXPECT_EQ(-1, url.IntPort());
  EXPECT_EQ("", url.port());
}

TEST(ContentSettingsPatternTest, FromURL) {
  // NOTICE: When content settings pattern are created from a GURL the following
  // happens:
  // - If the GURL scheme is "http" the scheme wildcard is used. Otherwise the
  //   GURL scheme is used.
  // - A domain wildcard is added to the GURL host.
  // - A port wildcard is used instead of the schemes default port.
  //   In case of non-default ports the specific GURL port is used.
  ContentSettingsPattern pattern = ContentSettingsPattern::FromURL(
      GURL("http://www.youtube.com"));
  EXPECT_TRUE(pattern.IsValid());
  EXPECT_STREQ("[*.]www.youtube.com", pattern.ToString().c_str());

  // Patterns created from a URL.
  pattern = ContentSettingsPattern::FromURL(GURL("http://www.google.com"));
  EXPECT_TRUE(pattern.Matches(GURL("http://www.google.com")));
  EXPECT_TRUE(pattern.Matches(GURL("http://foo.www.google.com")));
  EXPECT_TRUE(pattern.Matches(GURL("http://www.google.com:80")));
  EXPECT_TRUE(pattern.Matches(GURL("http://www.google.com:81")));
  EXPECT_FALSE(pattern.Matches(GURL("https://mail.google.com")));
  EXPECT_TRUE(pattern.Matches(GURL("https://www.google.com")));

  pattern = ContentSettingsPattern::FromURL(GURL("http://www.google.com:80"));
  EXPECT_TRUE(pattern.Matches(GURL("http://www.google.com")));
  EXPECT_TRUE(pattern.Matches(GURL("http://www.google.com:80")));
  EXPECT_TRUE(pattern.Matches(GURL("http://www.google.com:81")));

  pattern = ContentSettingsPattern::FromURL(GURL("https://www.google.com:443"));
  EXPECT_TRUE(pattern.Matches(GURL("https://www.google.com")));
  EXPECT_TRUE(pattern.Matches(GURL("https://www.google.com:443")));
  EXPECT_FALSE(pattern.Matches(GURL("https://www.google.com:444")));
  EXPECT_FALSE(pattern.Matches(GURL("http://www.google.com:443")));

  pattern = ContentSettingsPattern::FromURL(GURL("https://127.0.0.1"));
  EXPECT_TRUE(pattern.IsValid());
  EXPECT_STREQ("https://127.0.0.1:443", pattern.ToString().c_str());

  pattern = ContentSettingsPattern::FromURL(GURL("http://[::1]"));
  EXPECT_TRUE(pattern.IsValid());

  pattern = ContentSettingsPattern::FromURL(GURL("file:///foo/bar.html"));
  EXPECT_TRUE(pattern.IsValid());
  EXPECT_STREQ("file:///foo/bar.html", pattern.ToString().c_str());
}

TEST(ContentSettingsPatternTest, FromURLNoWildcard) {
  // If no port is specifed GURLs always use the default port for the schemes
  // HTTP and HTTPS. Hence a GURL always carries a port specification either
  // explicitly or implicitly. Therefore if a content settings pattern is
  // created from a GURL with no wildcard, specific values are used for the
  // scheme, host and port part of the pattern.
  // Creating content settings patterns from strings behaves different. Pattern
  // parts that are omitted in pattern specifications (strings), are completed
  // with a wildcard.
  ContentSettingsPattern pattern = ContentSettingsPattern::FromURLNoWildcard(
      GURL("http://www.example.com"));
  EXPECT_TRUE(pattern.IsValid());
  EXPECT_STREQ("http://www.example.com:80", pattern.ToString().c_str());
  EXPECT_TRUE(pattern.Matches(GURL("http://www.example.com")));
  EXPECT_FALSE(pattern.Matches(GURL("https://www.example.com")));
  EXPECT_FALSE(pattern.Matches(GURL("http://foo.www.example.com")));

  pattern = ContentSettingsPattern::FromURLNoWildcard(
      GURL("https://www.example.com"));
  EXPECT_TRUE(pattern.IsValid());
  EXPECT_STREQ("https://www.example.com:443", pattern.ToString().c_str());
  EXPECT_FALSE(pattern.Matches(GURL("http://www.example.com")));
  EXPECT_TRUE(pattern.Matches(GURL("https://www.example.com")));
  EXPECT_FALSE(pattern.Matches(GURL("http://foo.www.example.com")));

  pattern = ContentSettingsPattern::FromURLNoWildcard(
      GURL("https://www.example.com"));
}

TEST(ContentSettingsPatternTest, Wildcard) {
  EXPECT_TRUE(ContentSettingsPattern::Wildcard().IsValid());

  EXPECT_TRUE(ContentSettingsPattern::Wildcard().Matches(
      GURL("http://www.google.com")));
  EXPECT_TRUE(ContentSettingsPattern::Wildcard().Matches(
      GURL("https://www.google.com")));
  EXPECT_TRUE(ContentSettingsPattern::Wildcard().Matches(
      GURL("https://myhost:8080")));
  EXPECT_TRUE(ContentSettingsPattern::Wildcard().Matches(
      GURL("file:///foo/bar.txt")));

  EXPECT_STREQ("*", ContentSettingsPattern::Wildcard().ToString().c_str());

  EXPECT_EQ(ContentSettingsPattern::IDENTITY,
            ContentSettingsPattern::Wildcard().Compare(
                ContentSettingsPattern::Wildcard()));
}

TEST(ContentSettingsPatternTest, TrimEndingDotFromHost) {
  EXPECT_TRUE(Pattern("www.example.com").IsValid());
  EXPECT_TRUE(Pattern("www.example.com").Matches(
      GURL("http://www.example.com")));
  EXPECT_TRUE(Pattern("www.example.com").Matches(
      GURL("http://www.example.com.")));

  EXPECT_TRUE(Pattern("www.example.com.").IsValid());
  EXPECT_STREQ("www.example.com",
               Pattern("www.example.com.").ToString().c_str());

  EXPECT_TRUE(Pattern("www.example.com.") == Pattern("www.example.com"));
}

TEST(ContentSettingsPatternTest, FromString_WithNoWildcards) {
  // HTTP patterns with default port.
  EXPECT_TRUE(Pattern("http://www.example.com:80").IsValid());
  EXPECT_STREQ("http://www.example.com:80",
               Pattern("http://www.example.com:80").ToString().c_str());
  // HTTP patterns with none default port.
  EXPECT_TRUE(Pattern("http://www.example.com:81").IsValid());
  EXPECT_STREQ("http://www.example.com:81",
               Pattern("http://www.example.com:81").ToString().c_str());

  // HTTPS patterns with default port.
  EXPECT_TRUE(Pattern("https://www.example.com:443").IsValid());
  EXPECT_STREQ("https://www.example.com:443",
               Pattern("https://www.example.com:443").ToString().c_str());
  // HTTPS patterns with none default port.
  EXPECT_TRUE(Pattern("https://www.example.com:8080").IsValid());
  EXPECT_STREQ("https://www.example.com:8080",
               Pattern("https://www.example.com:8080").ToString().c_str());
}

TEST(ContentSettingsPatternTest, FromString_FilePatterns) {
  EXPECT_TRUE(Pattern("file:///").IsValid());
  EXPECT_STREQ("file:///",
               Pattern("file:///").ToString().c_str());
  EXPECT_TRUE(Pattern("file:///").Matches(
      GURL("file:///")));
  EXPECT_FALSE(Pattern("file:///").Matches(
      GURL("file:///tmp/test.html")));

  EXPECT_TRUE(Pattern("file:///tmp/test.html").IsValid());
  EXPECT_STREQ("file:///tmp/file.html",
               Pattern("file:///tmp/file.html").ToString().c_str());
  EXPECT_TRUE(Pattern("file:///tmp/test.html").Matches(
      GURL("file:///tmp/test.html")));
  EXPECT_FALSE(Pattern("file:///tmp/test.html").Matches(
      GURL("file:///tmp/other.html")));
  EXPECT_FALSE(Pattern("file:///tmp/test.html").Matches(
      GURL("http://example.org/")));
}

TEST(ContentSettingsPatternTest, FromString_ExtensionPatterns) {
  EXPECT_TRUE(Pattern("chrome-extension://peoadpeiejnhkmpaakpnompolbglelel/")
      .IsValid());
  EXPECT_STREQ("chrome-extension://peoadpeiejnhkmpaakpnompolbglelel/",
      Pattern("chrome-extension://peoadpeiejnhkmpaakpnompolbglelel/")
          .ToString().c_str());
  EXPECT_TRUE(Pattern("chrome-extension://peoadpeiejnhkmpaakpnompolbglelel/")
      .Matches(GURL("chrome-extension://peoadpeiejnhkmpaakpnompolbglelel/")));
}

TEST(ContentSettingsPatternTest, FromString_WithIPAdresses) {
  // IPv4
  EXPECT_TRUE(Pattern("192.168.0.1").IsValid());
  EXPECT_STREQ("192.168.1.1", Pattern("192.168.1.1").ToString().c_str());
  EXPECT_TRUE(Pattern("https://192.168.0.1:8080").IsValid());
  EXPECT_STREQ("https://192.168.0.1:8080",
               Pattern("https://192.168.0.1:8080").ToString().c_str());
  // IPv6
  EXPECT_TRUE(Pattern("[::1]").IsValid());
  EXPECT_STREQ("[::1]", Pattern("[::1]").ToString().c_str());
  EXPECT_TRUE(Pattern("https://[::1]:8080").IsValid());
  EXPECT_STREQ("https://[::1]:8080",
               Pattern("https://[::1]:8080").ToString().c_str());
}

TEST(ContentSettingsPatternTest, FromString_WithWildcards) {
  // Creating content settings patterns from strings completes pattern parts
  // that are omitted in pattern specifications (strings) with a wildcard.

  // The wildcard pattern.
  EXPECT_TRUE(Pattern("*").IsValid());
  EXPECT_STREQ("*", Pattern("*").ToString().c_str());
  EXPECT_EQ(ContentSettingsPattern::IDENTITY,
            Pattern("*").Compare(ContentSettingsPattern::Wildcard()));

  // Patterns with port wildcard.
  EXPECT_TRUE(Pattern("http://example.com:*").IsValid());
  EXPECT_STREQ("http://example.com",
               Pattern("http://example.com:*").ToString().c_str());

  EXPECT_TRUE(Pattern("https://example.com").IsValid());
  EXPECT_STREQ("https://example.com",
               Pattern("https://example.com").ToString().c_str());

  EXPECT_TRUE(Pattern("*://www.google.com.com:8080").IsValid());
  EXPECT_STREQ("www.google.com:8080",
               Pattern("*://www.google.com:8080").ToString().c_str());
  EXPECT_TRUE(Pattern("*://www.google.com:8080").Matches(
      GURL("http://www.google.com:8080")));
  EXPECT_TRUE(Pattern("*://www.google.com:8080").Matches(
      GURL("https://www.google.com:8080")));
  EXPECT_FALSE(
      Pattern("*://www.google.com").Matches(GURL("file:///foo/bar.html")));

  EXPECT_TRUE(Pattern("www.example.com:8080").IsValid());

  // Patterns with port and scheme wildcard.
  EXPECT_TRUE(Pattern("*://www.example.com:*").IsValid());
  EXPECT_STREQ("www.example.com",
               Pattern("*://www.example.com:*").ToString().c_str());

  EXPECT_TRUE(Pattern("*://www.example.com").IsValid());
  EXPECT_STREQ("www.example.com",
               Pattern("*://www.example.com").ToString().c_str());

  EXPECT_TRUE(Pattern("www.example.com:*").IsValid());
  EXPECT_STREQ("www.example.com",
               Pattern("www.example.com:*").ToString().c_str());

  EXPECT_TRUE(Pattern("www.example.com").IsValid());
  EXPECT_STREQ("www.example.com",
               Pattern("www.example.com").ToString().c_str());
  EXPECT_TRUE(Pattern("www.example.com").Matches(
      GURL("http://www.example.com/")));
  EXPECT_FALSE(Pattern("example.com").Matches(
      GURL("http://example.org/")));

  // Patterns with domain wildcard.
  EXPECT_TRUE(Pattern("[*.]example.com").IsValid());
  EXPECT_STREQ("[*.]example.com",
               Pattern("[*.]example.com").ToString().c_str());
  EXPECT_TRUE(Pattern("[*.]example.com").Matches(
      GURL("http://example.com/")));
  EXPECT_TRUE(Pattern("[*.]example.com").Matches(
      GURL("http://foo.example.com/")));
  EXPECT_FALSE(Pattern("[*.]example.com").Matches(
      GURL("http://example.org/")));

  EXPECT_TRUE(Pattern("[*.]google.com:80").Matches(
      GURL("http://mail.google.com:80")));
  EXPECT_FALSE(Pattern("[*.]google.com:80").Matches(
      GURL("http://mail.google.com:81")));
  EXPECT_TRUE(Pattern("[*.]google.com:80").Matches(
      GURL("http://www.google.com")));

  EXPECT_TRUE(Pattern("[*.]google.com:8080").Matches(
      GURL("http://mail.google.com:8080")));

  EXPECT_TRUE(Pattern("[*.]google.com:443").Matches(
      GURL("https://mail.google.com:443")));
  EXPECT_TRUE(Pattern("[*.]google.com:443").Matches(
      GURL("https://www.google.com")));

  EXPECT_TRUE(Pattern("[*.]google.com:4321").Matches(
      GURL("https://mail.google.com:4321")));
  EXPECT_TRUE(Pattern("[*.]example.com").Matches(
      GURL("http://example.com/")));
  EXPECT_TRUE(Pattern("[*.]example.com").Matches(
      GURL("http://www.example.com/")));

  // Patterns with host wildcard
  // TODO(markusheintz): Should these patterns be allowed?
  // EXPECT_TRUE(Pattern("http://*").IsValid());
  // EXPECT_TRUE(Pattern("http://*:8080").IsValid());
  EXPECT_TRUE(Pattern("*://*").IsValid());
  EXPECT_STREQ("*", Pattern("*://*").ToString().c_str());
}

TEST(ContentSettingsPatternTest, FromString_Canonicalized) {
  // UTF-8 patterns.
  EXPECT_TRUE(Pattern("[*.]\xC4\x87ira.com").IsValid());
  EXPECT_STREQ("[*.]xn--ira-ppa.com",
               Pattern("[*.]\xC4\x87ira.com").ToString().c_str());
  EXPECT_TRUE(Pattern("\xC4\x87ira.com").IsValid());
  EXPECT_STREQ("xn--ira-ppa.com",
               Pattern("\xC4\x87ira.com").ToString().c_str());
  EXPECT_TRUE(Pattern("file:///\xC4\x87ira.html").IsValid());
  EXPECT_STREQ("file:///%C4%87ira.html",
               Pattern("file:///\xC4\x87ira.html").ToString().c_str());

  // File path normalization.
  EXPECT_TRUE(Pattern("file:///tmp/bar/../test.html").IsValid());
  EXPECT_STREQ("file:///tmp/test.html",
               Pattern("file:///tmp/bar/../test.html").ToString().c_str());
}

TEST(ContentSettingsPatternTest, InvalidPatterns) {
  // StubObserver expects an empty pattern top be returned as empty string.
  EXPECT_FALSE(ContentSettingsPattern().IsValid());
  EXPECT_STREQ("", ContentSettingsPattern().ToString().c_str());

  // Empty pattern string
  EXPECT_FALSE(Pattern("").IsValid());
  EXPECT_STREQ("", Pattern("").ToString().c_str());

  // Pattern strings with invalid scheme part.
  EXPECT_FALSE(Pattern("ftp://myhost.org").IsValid());
  EXPECT_STREQ("", Pattern("ftp://myhost.org").ToString().c_str());

  // Pattern strings with invalid host part.
  EXPECT_FALSE(Pattern("*example.com").IsValid());
  EXPECT_STREQ("", Pattern("*example.com").ToString().c_str());
  EXPECT_FALSE(Pattern("example.*").IsValid());
  EXPECT_STREQ("", Pattern("example.*").ToString().c_str());
  EXPECT_FALSE(Pattern("*\xC4\x87ira.com").IsValid());
  EXPECT_STREQ("", Pattern("*\xC4\x87ira.com").ToString().c_str());
  EXPECT_FALSE(Pattern("\xC4\x87ira.*").IsValid());
  EXPECT_STREQ("", Pattern("\xC4\x87ira.*").ToString().c_str());

  // Pattern strings with invalid port parts.
  EXPECT_FALSE(Pattern("example.com:abc").IsValid());
  EXPECT_STREQ("", Pattern("example.com:abc").ToString().c_str());

  // Invalid file pattern strings.
  EXPECT_FALSE(Pattern("file://").IsValid());
  EXPECT_STREQ("", Pattern("file://").ToString().c_str());
  EXPECT_FALSE(Pattern("file:///foo/bar.html:8080").IsValid());
  EXPECT_STREQ("", Pattern("file:///foo/bar.html:8080").ToString().c_str());
}

TEST(ContentSettingsPatternTest, UnequalOperator) {
  EXPECT_TRUE(Pattern("http://www.foo.com") != Pattern("http://www.foo.com*"));
  EXPECT_TRUE(Pattern("http://www.foo.com*") !=
              ContentSettingsPattern::Wildcard());

  EXPECT_TRUE(Pattern("http://www.foo.com") !=
              ContentSettingsPattern::Wildcard());

  EXPECT_TRUE(Pattern("http://www.foo.com") != Pattern("www.foo.com"));
  EXPECT_TRUE(Pattern("http://www.foo.com") !=
              Pattern("http://www.foo.com:80"));

  EXPECT_FALSE(Pattern("http://www.foo.com") != Pattern("http://www.foo.com"));
  EXPECT_TRUE(Pattern("http://www.foo.com") == Pattern("http://www.foo.com"));
}

TEST(ContentSettingsPatternTest, Compare) {
  // Test identical patterns patterns.
  ContentSettingsPattern pattern1 =
      Pattern("http://www.google.com");
  EXPECT_EQ(ContentSettingsPattern::IDENTITY, pattern1.Compare(pattern1));
  EXPECT_EQ(ContentSettingsPattern::IDENTITY,
            Pattern("http://www.google.com:80").Compare(
                Pattern("http://www.google.com:80")));
  EXPECT_EQ(ContentSettingsPattern::IDENTITY,
            Pattern("*://[*.]google.com:*").Compare(
                Pattern("*://[*.]google.com:*")));

  ContentSettingsPattern invalid_pattern1;
  ContentSettingsPattern invalid_pattern2 =
      ContentSettingsPattern::FromString("google.com*");

  // Compare invalid patterns.
  EXPECT_TRUE(!invalid_pattern1.IsValid());
  EXPECT_TRUE(!invalid_pattern2.IsValid());
  EXPECT_EQ(ContentSettingsPattern::IDENTITY,
            invalid_pattern1.Compare(invalid_pattern2));
  EXPECT_TRUE(invalid_pattern1 == invalid_pattern2);

  // Compare a pattern with an IPv4 addresse to a pattern with a domain name.
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_POST,
            Pattern("http://www.google.com").Compare(
                Pattern("127.0.0.1")));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("127.0.0.1").Compare(
                Pattern("http://www.google.com")));
  EXPECT_TRUE(Pattern("127.0.0.1") > Pattern("http://www.google.com"));
  EXPECT_TRUE(Pattern("http://www.google.com") < Pattern("127.0.0.1"));

  // Compare a pattern with an IPv6 address to a patterns with a domain name.
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_POST,
            Pattern("http://www.google.com").Compare(
                Pattern("[::1]")));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("[::1]").Compare(
                Pattern("http://www.google.com")));
  EXPECT_TRUE(Pattern("[::1]") > Pattern("http://www.google.com"));
  EXPECT_TRUE(Pattern("http://www.google.com") < Pattern("[::1]"));

  // Compare a pattern with an IPv6 addresse to a pattern with an IPv4 addresse.
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("127.0.0.1").Compare(
                Pattern("[::1]")));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_POST,
            Pattern("[::1]").Compare(
                Pattern("127.0.0.1")));
  EXPECT_TRUE(Pattern("[::1]") < Pattern("127.0.0.1"));
  EXPECT_TRUE(Pattern("127.0.0.1") > Pattern("[::1]"));

  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("http://www.google.com").Compare(
                Pattern("http://www.youtube.com")));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("http://[*.]google.com").Compare(
                Pattern("http://[*.]youtube.com")));

  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_POST,
            Pattern("http://[*.]host.com").Compare(
                Pattern("http://[*.]evilhost.com")));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_POST,
            Pattern("*://www.google.com:80").Compare(
                Pattern("*://www.google.com:8080")));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("https://www.google.com:80").Compare(
                Pattern("http://www.google.com:80")));

  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("http://[*.]google.com:90").Compare(
                Pattern("http://mail.google.com:80")));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("https://[*.]google.com:80").Compare(
                Pattern("http://mail.google.com:80")));
  EXPECT_EQ(ContentSettingsPattern::DISJOINT_ORDER_PRE,
            Pattern("https://mail.google.com:*").Compare(
                Pattern("http://mail.google.com:80")));

  // Test patterns with different precedences.
  EXPECT_EQ(ContentSettingsPattern::PREDECESSOR,
            Pattern("mail.google.com").Compare(
                Pattern("[*.]google.com")));
  EXPECT_EQ(ContentSettingsPattern::SUCCESSOR,
            Pattern("[*.]google.com").Compare(
                Pattern("mail.google.com")));

  EXPECT_EQ(ContentSettingsPattern::PREDECESSOR,
            Pattern("[*.]mail.google.com").Compare(
                Pattern("[*.]google.com")));
  EXPECT_EQ(ContentSettingsPattern::SUCCESSOR,
            Pattern("[*.]google.com").Compare(
                Pattern("[*.]mail.google.com")));

  EXPECT_EQ(ContentSettingsPattern::PREDECESSOR,
            Pattern("mail.google.com:80").Compare(
                Pattern("mail.google.com:*")));
  EXPECT_EQ(ContentSettingsPattern::SUCCESSOR,
            Pattern("mail.google.com:*").Compare(
                Pattern("mail.google.com:80")));

  EXPECT_EQ(ContentSettingsPattern::PREDECESSOR,
            Pattern("https://mail.google.com:*").Compare(
                Pattern("*://mail.google.com:*")));
  EXPECT_EQ(ContentSettingsPattern::SUCCESSOR,
            Pattern("*://mail.google.com:*").Compare(
                Pattern("https://mail.google.com:*")));

  EXPECT_EQ(ContentSettingsPattern::PREDECESSOR,
            Pattern("*://mail.google.com:80").Compare(
                Pattern("https://mail.google.com:*")));
  EXPECT_EQ(ContentSettingsPattern::SUCCESSOR,
            Pattern("https://mail.google.com:*").Compare(
                Pattern("*://mail.google.com:80")));

  // Test the wildcard pattern.
  EXPECT_EQ(ContentSettingsPattern::IDENTITY,
            ContentSettingsPattern::Wildcard().Compare(
                ContentSettingsPattern::Wildcard()));

  EXPECT_EQ(ContentSettingsPattern::PREDECESSOR,
            Pattern("[*.]google.com").Compare(
                ContentSettingsPattern::Wildcard()));
  EXPECT_EQ(ContentSettingsPattern::SUCCESSOR,
            ContentSettingsPattern::Wildcard().Compare(
                 Pattern("[*.]google.com")));

  EXPECT_EQ(ContentSettingsPattern::PREDECESSOR,
            Pattern("mail.google.com").Compare(
                ContentSettingsPattern::Wildcard()));
  EXPECT_EQ(ContentSettingsPattern::SUCCESSOR,
            ContentSettingsPattern::Wildcard().Compare(
                 Pattern("mail.google.com")));
}

// Legacy tests to ensure backwards compatibility.

TEST(ContentSettingsPatternTest, PatternSupport_Legacy) {
  EXPECT_TRUE(Pattern("[*.]example.com").IsValid());
  EXPECT_TRUE(Pattern("example.com").IsValid());
  EXPECT_TRUE(Pattern("192.168.0.1").IsValid());
  EXPECT_TRUE(Pattern("[::1]").IsValid());
  EXPECT_TRUE(
      Pattern("file:///tmp/test.html").IsValid());
  EXPECT_FALSE(Pattern("*example.com").IsValid());
  EXPECT_FALSE(Pattern("example.*").IsValid());

  EXPECT_TRUE(
      Pattern("http://example.com").IsValid());
  EXPECT_TRUE(
      Pattern("https://example.com").IsValid());

  EXPECT_TRUE(Pattern("[*.]example.com").Matches(
              GURL("http://example.com/")));
  EXPECT_TRUE(Pattern("[*.]example.com").Matches(
              GURL("http://www.example.com/")));
  EXPECT_TRUE(Pattern("www.example.com").Matches(
              GURL("http://www.example.com/")));
  EXPECT_TRUE(
      Pattern("file:///tmp/test.html").Matches(
              GURL("file:///tmp/test.html")));
  EXPECT_FALSE(Pattern("").Matches(
               GURL("http://www.example.com/")));
  EXPECT_FALSE(Pattern("[*.]example.com").Matches(
               GURL("http://example.org/")));
  EXPECT_FALSE(Pattern("example.com").Matches(
               GURL("http://example.org/")));
  EXPECT_FALSE(
      Pattern("file:///tmp/test.html").Matches(
               GURL("file:///tmp/other.html")));
  EXPECT_FALSE(
      Pattern("file:///tmp/test.html").Matches(
               GURL("http://example.org/")));
}

TEST(ContentSettingsPatternTest, CanonicalizePattern_Legacy) {
  // Basic patterns.
  EXPECT_STREQ("[*.]ikea.com", Pattern("[*.]ikea.com").ToString().c_str());
  EXPECT_STREQ("example.com", Pattern("example.com").ToString().c_str());
  EXPECT_STREQ("192.168.1.1", Pattern("192.168.1.1").ToString().c_str());
  EXPECT_STREQ("[::1]", Pattern("[::1]").ToString().c_str());
  EXPECT_STREQ("file:///tmp/file.html",
               Pattern("file:///tmp/file.html").ToString().c_str());

  // UTF-8 patterns.
  EXPECT_STREQ("[*.]xn--ira-ppa.com",
               Pattern("[*.]\xC4\x87ira.com").ToString().c_str());
  EXPECT_STREQ("xn--ira-ppa.com",
               Pattern("\xC4\x87ira.com").ToString().c_str());
  EXPECT_STREQ("file:///%C4%87ira.html",
               Pattern("file:///\xC4\x87ira.html").ToString().c_str());

  // file:/// normalization.
  EXPECT_STREQ("file:///tmp/test.html",
               Pattern("file:///tmp/bar/../test.html").ToString().c_str());

  // Invalid patterns.
  EXPECT_STREQ("", Pattern("*example.com").ToString().c_str());
  EXPECT_STREQ("", Pattern("example.*").ToString().c_str());
  EXPECT_STREQ("", Pattern("*\xC4\x87ira.com").ToString().c_str());
  EXPECT_STREQ("", Pattern("\xC4\x87ira.*").ToString().c_str());
}
