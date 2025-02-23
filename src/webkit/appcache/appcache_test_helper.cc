// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_test_helper.h"

#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_entry.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_service.h"

namespace appcache {

AppCacheTestHelper::AppCacheTestHelper()
    : group_id_(0),
      appcache_id_(0),
      response_id_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(appcache_got_info_callback_(
          this, &AppCacheTestHelper::OnGotAppCacheInfo)),
      origins_(NULL) {}

AppCacheTestHelper::~AppCacheTestHelper() {}

void AppCacheTestHelper::OnGroupAndNewestCacheStored(
    AppCacheGroup* /*group*/,
    AppCache* /*newest_cache*/,
    bool success,
    bool /*would_exceed_quota*/) {
  ASSERT_TRUE(success);
  MessageLoop::current()->Quit();
}

void AppCacheTestHelper::AddGroupAndCache(AppCacheService* appcache_service,
                                          const GURL& manifest_url) {
  AppCacheGroup* appcache_group =
      new AppCacheGroup(appcache_service,
                        manifest_url,
                        ++group_id_);
  AppCache* appcache = new AppCache(appcache_service,
                                    ++appcache_id_);
  AppCacheEntry entry(AppCacheEntry::MANIFEST,
                      ++response_id_);
  appcache->AddEntry(manifest_url, entry);
  appcache->set_complete(true);
  appcache_group->AddCache(appcache);
  appcache_service->storage()->StoreGroupAndNewestCache(appcache_group,
                                                        appcache,
                                                        this);
  // OnGroupAndNewestCacheStored will quit the message loop.
  MessageLoop::current()->Run();
}

void AppCacheTestHelper::GetOriginsWithCaches(AppCacheService* appcache_service,
                                              std::set<GURL>* origins) {
  appcache_info_ = new AppCacheInfoCollection;
  origins_ = origins;
  appcache_service->GetAllAppCacheInfo(
      appcache_info_, &appcache_got_info_callback_);
  // OnGotAppCacheInfo will quit the message loop.
  MessageLoop::current()->Run();
}

void AppCacheTestHelper::OnGotAppCacheInfo(int rv) {
  typedef std::map<GURL, AppCacheInfoVector> InfoByOrigin;

  origins_->clear();
  for (InfoByOrigin::const_iterator origin =
           appcache_info_->infos_by_origin.begin();
       origin != appcache_info_->infos_by_origin.end(); ++origin) {
    origins_->insert(origin->first);
  }
  MessageLoop::current()->Quit();
}

}  // namespace appcache
