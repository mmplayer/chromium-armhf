// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/transport_security_state.h"

#include "base/base64.h"
#include "base/sha1.h"
#include "base/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_OPENSSL)
#include "crypto/openssl_util.h"
#else
#include "crypto/nss_util.h"
#endif

namespace net {

class TransportSecurityStateTest : public testing::Test {
  virtual void SetUp() {
#if defined(USE_OPENSSL)
    crypto::EnsureOpenSSLInit();
#else
    crypto::EnsureNSSInit();
#endif
  }
};

TEST_F(TransportSecurityStateTest, BogusHeaders) {
  int max_age = 42;
  bool include_subdomains = false;

  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "    ", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "abc", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "  abc", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "  abc   ", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "  max-age", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "  max-age  ", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "   max-age=", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "   max-age  =", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "   max-age=   ", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "   max-age  =     ", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "   max-age  =     xy", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "   max-age  =     3488a923", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488a923  ", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-ag=3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-aged=3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age==3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "amax-age=3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=-3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923;", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923     e", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923     includesubdomain", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923includesubdomains", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923=includesubdomains", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923 includesubdomainx", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923 includesubdomain=", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923 includesubdomain=true", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923 includesubdomainsx", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=3488923 includesubdomains x", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=34889.23 includesubdomains", &max_age, &include_subdomains));
  EXPECT_FALSE(TransportSecurityState::ParseHeader(
      "max-age=34889 includesubdomains", &max_age, &include_subdomains));

  EXPECT_EQ(max_age, 42);
  EXPECT_FALSE(include_subdomains);
}

TEST_F(TransportSecurityStateTest, ValidHeaders) {
  int max_age = 42;
  bool include_subdomains = true;

  EXPECT_TRUE(TransportSecurityState::ParseHeader(
      "max-age=243", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 243);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(TransportSecurityState::ParseHeader(
      "  Max-agE    = 567", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 567);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(TransportSecurityState::ParseHeader(
      "  mAx-aGe    = 890      ", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 890);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(TransportSecurityState::ParseHeader(
      "max-age=123;incLudesUbdOmains", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 123);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(TransportSecurityState::ParseHeader(
      "max-age=394082;  incLudesUbdOmains", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 394082);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(TransportSecurityState::ParseHeader(
      "max-age=39408299  ;incLudesUbdOmains", &max_age, &include_subdomains));
  EXPECT_EQ(max_age,
            std::min(TransportSecurityState::kMaxHSTSAgeSecs, 39408299l));
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(TransportSecurityState::ParseHeader(
      "max-age=394082038  ;  incLudesUbdOmains", &max_age, &include_subdomains));
  EXPECT_EQ(max_age,
            std::min(TransportSecurityState::kMaxHSTSAgeSecs, 394082038l));
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(TransportSecurityState::ParseHeader(
      "  max-age=0  ;  incLudesUbdOmains   ", &max_age, &include_subdomains));
  EXPECT_EQ(max_age, 0);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(TransportSecurityState::ParseHeader(
      "  max-age=999999999999999999999999999999999999999999999  ;"
      "  incLudesUbdOmains   ",
      &max_age, &include_subdomains));
  EXPECT_EQ(max_age, TransportSecurityState::kMaxHSTSAgeSecs);
  EXPECT_TRUE(include_subdomains);
}

TEST_F(TransportSecurityStateTest, SimpleMatches) {
  TransportSecurityState state("");
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "yahoo.com", true));
  domain_state.expiry = expiry;
  state.EnableHost("yahoo.com", domain_state);
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "yahoo.com", true));
}

TEST_F(TransportSecurityStateTest, MatchesCase1) {
  TransportSecurityState state("");
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "yahoo.com", true));
  domain_state.expiry = expiry;
  state.EnableHost("YAhoo.coM", domain_state);
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "yahoo.com", true));
}

TEST_F(TransportSecurityStateTest, MatchesCase2) {
  TransportSecurityState state("");
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "YAhoo.coM", true));
  domain_state.expiry = expiry;
  state.EnableHost("yahoo.com", domain_state);
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "YAhoo.coM", true));
}

TEST_F(TransportSecurityStateTest, SubdomainMatches) {
  TransportSecurityState state("");
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "yahoo.com", true));
  domain_state.expiry = expiry;
  domain_state.include_subdomains = true;
  state.EnableHost("yahoo.com", domain_state);
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "yahoo.com", true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "foo.yahoo.com", true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "foo.bar.yahoo.com",
                                     true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "foo.bar.baz.yahoo.com",
                                     true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "com", true));
}

