// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_auth_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class MockAuthHandler : public HttpAuthHandler {
 public:
  MockAuthHandler(HttpAuth::Scheme scheme,
                  const std::string& realm,
                  HttpAuth::Target target) {
    // Can't use initializer list since these are members of the base class.
    auth_scheme_ = scheme;
    realm_ = realm;
    score_ = 1;
    target_ = target;
    properties_ = 0;
  }

  virtual HttpAuth::AuthorizationResult HandleAnotherChallenge(
      HttpAuth::ChallengeTokenizer* challenge) {
    return HttpAuth::AUTHORIZATION_RESULT_REJECT;
  }

 protected:
  virtual bool Init(HttpAuth::ChallengeTokenizer* challenge) {
    return false;  // Unused.
  }

  virtual int GenerateAuthTokenImpl(const string16*,
                                    const string16*,
                                    const HttpRequestInfo*,
                                    OldCompletionCallback* callback,
                                    std::string* auth_token) {
    *auth_token = "mock-credentials";
    return OK;
  }


 private:
  ~MockAuthHandler() {}
};

const char* kRealm1 = "Realm1";
const char* kRealm2 = "Realm2";
const char* kRealm3 = "Realm3";
const char* kRealm4 = "Realm4";
const char* kRealm5 = "Realm5";
const string16 k123(ASCIIToUTF16("123"));
const string16 k1234(ASCIIToUTF16("1234"));
const string16 kAdmin(ASCIIToUTF16("admin"));
const string16 kAlice(ASCIIToUTF16("alice"));
const string16 kAlice2(ASCIIToUTF16("alice2"));
const string16 kPassword(ASCIIToUTF16("password"));
const string16 kRoot(ASCIIToUTF16("root"));
const string16 kUsername(ASCIIToUTF16("username"));
const string16 kWileCoyote(ASCIIToUTF16("wilecoyote"));

}  // namespace

