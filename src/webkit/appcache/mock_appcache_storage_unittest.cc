// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_response.h"
#include "webkit/appcache/appcache_storage.h"
#include "webkit/appcache/mock_appcache_service.h"

namespace appcache {

class MockAppCacheStorageTest : public testing::Test {
 public:
  class MockStorageDelegate : public AppCacheStorage::Delegate {
   public:
    explicit MockStorageDelegate()
        : loaded_cache_id_(0), stored_group_success_(false),
          obsoleted_success_(false), found_cache_id_(kNoCacheId) {
    }

    void OnCacheLoaded(AppCache* cache, int64 cache_id) {
      loaded_cache_ = cache;
      loaded_cache_id_ = cache_id;
    }

    void OnGroupLoaded(AppCacheGroup* group, const GURL& manifest_url) {
      loaded_group_ = group;
      loaded_manifest_url_ = manifest_url;
    }

    void OnGroupAndNewestCacheStored(
        AppCacheGroup* group, AppCache* newest_cache, bool success,
        bool would_exceed_quota) {
      stored_group_ = group;
      stored_group_success_ = success;
    }

    void OnGroupMadeObsolete(AppCacheGroup* group, bool success) {
      obsoleted_group_ = group;
      obsoleted_success_ = success;
    }

    void OnMainResponseFound(const GURL& url, const AppCacheEntry& entry,
                             const GURL& fallback_url,
                             const AppCacheEntry& fallback_entry,
                             int64 cache_id, const GURL& manifest_url) {
      found_url_ = url;
      found_entry_ = entry;
      found_fallback_url_ = fallback_url;
      found_fallback_entry_ = fallback_entry;
      found_cache_id_ = cache_id;
      found_manifest_url_ = manifest_url;
    }

