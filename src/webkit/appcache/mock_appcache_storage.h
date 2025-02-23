// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_MOCK_APPCACHE_STORAGE_H_
#define WEBKIT_APPCACHE_MOCK_APPCACHE_STORAGE_H_

#include <deque>
#include <map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_disk_cache.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_response.h"
#include "webkit/appcache/appcache_storage.h"

namespace appcache {

// For use in unit tests.
// Note: This class is also being used to bootstrap our development efforts.
// We can get layout tests up and running, and back fill with real storage
// somewhat in parallel.
class MockAppCacheStorage : public AppCacheStorage {
 public:
  explicit MockAppCacheStorage(AppCacheService* service);
  virtual ~MockAppCacheStorage();

  virtual void GetAllInfo(Delegate* delegate);
  virtual void LoadCache(int64 id, Delegate* delegate);
  virtual void LoadOrCreateGroup(const GURL& manifest_url, Delegate* delegate);
  virtual void StoreGroupAndNewestCache(
      AppCacheGroup* group, AppCache* newest_cache, Delegate* delegate);
  virtual void FindResponseForMainRequest(
      const GURL& url, const GURL& preferred_manifest_url, Delegate* delegate);
  virtual void FindResponseForSubRequest(
      AppCache* cache, const GURL& url,
      AppCacheEntry* found_entry, AppCacheEntry* found_fallback_entry,
      bool * found_network_namespace);
  virtual void MarkEntryAsForeign(const GURL& entry_url, int64 cache_id);
  virtual void MakeGroupObsolete(AppCacheGroup* group, Delegate* delegate);
  virtual AppCacheResponseReader* CreateResponseReader(
      const GURL& manifest_url, int64 response_id);
  virtual AppCacheResponseWriter* CreateResponseWriter(const GURL& origin);
  virtual void DoomResponses(
      const GURL& manifest_url, const std::vector<int64>& response_ids);
  virtual void DeleteResponses(
      const GURL& manifest_url, const std::vector<int64>& response_ids);
  virtual void PurgeMemory() {}

 private:
  friend class AppCacheRequestHandlerTest;
  friend class AppCacheServiceTest;
  friend class AppCacheUpdateJobTest;

  typedef base::hash_map<int64, scoped_refptr<AppCache> > StoredCacheMap;
  typedef std::map<GURL, scoped_refptr<AppCacheGroup> > StoredGroupMap;
  typedef std::set<int64> DoomedResponseIds;

  void ProcessGetAllInfo(scoped_refptr<DelegateReference> delegate_ref);
  void ProcessLoadCache(
      int64 id, scoped_refptr<DelegateReference> delegate_ref);
  void ProcessLoadOrCreateGroup(
      const GURL& manifest_url, scoped_refptr<DelegateReference> delegate_ref);
  void ProcessStoreGroupAndNewestCache(
      scoped_refptr<AppCacheGroup> group, scoped_refptr<AppCache> newest_cache,
      scoped_refptr<DelegateReference> delegate_ref);
  void ProcessMakeGroupObsolete(
      scoped_refptr<AppCacheGroup> group,
      scoped_refptr<DelegateReference> delegate_ref);
  void ProcessFindResponseForMainRequest(
      const GURL& url, scoped_refptr<DelegateReference> delegate_ref);

  void ScheduleTask(Task* task);
  void RunOnePendingTask();

  void AddStoredCache(AppCache* cache);
  void RemoveStoredCache(AppCache* cache);
  void RemoveStoredCaches(const AppCacheGroup::Caches& caches);
  bool IsCacheStored(const AppCache* cache) {
    return stored_caches_.find(cache->cache_id()) != stored_caches_.end();
  }

  void AddStoredGroup(AppCacheGroup* group);
  void RemoveStoredGroup(AppCacheGroup* group);
  bool IsGroupStored(const AppCacheGroup* group) {
    return IsGroupForManifestStored(group->manifest_url());
  }
  bool IsGroupForManifestStored(const GURL& manifest_url) {
    return stored_groups_.find(manifest_url) != stored_groups_.end();
  }

  // These helpers determine when certain operations should complete
  // asynchronously vs synchronously to faithfully mimic, or mock,
  // the behavior of the real implemenation of the AppCacheStorage
  // interface.
  bool ShouldGroupLoadAppearAsync(const AppCacheGroup* group);
  bool ShouldCacheLoadAppearAsync(const AppCache* cache);