// Test adding and looking-up cache entries (both by realm and by path).
TEST(HttpAuthCacheTest, Basic) {
  GURL origin("http://www.google.com");
  HttpAuthCache cache;
  HttpAuthCache::Entry* entry;

  // Add cache entries for 4 realms: "Realm1", "Realm2", "Realm3" and
  // "Realm4"

  scoped_ptr<HttpAuthHandler> realm1_handler(
      new MockAuthHandler(HttpAuth::AUTH_SCHEME_BASIC,
                          kRealm1,
                          HttpAuth::AUTH_SERVER));
  cache.Add(origin, realm1_handler->realm(), realm1_handler->auth_scheme(),
            "Basic realm=Realm1", ASCIIToUTF16("realm1-user"),
            ASCIIToUTF16("realm1-password"), "/foo/bar/index.html");

  scoped_ptr<HttpAuthHandler> realm2_handler(
      new MockAuthHandler(HttpAuth::AUTH_SCHEME_BASIC,
                          kRealm2,
                          HttpAuth::AUTH_SERVER));
  cache.Add(origin, realm2_handler->realm(), realm2_handler->auth_scheme(),
            "Basic realm=Realm2", ASCIIToUTF16("realm2-user"),
            ASCIIToUTF16("realm2-password"), "/foo2/index.html");

  scoped_ptr<HttpAuthHandler> realm3_basic_handler(
      new MockAuthHandler(HttpAuth::AUTH_SCHEME_BASIC,
                          kRealm3,
                          HttpAuth::AUTH_PROXY));
  cache.Add(origin, realm3_basic_handler->realm(),
            realm3_basic_handler->auth_scheme(), "Basic realm=Realm3",
            ASCIIToUTF16("realm3-basic-user"),
            ASCIIToUTF16("realm3-basic-password"), "");

  scoped_ptr<HttpAuthHandler> realm3_digest_handler(
      new MockAuthHandler(HttpAuth::AUTH_SCHEME_DIGEST,
                          kRealm3,
                          HttpAuth::AUTH_PROXY));
  cache.Add(origin, realm3_digest_handler->realm(),
            realm3_digest_handler->auth_scheme(), "Digest realm=Realm3",
            ASCIIToUTF16("realm3-digest-user"),
            ASCIIToUTF16("realm3-digest-password"), "/baz/index.html");

  scoped_ptr<HttpAuthHandler> realm4_basic_handler(
      new MockAuthHandler(HttpAuth::AUTH_SCHEME_BASIC,
                          kRealm4,
                          HttpAuth::AUTH_SERVER));
  cache.Add(origin, realm4_basic_handler->realm(),
            realm4_basic_handler->auth_scheme(), "Basic realm=Realm4",
            ASCIIToUTF16("realm4-basic-user"),
            ASCIIToUTF16("realm4-basic-password"), "/");

  // There is no Realm5
  entry = cache.Lookup(origin, kRealm5, HttpAuth::AUTH_SCHEME_BASIC);
  EXPECT_TRUE(NULL == entry);

  // While Realm3 does exist, the origin scheme is wrong.
  entry = cache.Lookup(GURL("https://www.google.com"), kRealm3,
                       HttpAuth::AUTH_SCHEME_BASIC);
  EXPECT_TRUE(NULL == entry);

  // Realm, origin scheme ok, authentication scheme wrong
  entry = cache.Lookup
      (GURL("http://www.google.com"), kRealm1, HttpAuth::AUTH_SCHEME_DIGEST);
  EXPECT_TRUE(NULL == entry);

  // Valid lookup by origin, realm, scheme.
  entry = cache.Lookup(
      GURL("http://www.google.com:80"), kRealm3, HttpAuth::AUTH_SCHEME_BASIC);
  ASSERT_FALSE(NULL == entry);
  EXPECT_EQ(HttpAuth::AUTH_SCHEME_BASIC, entry->scheme());
  EXPECT_EQ(kRealm3, entry->realm());
  EXPECT_EQ("Basic realm=Realm3", entry->auth_challenge());
  EXPECT_EQ(ASCIIToUTF16("realm3-basic-user"), entry->username());
  EXPECT_EQ(ASCIIToUTF16("realm3-basic-password"), entry->password());

  // Valid lookup by origin, realm, scheme when there's a duplicate
  // origin, realm in the cache
  entry = cache.Lookup(
      GURL("http://www.google.com:80"), kRealm3, HttpAuth::AUTH_SCHEME_DIGEST);
  ASSERT_FALSE(NULL == entry);
  EXPECT_EQ(HttpAuth::AUTH_SCHEME_DIGEST, entry->scheme());
  EXPECT_EQ(kRealm3, entry->realm());
  EXPECT_EQ("Digest realm=Realm3", entry->auth_challenge());
  EXPECT_EQ(ASCIIToUTF16("realm3-digest-user"), entry->username());
  EXPECT_EQ(ASCIIToUTF16("realm3-digest-password"), entry->password());

  // Valid lookup by realm.
  entry = cache.Lookup(origin, kRealm2, HttpAuth::AUTH_SCHEME_BASIC);
  ASSERT_FALSE(NULL == entry);
  EXPECT_EQ(HttpAuth::AUTH_SCHEME_BASIC, entry->scheme());
  EXPECT_EQ(kRealm2, entry->realm());
  EXPECT_EQ("Basic realm=Realm2", entry->auth_challenge());
  EXPECT_EQ(ASCIIToUTF16("realm2-user"), entry->username());
  EXPECT_EQ(ASCIIToUTF16("realm2-password"), entry->password());

  // Check that subpaths are recognized.
  HttpAuthCache::Entry* realm2_entry = cache.Lookup(
      origin, kRealm2, HttpAuth::AUTH_SCHEME_BASIC);
  HttpAuthCache::Entry* realm4_entry = cache.Lookup(
      origin, kRealm4, HttpAuth::AUTH_SCHEME_BASIC);
  EXPECT_FALSE(NULL == realm2_entry);
  EXPECT_FALSE(NULL == realm4_entry);
  // Realm4 applies to '/' and Realm2 applies to '/foo2/'.
  // LookupByPath() should return the closest enclosing path.
  // Positive tests:
  entry = cache.LookupByPath(origin, "/foo2/index.html");
  EXPECT_TRUE(realm2_entry == entry);
  entry = cache.LookupByPath(origin, "/foo2/foobar.html");
  EXPECT_TRUE(realm2_entry == entry);
  entry = cache.LookupByPath(origin, "/foo2/bar/index.html");
  EXPECT_TRUE(realm2_entry == entry);
  entry = cache.LookupByPath(origin, "/foo2/");
  EXPECT_TRUE(realm2_entry == entry);
  entry = cache.LookupByPath(origin, "/foo2");
  EXPECT_TRUE(realm4_entry == entry);
  entry = cache.LookupByPath(origin, "/");
  EXPECT_TRUE(realm4_entry == entry);

  // Negative tests:
  entry = cache.LookupByPath(origin, "/foo3/index.html");
  EXPECT_FALSE(realm2_entry == entry);
  entry = cache.LookupByPath(origin, "");
  EXPECT_FALSE(realm2_entry == entry);

  // Confirm we find the same realm, different auth scheme by path lookup
  HttpAuthCache::Entry* realm3_digest_entry =
      cache.Lookup(origin, kRealm3, HttpAuth::AUTH_SCHEME_DIGEST);
  EXPECT_FALSE(NULL == realm3_digest_entry);
  entry = cache.LookupByPath(origin, "/baz/index.html");
  EXPECT_TRUE(realm3_digest_entry == entry);
  entry = cache.LookupByPath(origin, "/baz/");
  EXPECT_TRUE(realm3_digest_entry == entry);
  entry = cache.LookupByPath(origin, "/baz");
  EXPECT_FALSE(realm3_digest_entry == entry);

  // Confirm we find the same realm, different auth scheme by path lookup
  HttpAuthCache::Entry* realm3DigestEntry =
      cache.Lookup(origin, kRealm3, HttpAuth::AUTH_SCHEME_DIGEST);
  EXPECT_FALSE(NULL == realm3DigestEntry);
  entry = cache.LookupByPath(origin, "/baz/index.html");
  EXPECT_TRUE(realm3DigestEntry == entry);
  entry = cache.LookupByPath(origin, "/baz/");
  EXPECT_TRUE(realm3DigestEntry == entry);
  entry = cache.LookupByPath(origin, "/baz");
  EXPECT_FALSE(realm3DigestEntry == entry);

  // Lookup using empty path (may be used for proxy).
  entry = cache.LookupByPath(origin, "");
  EXPECT_FALSE(NULL == entry);
  EXPECT_EQ(HttpAuth::AUTH_SCHEME_BASIC, entry->scheme());
  EXPECT_EQ(kRealm3, entry->realm());
}