    scoped_refptr<AppCache> loaded_cache_;
    int64 loaded_cache_id_;
    scoped_refptr<AppCacheGroup> loaded_group_;
    GURL loaded_manifest_url_;
    scoped_refptr<AppCacheGroup> stored_group_;
    bool stored_group_success_;
    scoped_refptr<AppCacheGroup> obsoleted_group_;
    bool obsoleted_success_;
    GURL found_url_;
    AppCacheEntry found_entry_;
    GURL found_fallback_url_;
    AppCacheEntry found_fallback_entry_;
    int64 found_cache_id_;
    GURL found_manifest_url_;
  };
};


TEST_F(MockAppCacheStorageTest, LoadCache_Miss) {
  // Attempt to load a cache that doesn't exist. Should
  // complete asyncly.
  MockAppCacheService service;
  MockStorageDelegate delegate;
  service.storage()->LoadCache(111, &delegate);
  EXPECT_NE(111, delegate.loaded_cache_id_);
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_EQ(111, delegate.loaded_cache_id_);
  EXPECT_FALSE(delegate.loaded_cache_);
}

TEST_F(MockAppCacheStorageTest, LoadCache_NearHit) {
  // Attempt to load a cache that is currently in use
  // and does not require loading from disk. This
  // load should complete syncly.
  MockAppCacheService service;

  // Setup some preconditions. Make an 'unstored' cache for
  // us to load. The ctor should put it in the working set.
  int64 cache_id = service.storage()->NewCacheId();
  scoped_refptr<AppCache> cache(new AppCache(&service, cache_id));

  // Conduct the test.
  MockStorageDelegate delegate;
  service.storage()->LoadCache(cache_id, &delegate);
  EXPECT_EQ(cache_id, delegate.loaded_cache_id_);
  EXPECT_EQ(cache.get(), delegate.loaded_cache_.get());
}

TEST_F(MockAppCacheStorageTest, CreateGroup) {
  // Attempt to load/create a group that doesn't exist.
  // Should complete asyncly.
  MockAppCacheService service;
  MockAppCacheStorage* storage =
      reinterpret_cast<MockAppCacheStorage*>(service.storage());
  MockStorageDelegate delegate;
  GURL manifest_url("http://blah/");
  service.storage()->LoadOrCreateGroup(manifest_url, &delegate);
  EXPECT_NE(manifest_url, delegate.loaded_manifest_url_);
  EXPECT_FALSE(delegate.loaded_group_.get());
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_EQ(manifest_url, delegate.loaded_manifest_url_);
  EXPECT_TRUE(delegate.loaded_group_.get());
  EXPECT_TRUE(delegate.loaded_group_->HasOneRef());
  EXPECT_FALSE(delegate.loaded_group_->newest_complete_cache());
  EXPECT_TRUE(storage->stored_groups_.empty());
}

TEST_F(MockAppCacheStorageTest, LoadGroup_NearHit) {
  // Attempt to load a group that is currently in use
  // and does not require loading from disk. This
  // load should complete syncly.
  MockAppCacheService service;
  MockStorageDelegate delegate;

  // Setup some preconditions. Create a group that appears
  // to be "unstored" and "currently in use".
  GURL manifest_url("http://blah/");
  service.storage()->LoadOrCreateGroup(manifest_url, &delegate);
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_EQ(manifest_url, delegate.loaded_manifest_url_);
  EXPECT_TRUE(delegate.loaded_group_.get());

  // Reset our delegate, and take a reference to the new group.
  scoped_refptr<AppCacheGroup> group;
  group.swap(delegate.loaded_group_);
  delegate.loaded_manifest_url_ = GURL();

  // Conduct the test.
  service.storage()->LoadOrCreateGroup(manifest_url, &delegate);
  EXPECT_EQ(manifest_url, delegate.loaded_manifest_url_);
  EXPECT_EQ(group.get(), delegate.loaded_group_.get());
}

TEST_F(MockAppCacheStorageTest, LoadGroupAndCache_FarHit) {
  // Attempt to load a cache that is not currently in use
  // and does require loading from disk. This
  // load should complete asyncly.
  MockAppCacheService service;
  MockAppCacheStorage* storage =
      reinterpret_cast<MockAppCacheStorage*>(service.storage());

  // Setup some preconditions. Create a group and newest cache that
  // appears to be "stored" and "not currently in use".
  GURL manifest_url("http://blah/");
  scoped_refptr<AppCacheGroup> group(
      new AppCacheGroup(&service, manifest_url, 111));
  int64 cache_id = storage->NewCacheId();
  scoped_refptr<AppCache> cache(new AppCache(&service, cache_id));
  cache->set_complete(true);
  group->AddCache(cache);
  storage->AddStoredGroup(group);
  storage->AddStoredCache(cache);

  // Drop the references from above so the only refs to these
  // objects are from within the storage class. This is to make
  // these objects appear as "not currently in use".
  AppCache* cache_ptr = cache.get();
  AppCacheGroup* group_ptr = group.get();
  cache = NULL;
  group = NULL;

  // Setup a delegate to receive completion callbacks.
  MockStorageDelegate delegate;

  // Conduct the cache load test.
  EXPECT_NE(cache_id, delegate.loaded_cache_id_);
  EXPECT_NE(cache_ptr, delegate.loaded_cache_.get());
  storage->LoadCache(cache_id, &delegate);
  EXPECT_NE(cache_id, delegate.loaded_cache_id_);
  EXPECT_NE(cache_ptr, delegate.loaded_cache_.get());
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_EQ(cache_id, delegate.loaded_cache_id_);
  EXPECT_EQ(cache_ptr, delegate.loaded_cache_.get());
  delegate.loaded_cache_ = NULL;

  // Conduct the group load test.
  EXPECT_NE(manifest_url, delegate.loaded_manifest_url_);
  EXPECT_FALSE(delegate.loaded_group_.get());
  storage->LoadOrCreateGroup(manifest_url, &delegate);
  EXPECT_NE(manifest_url, delegate.loaded_manifest_url_);
  EXPECT_FALSE(delegate.loaded_group_.get());
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_EQ(manifest_url, delegate.loaded_manifest_url_);
  EXPECT_EQ(group_ptr, delegate.loaded_group_.get());
}

TEST_F(MockAppCacheStorageTest, StoreNewGroup) {
  // Store a group and its newest cache. Should complete asyncly.
  MockAppCacheService service;
  MockAppCacheStorage* storage =
      reinterpret_cast<MockAppCacheStorage*>(service.storage());

  // Setup some preconditions. Create a group and newest cache that
  // appears to be "unstored".
  GURL manifest_url("http://blah/");
  scoped_refptr<AppCacheGroup> group(
      new AppCacheGroup(&service, manifest_url, 111));
  int64 cache_id = storage->NewCacheId();
  scoped_refptr<AppCache> cache(new AppCache(&service, cache_id));
  // Hold a ref to the cache simulate the UpdateJob holding that ref,
  // and hold a ref to the group to simulate the CacheHost holding that ref.

  // Conduct the store test.
  MockStorageDelegate delegate;
  EXPECT_TRUE(storage->stored_caches_.empty());
  EXPECT_TRUE(storage->stored_groups_.empty());
  storage->StoreGroupAndNewestCache(group, cache, &delegate);
  EXPECT_FALSE(delegate.stored_group_success_);
  EXPECT_TRUE(storage->stored_caches_.empty());
  EXPECT_TRUE(storage->stored_groups_.empty());
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_TRUE(delegate.stored_group_success_);
  EXPECT_FALSE(storage->stored_caches_.empty());
  EXPECT_FALSE(storage->stored_groups_.empty());
  EXPECT_EQ(cache, group->newest_complete_cache());
  EXPECT_TRUE(cache->is_complete());
}

TEST_F(MockAppCacheStorageTest, StoreExistingGroup) {
  // Store a group and its newest cache. Should complete asyncly.
  MockAppCacheService service;
  MockAppCacheStorage* storage =
      reinterpret_cast<MockAppCacheStorage*>(service.storage());

  // Setup some preconditions. Create a group and old complete cache
  // that appear to be "stored", and a newest unstored complete cache.
  GURL manifest_url("http://blah/");
  scoped_refptr<AppCacheGroup> group(
      new AppCacheGroup(&service, manifest_url, 111));
  int64 old_cache_id = storage->NewCacheId();
  scoped_refptr<AppCache> old_cache(new AppCache(&service, old_cache_id));
  old_cache->set_complete(true);
  group->AddCache(old_cache);
  storage->AddStoredGroup(group);
  storage->AddStoredCache(old_cache);
  int64 new_cache_id = storage->NewCacheId();
  scoped_refptr<AppCache> new_cache(new AppCache(&service, new_cache_id));
  // Hold our refs to simulate the UpdateJob holding these refs.

  // Conduct the test.
  MockStorageDelegate delegate;
  EXPECT_EQ(size_t(1), storage->stored_caches_.size());
  EXPECT_EQ(size_t(1), storage->stored_groups_.size());
  EXPECT_TRUE(storage->IsCacheStored(old_cache));
  EXPECT_FALSE(storage->IsCacheStored(new_cache));
  storage->StoreGroupAndNewestCache(group, new_cache, &delegate);
  EXPECT_FALSE(delegate.stored_group_success_);
  EXPECT_EQ(size_t(1), storage->stored_caches_.size());
  EXPECT_EQ(size_t(1), storage->stored_groups_.size());
  EXPECT_TRUE(storage->IsCacheStored(old_cache));
  EXPECT_FALSE(storage->IsCacheStored(new_cache));
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_TRUE(delegate.stored_group_success_);
  EXPECT_EQ(size_t(1), storage->stored_caches_.size());
  EXPECT_EQ(size_t(1), storage->stored_groups_.size());
  EXPECT_FALSE(storage->IsCacheStored(old_cache));
  EXPECT_TRUE(storage->IsCacheStored(new_cache));
  EXPECT_EQ(new_cache.get(), group->newest_complete_cache());
  EXPECT_TRUE(new_cache->is_complete());
}

TEST_F(MockAppCacheStorageTest, StoreExistingGroupExistingCache) {
  // Store a group with updates to its existing newest complete cache.
  MockAppCacheService service;
  MockAppCacheStorage* storage =
      reinterpret_cast<MockAppCacheStorage*>(service.storage());

  // Setup some preconditions. Create a group and a complete cache that
  // appear to be "stored".
  GURL manifest_url("http://blah");
  scoped_refptr<AppCacheGroup> group(
      new AppCacheGroup(&service, manifest_url, 111));
  int64 cache_id = storage->NewCacheId();
  scoped_refptr<AppCache> cache(new AppCache(&service, cache_id));
  cache->set_complete(true);
  group->AddCache(cache);
  storage->AddStoredGroup(group);
  storage->AddStoredCache(cache);
  // Hold our refs to simulate the UpdateJob holding these refs.

  // Change the group's newest cache.
  EXPECT_EQ(cache, group->newest_complete_cache());
  GURL entry_url("http://blah/blah");
  cache->AddEntry(entry_url, AppCacheEntry(AppCacheEntry::MASTER));

  // Conduct the test.
  MockStorageDelegate delegate;
  EXPECT_EQ(size_t(1), storage->stored_caches_.size());
  EXPECT_EQ(size_t(1), storage->stored_groups_.size());
  EXPECT_TRUE(storage->IsCacheStored(cache));
  storage->StoreGroupAndNewestCache(group, cache, &delegate);
  EXPECT_FALSE(delegate.stored_group_success_);
  EXPECT_EQ(size_t(1), storage->stored_caches_.size());
  EXPECT_EQ(size_t(1), storage->stored_groups_.size());
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_TRUE(delegate.stored_group_success_);
  EXPECT_EQ(size_t(1), storage->stored_caches_.size());
  EXPECT_EQ(size_t(1), storage->stored_groups_.size());
  EXPECT_TRUE(storage->IsCacheStored(cache));
  EXPECT_EQ(cache, group->newest_complete_cache());
  EXPECT_TRUE(cache->GetEntry(entry_url));
}

TEST_F(MockAppCacheStorageTest, MakeGroupObsolete) {
  // Make a group obsolete, should complete asyncly.
  MockAppCacheService service;
  MockAppCacheStorage* storage =
      reinterpret_cast<MockAppCacheStorage*>(service.storage());

  // Setup some preconditions. Create a group and newest cache that
  // appears to be "stored" and "currently in use".
  GURL manifest_url("http://blah/");
  scoped_refptr<AppCacheGroup> group(
      new AppCacheGroup(&service, manifest_url, 111));
  int64 cache_id = storage->NewCacheId();
  scoped_refptr<AppCache> cache(new AppCache(&service, cache_id));
  cache->set_complete(true);
  group->AddCache(cache);
  storage->AddStoredGroup(group);
  storage->AddStoredCache(cache);
  // Hold our refs to simulate the UpdateJob holding these refs.

  // Conduct the test.
  MockStorageDelegate delegate;
  EXPECT_FALSE(group->is_obsolete());
  EXPECT_EQ(size_t(1), storage->stored_caches_.size());
  EXPECT_EQ(size_t(1), storage->stored_groups_.size());
  EXPECT_FALSE(cache->HasOneRef());
  EXPECT_FALSE(group->HasOneRef());
  storage->MakeGroupObsolete(group, &delegate);
  EXPECT_FALSE(group->is_obsolete());
  EXPECT_EQ(size_t(1), storage->stored_caches_.size());
  EXPECT_EQ(size_t(1), storage->stored_groups_.size());
  EXPECT_FALSE(cache->HasOneRef());
  EXPECT_FALSE(group->HasOneRef());
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_TRUE(delegate.obsoleted_success_);
  EXPECT_EQ(group.get(), delegate.obsoleted_group_.get());
  EXPECT_TRUE(group->is_obsolete());
  EXPECT_TRUE(storage->stored_caches_.empty());
  EXPECT_TRUE(storage->stored_groups_.empty());
  EXPECT_TRUE(cache->HasOneRef());
  EXPECT_FALSE(group->HasOneRef());
  delegate.obsoleted_group_ = NULL;
  cache = NULL;
  EXPECT_TRUE(group->HasOneRef());
}

TEST_F(MockAppCacheStorageTest, MarkEntryAsForeign) {
  // Should complete syncly.
  MockAppCacheService service;
  MockAppCacheStorage* storage =
      reinterpret_cast<MockAppCacheStorage*>(service.storage());

  // Setup some preconditions. Create a cache with an entry.
  GURL entry_url("http://blah/entry");
  int64 cache_id = storage->NewCacheId();
  scoped_refptr<AppCache> cache(new AppCache(&service, cache_id));
  cache->AddEntry(entry_url, AppCacheEntry(AppCacheEntry::EXPLICIT));

  // Conduct the test.
  MockStorageDelegate delegate;
  EXPECT_FALSE(cache->GetEntry(entry_url)->IsForeign());
  storage->MarkEntryAsForeign(entry_url, cache_id);
  EXPECT_TRUE(cache->GetEntry(entry_url)->IsForeign());
  EXPECT_TRUE(cache->GetEntry(entry_url)->IsExplicit());
}

TEST_F(MockAppCacheStorageTest, FindNoMainResponse) {
  // Should complete asyncly.
  MockAppCacheService service;
  MockAppCacheStorage* storage =
      reinterpret_cast<MockAppCacheStorage*>(service.storage());

  // Conduct the test.
  MockStorageDelegate delegate;
  GURL url("http://blah/some_url");
  EXPECT_NE(url, delegate.found_url_);
  storage->FindResponseForMainRequest(url, GURL(), &delegate);
  EXPECT_NE(url, delegate.found_url_);
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_EQ(url, delegate.found_url_);
  EXPECT_TRUE(delegate.found_manifest_url_.is_empty());
  EXPECT_EQ(kNoCacheId, delegate.found_cache_id_);
  EXPECT_EQ(kNoResponseId, delegate.found_entry_.response_id());
  EXPECT_EQ(kNoResponseId, delegate.found_fallback_entry_.response_id());
  EXPECT_TRUE(delegate.found_fallback_url_.is_empty());
  EXPECT_EQ(0, delegate.found_entry_.types());
  EXPECT_EQ(0, delegate.found_fallback_entry_.types());
}

TEST_F(MockAppCacheStorageTest, BasicFindMainResponse) {
  // Should complete asyncly.
  MockAppCacheService service;
  MockAppCacheStorage* storage =
      reinterpret_cast<MockAppCacheStorage*>(service.storage());

  // Setup some preconditions. Create a complete cache with an entry.
  const int64 kCacheId = storage->NewCacheId();
  const GURL kEntryUrl("http://blah/entry");
  const GURL kManifestUrl("http://blah/manifest");
  const int64 kResponseId = 1;
  scoped_refptr<AppCache> cache(new AppCache(&service, kCacheId));
  cache->AddEntry(
      kEntryUrl, AppCacheEntry(AppCacheEntry::EXPLICIT, kResponseId));
  cache->set_complete(true);
  scoped_refptr<AppCacheGroup> group(
      new AppCacheGroup(&service, kManifestUrl, 111));
  group->AddCache(cache);
  storage->AddStoredGroup(group);
  storage->AddStoredCache(cache);

  // Conduct the test.
  MockStorageDelegate delegate;
  EXPECT_NE(kEntryUrl, delegate.found_url_);
  storage->FindResponseForMainRequest(kEntryUrl, GURL(), &delegate);
  EXPECT_NE(kEntryUrl, delegate.found_url_);
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_EQ(kEntryUrl, delegate.found_url_);
  EXPECT_EQ(kManifestUrl, delegate.found_manifest_url_);
  EXPECT_EQ(kCacheId, delegate.found_cache_id_);
  EXPECT_EQ(kResponseId, delegate.found_entry_.response_id());
  EXPECT_TRUE(delegate.found_entry_.IsExplicit());
  EXPECT_FALSE(delegate.found_fallback_entry_.has_response_id());
}

TEST_F(MockAppCacheStorageTest, BasicFindMainFallbackResponse) {
  // Should complete asyncly.
  MockAppCacheService service;
  MockAppCacheStorage* storage =
      reinterpret_cast<MockAppCacheStorage*>(service.storage());

  // Setup some preconditions. Create a complete cache with a
  // fallback namespace and entry.
  const int64 kCacheId = storage->NewCacheId();
  const GURL kFallbackEntryUrl1("http://blah/fallback_entry1");
  const GURL kFallbackNamespaceUrl1("http://blah/fallback_namespace/");
  const GURL kFallbackEntryUrl2("http://blah/fallback_entry2");
  const GURL kFallbackNamespaceUrl2("http://blah/fallback_namespace/longer");
  const GURL kManifestUrl("http://blah/manifest");
  const int64 kResponseId1 = 1;
  const int64 kResponseId2 = 2;

  Manifest manifest;
  manifest.fallback_namespaces.push_back(
      FallbackNamespace(kFallbackNamespaceUrl1, kFallbackEntryUrl1));
  manifest.fallback_namespaces.push_back(
      FallbackNamespace(kFallbackNamespaceUrl2, kFallbackEntryUrl2));

  scoped_refptr<AppCache> cache(new AppCache(&service, kCacheId));
  cache->InitializeWithManifest(&manifest);
  cache->AddEntry(
      kFallbackEntryUrl1, AppCacheEntry(AppCacheEntry::FALLBACK, kResponseId1));
  cache->AddEntry(
      kFallbackEntryUrl2, AppCacheEntry(AppCacheEntry::FALLBACK, kResponseId2));
  cache->set_complete(true);

  scoped_refptr<AppCacheGroup> group(
      new AppCacheGroup(&service, kManifestUrl, 111));
  group->AddCache(cache);
  storage->AddStoredGroup(group);
  storage->AddStoredCache(cache);

  // The test url is in both fallback namespace urls, but should match
  // the longer of the two.
  const GURL kTestUrl("http://blah/fallback_namespace/longer/test");

  // Conduct the test.
  MockStorageDelegate delegate;
  EXPECT_NE(kTestUrl, delegate.found_url_);
  storage->FindResponseForMainRequest(kTestUrl, GURL(), &delegate);
  EXPECT_NE(kTestUrl, delegate.found_url_);
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_EQ(kTestUrl, delegate.found_url_);
  EXPECT_EQ(kManifestUrl, delegate.found_manifest_url_);
  EXPECT_EQ(kCacheId, delegate.found_cache_id_);
  EXPECT_FALSE(delegate.found_entry_.has_response_id());
  EXPECT_EQ(kResponseId2, delegate.found_fallback_entry_.response_id());
  EXPECT_EQ(kFallbackEntryUrl2, delegate.found_fallback_url_);
  EXPECT_TRUE(delegate.found_fallback_entry_.IsFallback());
}

TEST_F(MockAppCacheStorageTest, FindMainResponseWithMultipleCandidates) {
  // Should complete asyncly.
  MockAppCacheService service;
  MockAppCacheStorage* storage =
      reinterpret_cast<MockAppCacheStorage*>(service.storage());

  // Setup some preconditions. Create 2 complete caches with an entry
  // for the same url.

  const GURL kEntryUrl("http://blah/entry");
  const int64 kCacheId1 = storage->NewCacheId();
  const int64 kCacheId2 = storage->NewCacheId();
  const GURL kManifestUrl1("http://blah/manifest1");
  const GURL kManifestUrl2("http://blah/manifest2");
  const int64 kResponseId1 = 1;
  const int64 kResponseId2 = 2;

  // The first cache.
  scoped_refptr<AppCache> cache(new AppCache(&service, kCacheId1));
  cache->AddEntry(
      kEntryUrl, AppCacheEntry(AppCacheEntry::EXPLICIT, kResponseId1));
  cache->set_complete(true);
  scoped_refptr<AppCacheGroup> group(
      new AppCacheGroup(&service, kManifestUrl1, 111));
  group->AddCache(cache);
  storage->AddStoredGroup(group);
  storage->AddStoredCache(cache);
  // Drop our references to cache1 so it appears as "not in use".
  cache = NULL;
  group = NULL;

  // The second cache.
  cache = new AppCache(&service, kCacheId2);
  cache->AddEntry(
      kEntryUrl, AppCacheEntry(AppCacheEntry::EXPLICIT, kResponseId2));
  cache->set_complete(true);
  group = new AppCacheGroup(&service, kManifestUrl2, 222);
  group->AddCache(cache);
  storage->AddStoredGroup(group);
  storage->AddStoredCache(cache);

  // Conduct the test, we should find the response from the second cache
  // since it's "in use".
  MockStorageDelegate delegate;
  EXPECT_NE(kEntryUrl, delegate.found_url_);
  storage->FindResponseForMainRequest(kEntryUrl, GURL(), &delegate);
  EXPECT_NE(kEntryUrl, delegate.found_url_);
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_EQ(kEntryUrl, delegate.found_url_);
  EXPECT_EQ(kManifestUrl2, delegate.found_manifest_url_);
  EXPECT_EQ(kCacheId2, delegate.found_cache_id_);
  EXPECT_EQ(kResponseId2, delegate.found_entry_.response_id());
  EXPECT_TRUE(delegate.found_entry_.IsExplicit());
  EXPECT_FALSE(delegate.found_fallback_entry_.has_response_id());
}

TEST_F(MockAppCacheStorageTest, FindMainResponseExclusions) {
  // Should complete asyncly.
  MockAppCacheService service;
  MockAppCacheStorage* storage =
      reinterpret_cast<MockAppCacheStorage*>(service.storage());

  // Setup some preconditions. Create a complete cache with a
  // foreign entry and an online namespace.

  const int64 kCacheId = storage->NewCacheId();
  const GURL kEntryUrl("http://blah/entry");
  const GURL kManifestUrl("http://blah/manifest");
  const GURL kOnlineNamespaceUrl("http://blah/online_namespace");
  const int64 kResponseId = 1;

  Manifest manifest;
  manifest.online_whitelist_namespaces.push_back(kOnlineNamespaceUrl);

  scoped_refptr<AppCache> cache(new AppCache(&service, kCacheId));
  cache->InitializeWithManifest(&manifest);
  cache->AddEntry(
      kEntryUrl,
      AppCacheEntry(AppCacheEntry::EXPLICIT | AppCacheEntry::FOREIGN,
                    kResponseId));
  cache->set_complete(true);
  scoped_refptr<AppCacheGroup> group(
      new AppCacheGroup(&service, kManifestUrl, 111));
  group->AddCache(cache);
  storage->AddStoredGroup(group);
  storage->AddStoredCache(cache);

  MockStorageDelegate delegate;

  // We should not find anything for the foreign entry.
  EXPECT_NE(kEntryUrl, delegate.found_url_);
  storage->FindResponseForMainRequest(kEntryUrl, GURL(), &delegate);
  EXPECT_NE(kEntryUrl, delegate.found_url_);
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_EQ(kEntryUrl, delegate.found_url_);
  EXPECT_TRUE(delegate.found_manifest_url_.is_empty());
  EXPECT_EQ(kNoCacheId, delegate.found_cache_id_);
  EXPECT_EQ(kNoResponseId, delegate.found_entry_.response_id());
  EXPECT_EQ(kNoResponseId, delegate.found_fallback_entry_.response_id());
  EXPECT_TRUE(delegate.found_fallback_url_.is_empty());
  EXPECT_EQ(0, delegate.found_entry_.types());
  EXPECT_EQ(0, delegate.found_fallback_entry_.types());

  // We should not find anything for the online namespace.
  EXPECT_NE(kOnlineNamespaceUrl, delegate.found_url_);
  storage->FindResponseForMainRequest(kOnlineNamespaceUrl, GURL(), &delegate);
  EXPECT_NE(kOnlineNamespaceUrl, delegate.found_url_);
  MessageLoop::current()->RunAllPending();  // Do async task execution.
  EXPECT_EQ(kOnlineNamespaceUrl, delegate.found_url_);
  EXPECT_TRUE(delegate.found_manifest_url_.is_empty());
  EXPECT_EQ(kNoCacheId, delegate.found_cache_id_);
  EXPECT_EQ(kNoResponseId, delegate.found_entry_.response_id());
  EXPECT_EQ(kNoResponseId, delegate.found_fallback_entry_.response_id());
  EXPECT_TRUE(delegate.found_fallback_url_.is_empty());
  EXPECT_EQ(0, delegate.found_entry_.types());
  EXPECT_EQ(0, delegate.found_fallback_entry_.types());
}

}  // namespace appcache