TEST_F(TransportSecurityStateTest, Serialise1) {
  TransportSecurityState state("");
  std::string output;
  bool dirty;
  state.Serialise(&output);
  EXPECT_TRUE(state.LoadEntries(output, &dirty));
  EXPECT_FALSE(dirty);
}

TEST_F(TransportSecurityStateTest, Serialise2) {
  TransportSecurityState state("");
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "yahoo.com", true));
  domain_state.mode = TransportSecurityState::DomainState::MODE_STRICT;
  domain_state.expiry = expiry;
  domain_state.include_subdomains = true;
  state.EnableHost("yahoo.com", domain_state);

  std::string output;
  bool dirty;
  state.Serialise(&output);
  EXPECT_TRUE(state.LoadEntries(output, &dirty));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "yahoo.com", true));
  EXPECT_EQ(domain_state.mode, TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "foo.yahoo.com", true));
  EXPECT_EQ(domain_state.mode, TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "foo.bar.yahoo.com",
                                     true));
  EXPECT_EQ(domain_state.mode, TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "foo.bar.baz.yahoo.com",
                                     true));
  EXPECT_EQ(domain_state.mode, TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "com", true));
}

TEST_F(TransportSecurityStateTest, DeleteSince) {
  TransportSecurityState state("");
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  const base::Time older = current_time - base::TimeDelta::FromSeconds(1000);

  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "yahoo.com", true));
  domain_state.mode = TransportSecurityState::DomainState::MODE_STRICT;
  domain_state.expiry = expiry;
  state.EnableHost("yahoo.com", domain_state);

  state.DeleteSince(expiry);
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "yahoo.com", true));
  state.DeleteSince(older);
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "yahoo.com", true));
}

TEST_F(TransportSecurityStateTest, DeleteHost) {
  TransportSecurityState state("");
  TransportSecurityState::DomainState domain_state;
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  domain_state.mode = TransportSecurityState::DomainState::MODE_STRICT;
  domain_state.expiry = expiry;
  state.EnableHost("yahoo.com", domain_state);

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "yahoo.com", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "example.com", true));
  EXPECT_TRUE(state.DeleteHost("yahoo.com"));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "yahoo.com", true));
}

TEST_F(TransportSecurityStateTest, SerialiseOld) {
  TransportSecurityState state("");
  // This is an old-style piece of transport state JSON, which has no creation
  // date.
  std::string output =
      "{ "
      "\"NiyD+3J1r6z1wjl2n1ALBu94Zj9OsEAMo0kCN8js0Uk=\": {"
      "\"expiry\": 1266815027.983453, "
      "\"include_subdomains\": false, "
      "\"mode\": \"strict\" "
      "}"
      "}";
  bool dirty;
  EXPECT_TRUE(state.LoadEntries(output, &dirty));
  EXPECT_TRUE(dirty);
}

TEST_F(TransportSecurityStateTest, IsPreloaded) {
  TransportSecurityState state("");

  const std::string paypal =
      TransportSecurityState::CanonicalizeHost("paypal.com");
  const std::string www_paypal =
      TransportSecurityState::CanonicalizeHost("www.paypal.com");
  const std::string a_www_paypal =
      TransportSecurityState::CanonicalizeHost("a.www.paypal.com");
  const std::string abc_paypal =
      TransportSecurityState::CanonicalizeHost("a.b.c.paypal.com");
  const std::string example =
      TransportSecurityState::CanonicalizeHost("example.com");
  const std::string aypal =
      TransportSecurityState::CanonicalizeHost("aypal.com");

  TransportSecurityState::DomainState domain_state;
  EXPECT_FALSE(state.IsPreloadedSTS(paypal, true, &domain_state));
  EXPECT_TRUE(state.IsPreloadedSTS(www_paypal, true, &domain_state));
  EXPECT_FALSE(domain_state.include_subdomains);
  EXPECT_FALSE(state.IsPreloadedSTS(a_www_paypal, true, &domain_state));
  EXPECT_FALSE(state.IsPreloadedSTS(abc_paypal, true, &domain_state));
  EXPECT_FALSE(state.IsPreloadedSTS(example, true, &domain_state));
  EXPECT_FALSE(state.IsPreloadedSTS(aypal, true, &domain_state));
}