TEST(HttpAuthCacheTest, AddPath) {
  HttpAuthCache::Entry entry;

  // All of these paths have a common root /1/2/2/4/5/
  entry.AddPath("/1/2/3/4/5/x.txt");
  entry.AddPath("/1/2/3/4/5/y.txt");
  entry.AddPath("/1/2/3/4/5/z.txt");

  EXPECT_EQ(1U, entry.paths_.size());
  EXPECT_EQ("/1/2/3/4/5/", entry.paths_.front());

  // Add a new entry (not a subpath).
  entry.AddPath("/1/XXX/q");
  EXPECT_EQ(2U, entry.paths_.size());
  EXPECT_EQ("/1/XXX/", entry.paths_.front());
  EXPECT_EQ("/1/2/3/4/5/", entry.paths_.back());

  // Add containing paths of /1/2/3/4/5/ -- should swallow up the deeper paths.
  entry.AddPath("/1/2/3/4/x.txt");
  EXPECT_EQ(2U, entry.paths_.size());
  EXPECT_EQ("/1/2/3/4/", entry.paths_.front());
  EXPECT_EQ("/1/XXX/", entry.paths_.back());
  entry.AddPath("/1/2/3/x");
  EXPECT_EQ(2U, entry.paths_.size());
  EXPECT_EQ("/1/2/3/", entry.paths_.front());
  EXPECT_EQ("/1/XXX/", entry.paths_.back());

  entry.AddPath("/index.html");
  EXPECT_EQ(1U, entry.paths_.size());
  EXPECT_EQ("/", entry.paths_.front());
}