  // Lazily constructed in-memory disk cache.
  AppCacheDiskCache* disk_cache() {
    if (!disk_cache_.get()) {
      const int kMaxCacheSize = 10 * 1024 * 1024;
      disk_cache_.reset(new AppCacheDiskCache);
      disk_cache_->InitWithMemBackend(kMaxCacheSize, NULL);
    }
    return disk_cache_.get();
  }

  // Simulate failures for testing. Once set all subsequent calls
  // to MakeGroupObsolete or StorageGroupAndNewestCache will fail.
  void SimulateMakeGroupObsoleteFailure() {
    simulate_make_group_obsolete_failure_ = true;
  }
  void SimulateStoreGroupAndNewestCacheFailure() {
    simulate_store_group_and_newest_cache_failure_ = true;
  }

  // Simulate FindResponseFor results for testing. These
  // provided values will be return on the next call to
  // the corresponding Find method, subsequent calls are
  // unaffected.
  void SimulateFindMainResource(
      const AppCacheEntry& entry,
      const GURL& fallback_url,
      const AppCacheEntry& fallback_entry,
      int64 cache_id, const GURL& manifest_url) {
    simulate_find_main_resource_ = true;
    simulate_find_sub_resource_ = false;
    simulated_found_entry_ = entry;
    simulated_found_fallback_url_ = fallback_url;
    simulated_found_fallback_entry_ = fallback_entry;
    simulated_found_cache_id_ = cache_id;
    simulated_found_manifest_url_ = manifest_url,
    simulated_found_network_namespace_ = false;  // N/A to main resource loads
  }
  void SimulateFindSubResource(
      const AppCacheEntry& entry,
      const AppCacheEntry& fallback_entry,
      bool network_namespace) {
    simulate_find_main_resource_ = false;
    simulate_find_sub_resource_ = true;
    simulated_found_entry_ = entry;
    simulated_found_fallback_entry_ = fallback_entry;
    simulated_found_cache_id_ = kNoCacheId;  // N/A to sub resource loads
    simulated_found_manifest_url_ = GURL();  // N/A to sub resource loads
    simulated_found_network_namespace_ = network_namespace;
  }

  void SimulateGetAllInfo(AppCacheInfoCollection* info) {
    simulated_appcache_info_ = info;
  }

  void SimulateResponseReader(AppCacheResponseReader* reader) {
    simulated_reader_.reset(reader);
  }

  StoredCacheMap stored_caches_;
  StoredGroupMap stored_groups_;
  DoomedResponseIds doomed_response_ids_;
  scoped_ptr<AppCacheDiskCache> disk_cache_;
  std::deque<Task*> pending_tasks_;
  ScopedRunnableMethodFactory<MockAppCacheStorage> method_factory_;

  bool simulate_make_group_obsolete_failure_;
  bool simulate_store_group_and_newest_cache_failure_;

  bool simulate_find_main_resource_;
  bool simulate_find_sub_resource_;
  AppCacheEntry simulated_found_entry_;
  AppCacheEntry simulated_found_fallback_entry_;
  int64 simulated_found_cache_id_;
  GURL simulated_found_fallback_url_;
  GURL simulated_found_manifest_url_;
  bool simulated_found_network_namespace_;
  scoped_refptr<AppCacheInfoCollection> simulated_appcache_info_;
  scoped_ptr<AppCacheResponseReader> simulated_reader_;

  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest, BasicFindMainResponse);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest,
                           BasicFindMainFallbackResponse);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest, CreateGroup);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest, FindMainResponseExclusions);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest,
                           FindMainResponseWithMultipleCandidates);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest, LoadCache_FarHit);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest, LoadGroupAndCache_FarHit);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest, MakeGroupObsolete);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest, StoreNewGroup);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest, StoreExistingGroup);
  FRIEND_TEST_ALL_PREFIXES(MockAppCacheStorageTest,
                           StoreExistingGroupExistingCache);
  FRIEND_TEST_ALL_PREFIXES(AppCacheServiceTest, DeleteAppCachesForOrigin);

  DISALLOW_COPY_AND_ASSIGN(MockAppCacheStorage);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_MOCK_APPCACHE_STORAGE_H_