TEST_F(TransportSecurityStateTest, Preloaded) {
  TransportSecurityState state("");
  TransportSecurityState::DomainState domain_state;
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "paypal.com", true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "www.paypal.com", true));
  EXPECT_EQ(domain_state.mode,
            TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_TRUE(domain_state.preloaded);
  EXPECT_FALSE(domain_state.include_subdomains);
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "www2.paypal.com", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "a.www.paypal.com",
                                      true));

  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "elanex.biz", true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "www.elanex.biz", true));
  EXPECT_EQ(domain_state.mode,
            TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "foo.elanex.biz", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "a.foo.elanex.biz",
                                      true));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "sunshinepress.org",
                                     true));
  EXPECT_EQ(domain_state.mode,
            TransportSecurityState::DomainState::MODE_STRICT);
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "www.sunshinepress.org",
                                     true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "a.b.sunshinepress.org",
                                     true));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "www.noisebridge.net",
                                     true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "noisebridge.net",
                                      true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "foo.noisebridge.net",
                                      true));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "neg9.org", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "www.neg9.org", true));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "riseup.net", true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "foo.riseup.net", true));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "factor.cc", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "www.factor.cc", true));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "members.mayfirst.org",
                                     true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "support.mayfirst.org",
                                     true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "id.mayfirst.org", true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "lists.mayfirst.org",
                                     true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "www.mayfirst.org",
                                      true));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "splendidbacon.com",
                                     true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "www.splendidbacon.com",
                                     true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "foo.splendidbacon.com",
                                     true));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "chrome.google.com",
                                     true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "checkout.google.com",
                                     true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "health.google.com",
                                     true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "aladdinschools.appspot.com",
                                     true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "ottospora.nl", true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "www.ottospora.nl", true));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "docs.google.com", true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "sites.google.com", true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "drive.google.com", true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "spreadsheets.google.com",
                                     true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "appengine.google.com",
                                     true));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "www.paycheckrecords.com",
                                     true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "market.android.com",
                                     true));
  // The domain wasn't being set, leading to a blank string in the
  // chrome://net-internals/#hsts UI. So test that.
  EXPECT_EQ(domain_state.domain, "market.android.com");
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "sub.market.android.com",
                                     true));
  EXPECT_EQ(domain_state.domain, "market.android.com");

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "lastpass.com", true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "www.lastpass.com", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "blog.lastpass.com",
                                      true));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "keyerror.com", true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "www.keyerror.com", true));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "encrypted.google.com",
                                     true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "accounts.google.com",
                                     true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "profiles.google.com",
                                     true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "mail.google.com", true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "chatenabled.mail.google.com",
                                     true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "talkgadget.google.com",
                                     true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "hostedtalkgadget.google.com",
                                     true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "talk.google.com", true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "plus.google.com", true));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "entropia.de", true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "www.entropia.de", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "foo.entropia.de", true));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "ssl.google-analytics.com",
                                     true));

  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "www.google.com", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "google.com", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "www.youtube.com", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "youtube.com", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "i.ytimg.com", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "ytimg.com", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "googleusercontent.com",
                                      true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "www.googleusercontent.com",
                                      true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "www.google-analytics.com",
                                      true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "google-analytics.com",
                                      true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "googleapis.com", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "googleadservices.com",
                                      true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "googlecode.com", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "appspot.com", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "googlesyndication.com",
                                      true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "doubleclick.net", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "googlegroups.com",
                                      true));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "gmail.com", true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "www.gmail.com", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "m.gmail.com", true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "googlemail.com", true));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "www.googlemail.com",
                                     true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "m.googlemail.com",
                                      true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "gmail.com", false));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "www.gmail.com", false));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "m.gmail.com", false));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "googlemail.com", false));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "www.googlemail.com",
                                      false));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "m.googlemail.com",
                                      false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "www.googleplex.com",
                                     true));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "romab.com", false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "www.romab.com", false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "foo.romab.com", false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "logentries.com", false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "www.logentries.com",
                                     false));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "foo.logentries.com",
                                      false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "stripe.com", false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "foo.stripe.com", false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "cloudsecurityalliance.org",
                                     false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "foo.cloudsecurityalliance.org",
                                     false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "login.sapo.pt",
                                     false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "foo.login.sapo.pt",
                                     false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "mattmccutchen.net",
                                     false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "foo.mattmccutchen.net",
                                     false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "betnet.fr",
                                     false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "foo.betnet.fr",
                                     false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "uprotect.it",
                                     false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "foo.uprotect.it",
                                     false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "squareup.com",
                                     false));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "foo.squareup.com",
                                      false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "cert.se",
                                     false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "foo.cert.se",
                                     false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "crypto.is",
                                     false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "foo.crypto.is",
                                     false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "simon.butcher.name",
                                     false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "foo.simon.butcher.name",
                                     false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "linx.net",
                                     false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "foo.linx.net",
                                     false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "dropcam.com",
                                     false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "www.dropcam.com",
                                     false));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "foo.dropcam.com",
                                      false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "ebanking.indovinabank.com.vn",
                                     false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "foo.ebanking.indovinabank.com.vn",
                                     false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "epoxate.com",
                                     false));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "foo.epoxate.com",
                                      false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "torproject.org",
                                     false));
  EXPECT_TRUE(domain_state.public_key_hashes.size() != 0);
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "www.torproject.org",
                                     false));
  EXPECT_TRUE(domain_state.public_key_hashes.size() != 0);
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "check.torproject.org",
                                     false));
  EXPECT_TRUE(domain_state.public_key_hashes.size() != 0);
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "blog.torproject.org",
                                     false));
  EXPECT_TRUE(domain_state.public_key_hashes.size() != 0);

  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "foo.torproject.org",
                                      false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "www.moneybookers.com",
                                     false));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "moneybookers.com",
                                      false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "ledgerscope.net",
                                     false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "www.ledgerscope.net",
                                     false));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "status.ledgerscope.net",
                                      false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "kyps.net",
                                     false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "www.kyps.net",
                                     false));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "foo.kyps.net",
                                      false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "foo.app.recurly.com",
                                     false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "foo.api.recurly.com",
                                     false));

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "greplin.com",
                                     false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "www.greplin.com",
                                     false));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "foo.greplin.com",
                                      false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "luneta.nearbuysystems.com",
                                     false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "foo.luneta.nearbuysystems.com",
                                     false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "ubertt.org",
                                     false));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "foo.ubertt.org",
                                     false));