// Calling Add when the realm entry already exists, should append that
// path.
TEST(HttpAuthCacheTest, AddToExistingEntry) {
  HttpAuthCache cache;
  GURL origin("http://www.foobar.com:70");
  const std::string auth_challenge = "Basic realm=MyRealm";

  scoped_ptr<HttpAuthHandler> handler(
      new MockAuthHandler(
          HttpAuth::AUTH_SCHEME_BASIC, "MyRealm", HttpAuth::AUTH_SERVER));
  HttpAuthCache::Entry* orig_entry = cache.Add(
      origin, handler->realm(), handler->auth_scheme(), auth_challenge,
      ASCIIToUTF16("user1"), ASCIIToUTF16("password1"), "/x/y/z/");
  cache.Add(origin, handler->realm(), handler->auth_scheme(), auth_challenge,
            ASCIIToUTF16("user2"), ASCIIToUTF16("password2"), "/z/y/x/");
  cache.Add(origin, handler->realm(), handler->auth_scheme(), auth_challenge,
            ASCIIToUTF16("user3"), ASCIIToUTF16("password3"), "/z/y");

  HttpAuthCache::Entry* entry = cache.Lookup(
      origin, "MyRealm", HttpAuth::AUTH_SCHEME_BASIC);

  EXPECT_TRUE(entry == orig_entry);
  EXPECT_EQ(ASCIIToUTF16("user3"), entry->username());
  EXPECT_EQ(ASCIIToUTF16("password3"), entry->password());

  EXPECT_EQ(2U, entry->paths_.size());
  EXPECT_EQ("/z/", entry->paths_.front());
  EXPECT_EQ("/x/y/z/", entry->paths_.back());
}