#if 0
  // Currently disabled to debug Twitter public key pins  --agl
#if defined(OS_CHROMEOS)
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "twitter.com",
                                     false));
#else
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "twitter.com",
                                      false));
#endif
#endif
}

TEST_F(TransportSecurityStateTest, LongNames) {
  TransportSecurityState state("");
  const char kLongName[] =
      "lookupByWaveIdHashAndWaveIdIdAndWaveIdDomainAndWaveletIdIdAnd"
      "WaveletIdDomainAndBlipBlipid";
  TransportSecurityState::DomainState domain_state;
  // Just checks that we don't hit a NOTREACHED.
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, kLongName, true));
}

TEST_F(TransportSecurityStateTest, PublicKeyHashes) {
  TransportSecurityState state("");
  TransportSecurityState::DomainState domain_state;
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "example.com", false));
  std::vector<SHA1Fingerprint> hashes;
  EXPECT_TRUE(domain_state.IsChainOfPublicKeysPermitted(hashes));

  SHA1Fingerprint hash;
  memset(hash.data, '1', sizeof(hash.data));
  domain_state.public_key_hashes.push_back(hash);

  EXPECT_FALSE(domain_state.IsChainOfPublicKeysPermitted(hashes));
  hashes.push_back(hash);
  EXPECT_TRUE(domain_state.IsChainOfPublicKeysPermitted(hashes));
  hashes[0].data[0] = '2';
  EXPECT_FALSE(domain_state.IsChainOfPublicKeysPermitted(hashes));

  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  domain_state.expiry = expiry;
  state.EnableHost("example.com", domain_state);
  std::string ser;
  EXPECT_TRUE(state.Serialise(&ser));
  bool dirty;
  EXPECT_TRUE(state.LoadEntries(ser, &dirty));
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "example.com", false));
  EXPECT_EQ(1u, domain_state.public_key_hashes.size());
  EXPECT_TRUE(0 == memcmp(domain_state.public_key_hashes[0].data, hash.data,
                          sizeof(hash.data)));
}

TEST_F(TransportSecurityStateTest, BuiltinCertPins) {
  TransportSecurityState state("");
  TransportSecurityState::DomainState domain_state;
  EXPECT_TRUE(state.IsEnabledForHost(&domain_state,
                                     "chrome.google.com",
                                     true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "chrome.google.com", true));
  std::vector<SHA1Fingerprint> hashes;
  // This essential checks that a built-in list does exist.
  EXPECT_FALSE(domain_state.IsChainOfPublicKeysPermitted(hashes));
  EXPECT_FALSE(state.HasPinsForHost(&domain_state, "www.paypal.com", true));

  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "docs.google.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "1.docs.google.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "sites.google.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "drive.google.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state,
                                   "spreadsheets.google.com",
                                   true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "health.google.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state,
                                   "checkout.google.com",
                                   true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state,
                                   "appengine.google.com",
                                   true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "market.android.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state,
                                   "encrypted.google.com",
                                   true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state,
                                   "accounts.google.com",
                                   true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state,
                                   "profiles.google.com",
                                   true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "mail.google.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state,
                                   "chatenabled.mail.google.com",
                                   true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state,
                                   "talkgadget.google.com",
                                   true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state,
                                   "hostedtalkgadget.google.com",
                                   true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "talk.google.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "plus.google.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "ssl.gstatic.com", true));
  EXPECT_FALSE(state.HasPinsForHost(&domain_state, "www.gstatic.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state,
                                   "ssl.google-analytics.com",
                                   true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "www.googleplex.com", true));

#if 0
  // Disabled in order to help track down pinning failures --agl
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "twitter.com", true));
  EXPECT_FALSE(state.HasPinsForHost(&domain_state, "foo.twitter.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "www.twitter.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "api.twitter.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "oauth.twitter.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "mobile.twitter.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "dev.twitter.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "business.twitter.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "platform.twitter.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "si0.twimg.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "twimg0-a.akamaihd.net", true));
#endif
}

static bool AddHash(const std::string& type_and_base64,
                    std::vector<SHA1Fingerprint>* out) {
  std::string hash_str;
  if (type_and_base64.find("sha1/") == 0 &&
      base::Base64Decode(type_and_base64.substr(5, type_and_base64.size() - 5),
                         &hash_str) &&
      hash_str.size() == base::kSHA1Length) {
    SHA1Fingerprint hash;
    memcpy(hash.data, hash_str.data(), sizeof(hash.data));
    out->push_back(hash);
    return true;
  }
  return false;
}

TEST_F(TransportSecurityStateTest, PinValidationWithRejectedCerts) {
  // kGoodPath is plus.google.com via Google Internet Authority.
  static const char* kGoodPath[] = {
    "sha1/4BjDjn8v2lWeUFQnqSs0BgbIcrU=",
    "sha1/QMVAHW+MuvCLAO3vse6H0AWzuc0=",
    "sha1/SOZo+SvSspXXR9gjIBBPM5iQn9Q=",
    NULL,
  };

  // kBadPath is plus.google.com via Trustcenter, which contains a required
  // certificate (Equifax root), but also an excluded certificate
  // (Trustcenter).
  static const char* kBadPath[] = {
    "sha1/4BjDjn8v2lWeUFQnqSs0BgbIcrU=",
    "sha1/gzuEEAB/bkqdQS3EIjk2by7lW+k=",
    "sha1/SOZo+SvSspXXR9gjIBBPM5iQn9Q=",
    NULL,
  };

  std::vector<net::SHA1Fingerprint> good_hashes, bad_hashes;

  for (size_t i = 0; kGoodPath[i]; i++) {
    EXPECT_TRUE(AddHash(kGoodPath[i], &good_hashes));
  }
  for (size_t i = 0; kBadPath[i]; i++) {
    EXPECT_TRUE(AddHash(kBadPath[i], &bad_hashes));
  }

  TransportSecurityState state("");
  TransportSecurityState::DomainState domain_state;
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "plus.google.com", true));

  EXPECT_TRUE(domain_state.IsChainOfPublicKeysPermitted(good_hashes));
  EXPECT_FALSE(domain_state.IsChainOfPublicKeysPermitted(bad_hashes));
}