TEST(HttpAuthCacheTest, Remove) {
  GURL origin("http://foobar2.com");

  scoped_ptr<HttpAuthHandler> realm1_handler(
      new MockAuthHandler(
          HttpAuth::AUTH_SCHEME_BASIC, kRealm1, HttpAuth::AUTH_SERVER));

  scoped_ptr<HttpAuthHandler> realm2_handler(
      new MockAuthHandler(
          HttpAuth::AUTH_SCHEME_BASIC, kRealm2, HttpAuth::AUTH_SERVER));

  scoped_ptr<HttpAuthHandler> realm3_basic_handler(
      new MockAuthHandler(
          HttpAuth::AUTH_SCHEME_BASIC, kRealm3, HttpAuth::AUTH_SERVER));

  scoped_ptr<HttpAuthHandler> realm3_digest_handler(
      new MockAuthHandler(
          HttpAuth::AUTH_SCHEME_DIGEST, kRealm3, HttpAuth::AUTH_SERVER));

  HttpAuthCache cache;
  cache.Add(origin, realm1_handler->realm(), realm1_handler->auth_scheme(),
            "basic realm=Realm1", kAlice, k123, "/");
  cache.Add(origin, realm2_handler->realm(), realm2_handler->auth_scheme(),
            "basic realm=Realm2", ASCIIToUTF16("bob"), ASCIIToUTF16("princess"),
            "/");
  cache.Add(origin, realm3_basic_handler->realm(),
            realm3_basic_handler->auth_scheme(), "basic realm=Realm3",
            kAdmin, kPassword, "/");
  cache.Add(origin, realm3_digest_handler->realm(),
            realm3_digest_handler->auth_scheme(), "digest realm=Realm3",
            kRoot, kWileCoyote, "/");

  // Fails, because there is no realm "Realm5".
  EXPECT_FALSE(cache.Remove(
      origin, kRealm5, HttpAuth::AUTH_SCHEME_BASIC, kAlice, k123));

  // Fails because the origin is wrong.
  EXPECT_FALSE(cache.Remove(GURL("http://foobar2.com:100"),
                            kRealm1,
                            HttpAuth::AUTH_SCHEME_BASIC,
                            kAlice,
                            k123));

  // Fails because the username is wrong.
  EXPECT_FALSE(cache.Remove(
      origin, kRealm1, HttpAuth::AUTH_SCHEME_BASIC, kAlice2, k123));

  // Fails because the password is wrong.
  EXPECT_FALSE(cache.Remove(
      origin, kRealm1, HttpAuth::AUTH_SCHEME_BASIC, kAlice, k1234));

  // Fails because the authentication type is wrong.
  EXPECT_FALSE(cache.Remove(
      origin, kRealm1, HttpAuth::AUTH_SCHEME_DIGEST, kAlice, k123));

  // Succeeds.
  EXPECT_TRUE(cache.Remove(
      origin, kRealm1, HttpAuth::AUTH_SCHEME_BASIC, kAlice, k123));

  // Fails because we just deleted the entry!
  EXPECT_FALSE(cache.Remove(
      origin, kRealm1, HttpAuth::AUTH_SCHEME_BASIC, kAlice, k123));

  // Succeed when there are two authentication types for the same origin,realm.
  EXPECT_TRUE(cache.Remove(
      origin, kRealm3, HttpAuth::AUTH_SCHEME_DIGEST, kRoot, kWileCoyote));

  // Succeed as above, but when entries were added in opposite order
  cache.Add(origin, realm3_digest_handler->realm(),
            realm3_digest_handler->auth_scheme(), "digest realm=Realm3",
            kRoot, kWileCoyote, "/");
  EXPECT_TRUE(cache.Remove(
      origin, kRealm3, HttpAuth::AUTH_SCHEME_BASIC, kAdmin, kPassword));

  // Make sure that removing one entry still leaves the other available for
  // lookup.
  HttpAuthCache::Entry* entry = cache.Lookup(
      origin, kRealm3, HttpAuth::AUTH_SCHEME_DIGEST);
  EXPECT_FALSE(NULL == entry);
}

TEST(HttpAuthCacheTest, UpdateStaleChallenge) {
  HttpAuthCache cache;
  GURL origin("http://foobar2.com");
  scoped_ptr<HttpAuthHandler> digest_handler(
      new MockAuthHandler(
          HttpAuth::AUTH_SCHEME_DIGEST, kRealm1, HttpAuth::AUTH_PROXY));
  HttpAuthCache::Entry* entry_pre = cache.Add(
      origin,
      digest_handler->realm(),
      digest_handler->auth_scheme(),
      "Digest realm=Realm1,"
      "nonce=\"s3MzvFhaBAA=4c520af5acd9d8d7ae26947529d18c8eae1e98f4\"",
      ASCIIToUTF16("realm-digest-user"),
      ASCIIToUTF16("realm-digest-password"),
      "/baz/index.html");
  ASSERT_TRUE(entry_pre != NULL);

  EXPECT_EQ(2, entry_pre->IncrementNonceCount());
  EXPECT_EQ(3, entry_pre->IncrementNonceCount());
  EXPECT_EQ(4, entry_pre->IncrementNonceCount());

  bool update_success = cache.UpdateStaleChallenge(
      origin,
      digest_handler->realm(),
      digest_handler->auth_scheme(),
      "Digest realm=Realm1,"
      "nonce=\"claGgoRXBAA=7583377687842fdb7b56ba0555d175baa0b800e3\","
      "stale=\"true\"");
  EXPECT_TRUE(update_success);

  // After the stale update, the entry should still exist in the cache and
  // the nonce count should be reset to 0.
  HttpAuthCache::Entry* entry_post = cache.Lookup(
      origin,
      digest_handler->realm(),
      digest_handler->auth_scheme());
  ASSERT_TRUE(entry_post != NULL);
  EXPECT_EQ(2, entry_post->IncrementNonceCount());

  // UpdateStaleChallenge will fail if an entry doesn't exist in the cache.
  bool update_failure = cache.UpdateStaleChallenge(
      origin,
      kRealm2,
      digest_handler->auth_scheme(),
      "Digest realm=Realm2,"
      "nonce=\"claGgoRXBAA=7583377687842fdb7b56ba0555d175baa0b800e3\","
      "stale=\"true\"");
  EXPECT_FALSE(update_failure);
}

TEST(HttpAuthCacheTest, UpdateAllFrom) {
  GURL origin("http://example.com");
  std::string path("/some/path");
  std::string another_path("/another/path");

  scoped_ptr<HttpAuthHandler> realm1_handler(
      new MockAuthHandler(
          HttpAuth::AUTH_SCHEME_BASIC, kRealm1, HttpAuth::AUTH_SERVER));

  scoped_ptr<HttpAuthHandler> realm2_handler(
      new MockAuthHandler(
          HttpAuth::AUTH_SCHEME_BASIC, kRealm2, HttpAuth::AUTH_PROXY));

  scoped_ptr<HttpAuthHandler> realm3_digest_handler(
      new MockAuthHandler(
          HttpAuth::AUTH_SCHEME_DIGEST, kRealm3, HttpAuth::AUTH_SERVER));

  scoped_ptr<HttpAuthHandler> realm4_handler(
      new MockAuthHandler(
          HttpAuth::AUTH_SCHEME_BASIC, kRealm4, HttpAuth::AUTH_SERVER));

  HttpAuthCache first_cache;
  HttpAuthCache::Entry* entry;

  first_cache.Add(origin, realm1_handler->realm(),
                  realm1_handler->auth_scheme(), "basic realm=Realm1",
                  kAlice, k123, path);
  first_cache.Add(origin, realm2_handler->realm(),
                  realm2_handler->auth_scheme(), "basic realm=Realm2",
                  kAlice2, k1234, path);
  first_cache.Add(origin, realm3_digest_handler->realm(),
                  realm3_digest_handler->auth_scheme(), "digest realm=Realm3",
                  kRoot, kWileCoyote, path);
  entry = first_cache.Add(
      origin, realm3_digest_handler->realm(),
      realm3_digest_handler->auth_scheme(), "digest realm=Realm3",
      kRoot, kWileCoyote, another_path);

  EXPECT_EQ(2, entry->IncrementNonceCount());

  HttpAuthCache second_cache;
  // Will be overwritten by kRoot:kWileCoyote.
  second_cache.Add(origin, realm3_digest_handler->realm(),
                   realm3_digest_handler->auth_scheme(), "digest realm=Realm3",
                   kAlice2, k1234, path);
  // Should be left intact.
  second_cache.Add(origin, realm4_handler->realm(),
                   realm4_handler->auth_scheme(), "basic realm=Realm4",
                   kAdmin, kRoot, path);

  second_cache.UpdateAllFrom(first_cache);

  // Copied from first_cache.
  entry = second_cache.Lookup(origin, kRealm1, HttpAuth::AUTH_SCHEME_BASIC);
  EXPECT_TRUE(NULL != entry);
  EXPECT_EQ(kAlice, entry->username());
  EXPECT_EQ(k123, entry->password());

  // Copied from first_cache.
  entry = second_cache.Lookup(origin, kRealm2, HttpAuth::AUTH_SCHEME_BASIC);
  EXPECT_TRUE(NULL != entry);
  EXPECT_EQ(kAlice2, entry->username());
  EXPECT_EQ(k1234, entry->password());

  // Overwritten from first_cache.
  entry = second_cache.Lookup(origin, kRealm3, HttpAuth::AUTH_SCHEME_DIGEST);
  EXPECT_TRUE(NULL != entry);
  EXPECT_EQ(kRoot, entry->username());
  EXPECT_EQ(kWileCoyote, entry->password());
  // Nonce count should get copied.
  EXPECT_EQ(3, entry->IncrementNonceCount());

  // All paths should get copied.
  entry = second_cache.LookupByPath(origin, another_path);
  EXPECT_TRUE(NULL != entry);
  EXPECT_EQ(kRoot, entry->username());
  EXPECT_EQ(kWileCoyote, entry->password());

  // Left intact in second_cache.
  entry = second_cache.Lookup(origin, kRealm4, HttpAuth::AUTH_SCHEME_BASIC);
  EXPECT_TRUE(NULL != entry);
  EXPECT_EQ(kAdmin, entry->username());
  EXPECT_EQ(kRoot, entry->password());
}