TEST_F(TransportSecurityStateTest, PinValidationWithoutRejectedCerts) {
  // kGoodPath is blog.torproject.org.
  static const char* kGoodPath[] = {
    "sha1/m9lHYJYke9k0GtVZ+bXSQYE8nDI=",
    "sha1/o5OZxATDsgmwgcIfIWIneMJ0jkw=",
    "sha1/wHqYaI2J+6sFZAwRfap9ZbjKzE4=",
    NULL,
  };

  // kBadPath is plus.google.com via Trustcenter, which is utterly wrong for
  // torproject.org.
  static const char* kBadPath[] = {
    "sha1/4BjDjn8v2lWeUFQnqSs0BgbIcrU=",
    "sha1/gzuEEAB/bkqdQS3EIjk2by7lW+k=",
    "sha1/SOZo+SvSspXXR9gjIBBPM5iQn9Q=",
    NULL,
  };

  std::vector<net::SHA1Fingerprint> good_hashes, bad_hashes;

  for (size_t i = 0; kGoodPath[i]; i++) {
    EXPECT_TRUE(AddHash(kGoodPath[i], &good_hashes));
  }
  for (size_t i = 0; kBadPath[i]; i++) {
    EXPECT_TRUE(AddHash(kBadPath[i], &bad_hashes));
  }

  TransportSecurityState state("");
  TransportSecurityState::DomainState domain_state;
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "blog.torproject.org", true));

  EXPECT_TRUE(domain_state.IsChainOfPublicKeysPermitted(good_hashes));
  EXPECT_FALSE(domain_state.IsChainOfPublicKeysPermitted(bad_hashes));
}

TEST_F(TransportSecurityStateTest, OptionalHSTSCertPins) {
  TransportSecurityState state("");
  TransportSecurityState::DomainState domain_state;
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "www.google-analytics.com",
                                      false));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state,
                                      "www.google-analytics.com",
                                      true));
  EXPECT_FALSE(state.HasPinsForHost(&domain_state,
                                    "www.google-analytics.com",
                                    false));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state,
                                   "www.google-analytics.com",
                                   true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "google.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "www.google.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state,
                                   "mail-attachment.googleusercontent.com",
                                   true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "www.youtube.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "i.ytimg.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "googleapis.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state,
                                   "ajax.googleapis.com",
                                   true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state,
                                   "googleadservices.com",
                                   true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state,
                                   "pagead2.googleadservices.com",
                                   true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "googlecode.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state,
                                   "kibbles.googlecode.com",
                                   true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "appspot.com", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state,
                                   "googlesyndication.com",
                                   true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "doubleclick.net", true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "ad.doubleclick.net", true));
  EXPECT_FALSE(state.HasPinsForHost(&domain_state,
                                    "learn.doubleclick.net",
                                    true));
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "a.googlegroups.com", true));
  EXPECT_FALSE(state.HasPinsForHost(&domain_state,
                                    "a.googlegroups.com",
                                    false));
}

TEST_F(TransportSecurityStateTest, ForcePreloads) {
  // This is a docs.google.com override.
  std::string preload("{"
                      "\"4AGT3lHihuMSd5rUj7B4u6At0jlSH3HFePovjPR+oLE=\": {"
                      "\"created\": 0.0,"
                      "\"expiry\": 2000000000.0,"
                      "\"include_subdomains\": false,"
                      "\"mode\": \"none\""
                      "}}");

  TransportSecurityState state(preload);
  TransportSecurityState::DomainState domain_state;
  EXPECT_FALSE(state.HasPinsForHost(&domain_state, "docs.google.com", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "docs.google.com", true));
}

TEST_F(TransportSecurityStateTest, OverrideBuiltins) {
  TransportSecurityState state("");
  TransportSecurityState::DomainState domain_state;
  EXPECT_TRUE(state.HasPinsForHost(&domain_state, "google.com", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "google.com", true));
  EXPECT_FALSE(state.IsEnabledForHost(&domain_state, "www.google.com", true));

  domain_state = TransportSecurityState::DomainState();
  const base::Time current_time(base::Time::Now());
  const base::Time expiry = current_time + base::TimeDelta::FromSeconds(1000);
  domain_state.expiry = expiry;
  state.EnableHost("www.google.com", domain_state);

  EXPECT_TRUE(state.IsEnabledForHost(&domain_state, "www.google.com", true));
}

static const uint8 kSidePinLeafSPKI[] = {
  0x30, 0x5c, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01,
  0x01, 0x01, 0x05, 0x00, 0x03, 0x4b, 0x00, 0x30, 0x48, 0x02, 0x41, 0x00, 0xe4,
  0x1d, 0xcc, 0xf2, 0x92, 0xe7, 0x7a, 0xc6, 0x36, 0xf7, 0x1a, 0x62, 0x31, 0x7d,
  0x37, 0xea, 0x0d, 0xa2, 0xa8, 0x12, 0x2b, 0xc2, 0x1c, 0x82, 0x3e, 0xa5, 0x70,
  0x4a, 0x83, 0x5d, 0x9b, 0x84, 0x82, 0x70, 0xa4, 0x88, 0x98, 0x98, 0x41, 0x29,
  0x31, 0xcb, 0x6e, 0x2a, 0x54, 0x65, 0x14, 0x60, 0xcc, 0x00, 0xe8, 0x10, 0x30,
  0x0a, 0x4a, 0xd1, 0xa7, 0x52, 0xfe, 0x2d, 0x31, 0x2a, 0x1d, 0x0d, 0x02, 0x03,
  0x01, 0x00, 0x01,
};

static const uint8 kSidePinInfo[] = {
  0x01, 0x00, 0x53, 0x50, 0x49, 0x4e, 0xa0, 0x00, 0x03, 0x00, 0x53, 0x49, 0x47,
  0x00, 0x50, 0x55, 0x42, 0x4b, 0x41, 0x4c, 0x47, 0x4f, 0x47, 0x00, 0x41, 0x00,
  0x04, 0x00, 0x30, 0x45, 0x02, 0x21, 0x00, 0xfb, 0x26, 0xd5, 0xe8, 0x76, 0x35,
  0x96, 0x6d, 0x91, 0x9b, 0x5b, 0x27, 0xe6, 0x09, 0x1c, 0x7b, 0x6c, 0xcd, 0xc8,
  0x10, 0x25, 0x95, 0xc0, 0xa5, 0xf6, 0x6c, 0x6f, 0xfb, 0x59, 0x1e, 0x2d, 0xf4,
  0x02, 0x20, 0x33, 0x0a, 0xf8, 0x8b, 0x3e, 0xc4, 0xca, 0x75, 0x28, 0xdf, 0x5f,
  0xab, 0xe4, 0x46, 0xa0, 0xdd, 0x2d, 0xe5, 0xad, 0xc3, 0x81, 0x44, 0x70, 0xb2,
  0x10, 0x87, 0xe8, 0xc3, 0xd6, 0x6e, 0x12, 0x5d, 0x04, 0x67, 0x0b, 0x7d, 0xf2,
  0x99, 0x75, 0x57, 0x99, 0x3a, 0x98, 0xf8, 0xe4, 0xdf, 0x79, 0xdf, 0x8e, 0x02,
  0x2c, 0xbe, 0xd8, 0xfd, 0x75, 0x80, 0x18, 0xb1, 0x6f, 0x43, 0xd9, 0x8a, 0x79,
  0xc3, 0x6e, 0x18, 0xdf, 0x79, 0xc0, 0x59, 0xab, 0xd6, 0x77, 0x37, 0x6a, 0x94,
  0x5a, 0x7e, 0xfb, 0xa9, 0xc5, 0x54, 0x14, 0x3a, 0x7b, 0x97, 0x17, 0x2a, 0xb6,
  0x1e, 0x59, 0x4f, 0x2f, 0xb1, 0x15, 0x1a, 0x34, 0x50, 0x32, 0x35, 0x36,
};

static const uint8 kSidePinExpectedHash[20] = {
  0xb5, 0x91, 0x66, 0x47, 0x43, 0x16, 0x62, 0x86, 0xd4, 0x1e, 0x5d, 0x36, 0xe1,
  0xc4, 0x09, 0x3d, 0x2d, 0x1d, 0xea, 0x1e,
};

TEST_F(TransportSecurityStateTest, ParseSidePins) {

  base::StringPiece leaf_spki(reinterpret_cast<const char*>(kSidePinLeafSPKI),
                              sizeof(kSidePinLeafSPKI));
  base::StringPiece side_info(reinterpret_cast<const char*>(kSidePinInfo),
                              sizeof(kSidePinInfo));

  std::vector<SHA1Fingerprint> pub_key_hashes;
  EXPECT_TRUE(TransportSecurityState::ParseSidePin(
      leaf_spki, side_info, &pub_key_hashes));
  ASSERT_EQ(1u, pub_key_hashes.size());
  EXPECT_TRUE(0 == memcmp(pub_key_hashes[0].data, kSidePinExpectedHash,
                          sizeof(kSidePinExpectedHash)));
}