// Test fixture class for eviction tests (contains helpers for bulk
// insertion and existence testing).
class HttpAuthCacheEvictionTest : public testing::Test {
 protected:
  HttpAuthCacheEvictionTest() : origin_("http://www.google.com") { }

  std::string GenerateRealm(int realm_i) {
    return base::StringPrintf("Realm %d", realm_i);
  }

  std::string GeneratePath(int realm_i, int path_i) {
    return base::StringPrintf("/%d/%d/x/y", realm_i, path_i);
  }

  void AddRealm(int realm_i) {
    AddPathToRealm(realm_i, 0);
  }

  void AddPathToRealm(int realm_i, int path_i) {
    cache_.Add(origin_, GenerateRealm(realm_i), HttpAuth::AUTH_SCHEME_BASIC, "",
               kUsername, kPassword, GeneratePath(realm_i, path_i));
  }

  void CheckRealmExistence(int realm_i, bool exists) {
    const HttpAuthCache::Entry* entry =
        cache_.Lookup(
            origin_, GenerateRealm(realm_i), HttpAuth::AUTH_SCHEME_BASIC);
    if (exists) {
      EXPECT_FALSE(entry == NULL);
      EXPECT_EQ(GenerateRealm(realm_i), entry->realm());
    } else {
      EXPECT_TRUE(entry == NULL);
    }
  }

  void CheckPathExistence(int realm_i, int path_i, bool exists) {
    const HttpAuthCache::Entry* entry =
        cache_.LookupByPath(origin_, GeneratePath(realm_i, path_i));
    if (exists) {
      EXPECT_FALSE(entry == NULL);
      EXPECT_EQ(GenerateRealm(realm_i), entry->realm());
    } else {
      EXPECT_TRUE(entry == NULL);
    }
  }

  GURL origin_;
  HttpAuthCache cache_;

  static const int kMaxPaths = HttpAuthCache::kMaxNumPathsPerRealmEntry;
  static const int kMaxRealms = HttpAuthCache::kMaxNumRealmEntries;
};

// Add the maxinim number of realm entries to the cache. Each of these entries
// must still be retrievable. Next add three more entries -- since the cache is
// full this causes FIFO eviction of the first three entries.
TEST_F(HttpAuthCacheEvictionTest, RealmEntryEviction) {
  for (int i = 0; i < kMaxRealms; ++i)
    AddRealm(i);

  for (int i = 0; i < kMaxRealms; ++i)
    CheckRealmExistence(i, true);

  for (int i = 0; i < 3; ++i)
    AddRealm(i + kMaxRealms);

  for (int i = 0; i < 3; ++i)
    CheckRealmExistence(i, false);

  for (int i = 0; i < kMaxRealms; ++i)
    CheckRealmExistence(i + 3, true);
}

// Add the maximum number of paths to a single realm entry. Each of these
// paths should be retrievable. Next add 3 more paths -- since the cache is
// full this causes FIFO eviction of the first three paths.
TEST_F(HttpAuthCacheEvictionTest, RealmPathEviction) {
  for (int i = 0; i < kMaxPaths; ++i)
    AddPathToRealm(0, i);

  for (int i = 1; i < kMaxRealms; ++i)
    AddRealm(i);

  for (int i = 0; i < 3; ++i)
    AddPathToRealm(0, i + kMaxPaths);

  for (int i = 0; i < 3; ++i)
    CheckPathExistence(0, i, false);

  for (int i = 0; i < kMaxPaths; ++i)
    CheckPathExistence(0, i + 3, true);

  for (int i = 0; i < kMaxRealms; ++i)
    CheckRealmExistence(i, true);
}

}  // namespace net