TEST_F(TransportSecurityStateTest, ParseSidePinsFailsWithBadData) {

  uint8 leaf_spki_copy[sizeof(kSidePinLeafSPKI)];
  memcpy(leaf_spki_copy, kSidePinLeafSPKI, sizeof(leaf_spki_copy));

  uint8 side_info_copy[sizeof(kSidePinInfo)];
  memcpy(side_info_copy, kSidePinInfo, sizeof(kSidePinInfo));

  base::StringPiece leaf_spki(reinterpret_cast<const char*>(leaf_spki_copy),
                              sizeof(leaf_spki_copy));
  base::StringPiece side_info(reinterpret_cast<const char*>(side_info_copy),
                              sizeof(side_info_copy));
  std::vector<SHA1Fingerprint> pub_key_hashes;

  // Tweak |leaf_spki| and expect a failure.
  leaf_spki_copy[10] ^= 1;
  EXPECT_FALSE(TransportSecurityState::ParseSidePin(
      leaf_spki, side_info, &pub_key_hashes));
  ASSERT_EQ(0u, pub_key_hashes.size());

  // Undo the change to |leaf_spki| and tweak |side_info|.
  leaf_spki_copy[10] ^= 1;
  side_info_copy[30] ^= 1;
  EXPECT_FALSE(TransportSecurityState::ParseSidePin(
      leaf_spki, side_info, &pub_key_hashes));
  ASSERT_EQ(0u, pub_key_hashes.size());
}

TEST_F(TransportSecurityStateTest, DISABLED_ParseSidePinsFuzz) {
  // Disabled because it's too slow for normal tests. Run manually when
  // changing the underlying code.

  base::StringPiece leaf_spki(reinterpret_cast<const char*>(kSidePinLeafSPKI),
                              sizeof(kSidePinLeafSPKI));
  uint8 side_info_copy[sizeof(kSidePinInfo)];
  base::StringPiece side_info(reinterpret_cast<const char*>(side_info_copy),
                              sizeof(side_info_copy));
  std::vector<SHA1Fingerprint> pub_key_hashes;
  static const size_t bit_length = sizeof(kSidePinInfo) * 8;

  for (size_t bit_to_flip = 0; bit_to_flip < bit_length; bit_to_flip++) {
    memcpy(side_info_copy, kSidePinInfo, sizeof(kSidePinInfo));

    size_t byte = bit_to_flip >> 3;
    size_t bit = bit_to_flip & 7;
    side_info_copy[byte] ^= (1 << bit);

    EXPECT_FALSE(TransportSecurityState::ParseSidePin(
        leaf_spki, side_info, &pub_key_hashes));
    ASSERT_EQ(0u, pub_key_hashes.size());
  }
}

TEST_F(TransportSecurityStateTest, GooglePinnedProperties) {
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "www.example.com", true));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "www.paypal.com", true));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "mail.twitter.com", true));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "www.google.com.int", true));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "jottit.com", true));
  // learn.doubleclick.net has a more specific match than
  // *.doubleclick.com, and has 0 or NULL for its required certs.
  // This test ensures that the exact-match-preferred behavior
  // works.
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "learn.doubleclick.net", true));

  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "encrypted.google.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "mail.google.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "accounts.google.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "doubleclick.net", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "ad.doubleclick.net", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "youtube.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "www.profiles.google.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "checkout.google.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "googleadservices.com", true));

  // Test with sni_enabled false:
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "www.example.com", false));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "www.paypal.com", false));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "checkout.google.com", false));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "googleadservices.com", false));

  // Test some SNI hosts:
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "gmail.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "googlegroups.com", true));
  EXPECT_TRUE(TransportSecurityState::IsGooglePinnedProperty(
      "www.googlegroups.com", true));
  // Expect to fail for SNI hosts when not searching the SNI list:
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "gmail.com", false));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "googlegroups.com", false));
  EXPECT_FALSE(TransportSecurityState::IsGooglePinnedProperty(
      "www.googlegroups.com", false));
}

}  // namespace net
