// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/memory/scoped_callback_factory.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache_quota_client.h"
#include "webkit/appcache/mock_appcache_service.h"

namespace appcache {

// Declared to shorten the line lengths.
static const quota::StorageType kTemp = quota::kStorageTypeTemporary;
static const quota::StorageType kPerm = quota::kStorageTypePersistent;

// Base class for our test fixtures.
class AppCacheQuotaClientTest : public testing::Test {
 public:
  const GURL kOriginA;
  const GURL kOriginB;
  const GURL kOriginOther;

  AppCacheQuotaClientTest()
      : kOriginA("http://host"),
        kOriginB("http://host:8000"),
        kOriginOther("http://other"),
        usage_(0),
        delete_status_(quota::kQuotaStatusUnknown),
        num_get_origin_usage_completions_(0),
        num_get_origins_completions_(0),
        num_delete_origins_completions_(0),
        callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  }

  int64 GetOriginUsage(
      quota::QuotaClient* client,
      const GURL& origin,
      quota::StorageType type) {
    usage_ = -1;
    AsyncGetOriginUsage(client, origin, type);
    MessageLoop::current()->RunAllPending();
    return usage_;
  }

  const std::set<GURL>& GetOriginsForType(
      quota::QuotaClient* client,
      quota::StorageType type) {
    origins_.clear();
    AsyncGetOriginsForType(client, type);
    MessageLoop::current()->RunAllPending();
    return origins_;
  }

  const std::set<GURL>& GetOriginsForHost(
      quota::QuotaClient* client,
      quota::StorageType type,
      const std::string& host) {
    origins_.clear();
    AsyncGetOriginsForHost(client, type, host);
    MessageLoop::current()->RunAllPending();
    return origins_;
  }

  quota::QuotaStatusCode DeleteOriginData(
      quota::QuotaClient* client,
      quota::StorageType type,
      const GURL& origin) {
    delete_status_ = quota::kQuotaStatusUnknown;
    AsyncDeleteOriginData(client, type, origin);
    MessageLoop::current()->RunAllPending();
    return delete_status_;
  }

  void AsyncGetOriginUsage(
      quota::QuotaClient* client,
      const GURL& origin,
      quota::StorageType type) {
    client->GetOriginUsage(origin, type,
        callback_factory_.NewCallback(
            &AppCacheQuotaClientTest::OnGetOriginUsageComplete));
  }

  void AsyncGetOriginsForType(
      quota::QuotaClient* client,
      quota::StorageType type) {
    client->GetOriginsForType(type,
        callback_factory_.NewCallback(
            &AppCacheQuotaClientTest::OnGetOriginsComplete));
  }

  void AsyncGetOriginsForHost(
      quota::QuotaClient* client,
      quota::StorageType type,
      const std::string& host) {
    client->GetOriginsForHost(type, host,
        callback_factory_.NewCallback(
            &AppCacheQuotaClientTest::OnGetOriginsComplete));
  }

  void AsyncDeleteOriginData(
      quota::QuotaClient* client,
      quota::StorageType type,
      const GURL& origin) {
    client->DeleteOriginData(origin, type,
        callback_factory_.NewCallback(
            &AppCacheQuotaClientTest::OnDeleteOriginDataComplete));
  }

  void SetUsageMapEntry(const GURL& origin, int64 usage) {
    mock_service_.storage()->usage_map_[origin] = usage;
  }

  AppCacheQuotaClient* CreateClient() {
    return new AppCacheQuotaClient(&mock_service_);
  }

  void Call_NotifyAppCacheReady(AppCacheQuotaClient* client) {
    client->NotifyAppCacheReady();
  }

  void Call_NotifyAppCacheDestroyed(AppCacheQuotaClient* client) {
    client->NotifyAppCacheDestroyed();
  }

  void Call_OnQuotaManagerDestroyed(AppCacheQuotaClient* client) {
    client->OnQuotaManagerDestroyed();
  }

 protected:
  void OnGetOriginUsageComplete(int64 usage) {
    ++num_get_origin_usage_completions_;
    usage_ = usage;
  }

  void OnGetOriginsComplete(const std::set<GURL>& origins,
                            quota::StorageType type) {
    ++num_get_origins_completions_;
    origins_ = origins;
    type_ = type;
  }

  void OnDeleteOriginDataComplete(quota::QuotaStatusCode status) {
    ++num_delete_origins_completions_;
    delete_status_ = status;
  }

  int64 usage_;
  std::set<GURL> origins_;
  quota::StorageType type_;
  quota::QuotaStatusCode delete_status_;
  int num_get_origin_usage_completions_;
  int num_get_origins_completions_;
  int num_delete_origins_completions_;
  MockAppCacheService mock_service_;
  base::ScopedCallbackFactory<AppCacheQuotaClientTest> callback_factory_;
};


TEST_F(AppCacheQuotaClientTest, BasicCreateDestroy) {
  AppCacheQuotaClient* client = CreateClient();
  Call_NotifyAppCacheReady(client);
  Call_OnQuotaManagerDestroyed(client);
  Call_NotifyAppCacheDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, EmptyService) {
  AppCacheQuotaClient* client = CreateClient();
  Call_NotifyAppCacheReady(client);

  EXPECT_EQ(0, GetOriginUsage(client, kOriginA, kTemp));
  EXPECT_EQ(0, GetOriginUsage(client, kOriginA, kPerm));
  EXPECT_TRUE(GetOriginsForType(client, kTemp).empty());
  EXPECT_TRUE(GetOriginsForType(client, kPerm).empty());
  EXPECT_TRUE(GetOriginsForHost(client, kTemp, kOriginA.host()).empty());
  EXPECT_TRUE(GetOriginsForHost(client, kPerm, kOriginA.host()).empty());
  EXPECT_EQ(quota::kQuotaStatusOk, DeleteOriginData(client, kTemp, kOriginA));
  EXPECT_EQ(quota::kQuotaStatusOk, DeleteOriginData(client, kPerm, kOriginA));

  Call_NotifyAppCacheDestroyed(client);
  Call_OnQuotaManagerDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, NoService) {
  AppCacheQuotaClient* client = CreateClient();
  Call_NotifyAppCacheReady(client);
  Call_NotifyAppCacheDestroyed(client);

  EXPECT_EQ(0, GetOriginUsage(client, kOriginA, kTemp));
  EXPECT_EQ(0, GetOriginUsage(client, kOriginA, kPerm));
  EXPECT_TRUE(GetOriginsForType(client, kTemp).empty());
  EXPECT_TRUE(GetOriginsForType(client, kPerm).empty());
  EXPECT_TRUE(GetOriginsForHost(client, kTemp, kOriginA.host()).empty());
  EXPECT_TRUE(GetOriginsForHost(client, kPerm, kOriginA.host()).empty());
  EXPECT_EQ(quota::kQuotaErrorAbort,
            DeleteOriginData(client, kTemp, kOriginA));
  EXPECT_EQ(quota::kQuotaErrorAbort,
            DeleteOriginData(client, kPerm, kOriginA));

  Call_OnQuotaManagerDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, GetOriginUsage) {
  AppCacheQuotaClient* client = CreateClient();
  Call_NotifyAppCacheReady(client);

  SetUsageMapEntry(kOriginA, 1000);
  EXPECT_EQ(1000, GetOriginUsage(client, kOriginA, kTemp));
  EXPECT_EQ(0, GetOriginUsage(client, kOriginA, kPerm));

  Call_NotifyAppCacheDestroyed(client);
  Call_OnQuotaManagerDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, GetOriginsForHost) {
  AppCacheQuotaClient* client = CreateClient();
  Call_NotifyAppCacheReady(client);

  EXPECT_EQ(kOriginA.host(), kOriginB.host());
  EXPECT_NE(kOriginA.host(), kOriginOther.host());

  std::set<GURL> origins = GetOriginsForHost(client, kTemp, kOriginA.host());
  EXPECT_TRUE(origins.empty());

  SetUsageMapEntry(kOriginA, 1000);
  SetUsageMapEntry(kOriginB, 10);
  SetUsageMapEntry(kOriginOther, 500);

  origins = GetOriginsForHost(client, kTemp, kOriginA.host());
  EXPECT_EQ(2ul, origins.size());
  EXPECT_TRUE(origins.find(kOriginA) != origins.end());
  EXPECT_TRUE(origins.find(kOriginB) != origins.end());

  origins = GetOriginsForHost(client, kTemp, kOriginOther.host());
  EXPECT_EQ(1ul, origins.size());
  EXPECT_TRUE(origins.find(kOriginOther) != origins.end());

  origins = GetOriginsForHost(client, kPerm, kOriginA.host());
  EXPECT_TRUE(origins.empty());

  Call_NotifyAppCacheDestroyed(client);
  Call_OnQuotaManagerDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, GetOriginsForType) {
  AppCacheQuotaClient* client = CreateClient();
  Call_NotifyAppCacheReady(client);

  EXPECT_TRUE(GetOriginsForType(client, kTemp).empty());
  EXPECT_TRUE(GetOriginsForType(client, kPerm).empty());

  SetUsageMapEntry(kOriginA, 1000);
  SetUsageMapEntry(kOriginB, 10);

  std::set<GURL> origins = GetOriginsForType(client, kTemp);
  EXPECT_EQ(2ul, origins.size());
  EXPECT_TRUE(origins.find(kOriginA) != origins.end());
  EXPECT_TRUE(origins.find(kOriginB) != origins.end());

  EXPECT_TRUE(GetOriginsForType(client, kPerm).empty());

  Call_NotifyAppCacheDestroyed(client);
  Call_OnQuotaManagerDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, DeleteOriginData) {
  AppCacheQuotaClient* client = CreateClient();
  Call_NotifyAppCacheReady(client);

  // Perm deletions are short circuited in the Client and
  // should not reach the AppCacheService.
  EXPECT_EQ(quota::kQuotaStatusOk,
            DeleteOriginData(client, kPerm, kOriginA));
  EXPECT_EQ(0, mock_service_.delete_called_count());

  EXPECT_EQ(quota::kQuotaStatusOk,
            DeleteOriginData(client, kTemp, kOriginA));
  EXPECT_EQ(1, mock_service_.delete_called_count());

  mock_service_.set_mock_delete_appcaches_for_origin_result(
      net::ERR_ABORTED);
  EXPECT_EQ(quota::kQuotaErrorAbort,
            DeleteOriginData(client, kTemp, kOriginA));
  EXPECT_EQ(2, mock_service_.delete_called_count());

  Call_OnQuotaManagerDestroyed(client);
  Call_NotifyAppCacheDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, PendingRequests) {
  AppCacheQuotaClient* client = CreateClient();

  SetUsageMapEntry(kOriginA, 1000);
  SetUsageMapEntry(kOriginB, 10);
  SetUsageMapEntry(kOriginOther, 500);

  // Queue up some reqeusts.
  AsyncGetOriginUsage(client, kOriginA, kPerm);
  AsyncGetOriginUsage(client, kOriginB, kTemp);
  AsyncGetOriginsForType(client, kPerm);
  AsyncGetOriginsForType(client, kTemp);
  AsyncGetOriginsForHost(client, kTemp, kOriginA.host());
  AsyncGetOriginsForHost(client, kTemp, kOriginOther.host());
  AsyncDeleteOriginData(client, kTemp, kOriginA);
  AsyncDeleteOriginData(client, kPerm, kOriginA);
  AsyncDeleteOriginData(client, kTemp, kOriginB);

  EXPECT_EQ(0, num_get_origin_usage_completions_);
  EXPECT_EQ(0, num_get_origins_completions_);
  EXPECT_EQ(0, num_delete_origins_completions_);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, num_get_origin_usage_completions_);
  EXPECT_EQ(0, num_get_origins_completions_);
  EXPECT_EQ(0, num_delete_origins_completions_);

  // Pending requests should get serviced when the appcache is ready.
  Call_NotifyAppCacheReady(client);
  EXPECT_EQ(2, num_get_origin_usage_completions_);
  EXPECT_EQ(4, num_get_origins_completions_);
  EXPECT_EQ(0, num_delete_origins_completions_);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(3, num_delete_origins_completions_);  // deletes are really async

  // They should be serviced in order requested.
  EXPECT_EQ(10, usage_);
  EXPECT_EQ(1ul, origins_.size());
  EXPECT_TRUE(origins_.find(kOriginOther) != origins_.end());

  Call_NotifyAppCacheDestroyed(client);
  Call_OnQuotaManagerDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, DestroyServiceWithPending) {
  AppCacheQuotaClient* client = CreateClient();

  SetUsageMapEntry(kOriginA, 1000);
  SetUsageMapEntry(kOriginB, 10);
  SetUsageMapEntry(kOriginOther, 500);

  // Queue up some reqeusts prior to being ready.
  AsyncGetOriginUsage(client, kOriginA, kPerm);
  AsyncGetOriginUsage(client, kOriginB, kTemp);
  AsyncGetOriginsForType(client, kPerm);
  AsyncGetOriginsForType(client, kTemp);
  AsyncGetOriginsForHost(client, kTemp, kOriginA.host());
  AsyncGetOriginsForHost(client, kTemp, kOriginOther.host());
  AsyncDeleteOriginData(client, kTemp, kOriginA);
  AsyncDeleteOriginData(client, kPerm, kOriginA);
  AsyncDeleteOriginData(client, kTemp, kOriginB);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, num_get_origin_usage_completions_);
  EXPECT_EQ(0, num_get_origins_completions_);
  EXPECT_EQ(0, num_delete_origins_completions_);

  // Kill the service.
  Call_NotifyAppCacheDestroyed(client);

  // All should have been aborted and called completion.
  EXPECT_EQ(2, num_get_origin_usage_completions_);
  EXPECT_EQ(4, num_get_origins_completions_);
  EXPECT_EQ(3, num_delete_origins_completions_);
  EXPECT_EQ(0, usage_);
  EXPECT_TRUE(origins_.empty());
  EXPECT_EQ(quota::kQuotaErrorAbort, delete_status_);

  Call_OnQuotaManagerDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, DestroyQuotaManagerWithPending) {
  AppCacheQuotaClient* client = CreateClient();

  SetUsageMapEntry(kOriginA, 1000);
  SetUsageMapEntry(kOriginB, 10);
  SetUsageMapEntry(kOriginOther, 500);

  // Queue up some reqeusts prior to being ready.
  AsyncGetOriginUsage(client, kOriginA, kPerm);
  AsyncGetOriginUsage(client, kOriginB, kTemp);
  AsyncGetOriginsForType(client, kPerm);
  AsyncGetOriginsForType(client, kTemp);
  AsyncGetOriginsForHost(client, kTemp, kOriginA.host());
  AsyncGetOriginsForHost(client, kTemp, kOriginOther.host());
  AsyncDeleteOriginData(client, kTemp, kOriginA);
  AsyncDeleteOriginData(client, kPerm, kOriginA);
  AsyncDeleteOriginData(client, kTemp, kOriginB);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, num_get_origin_usage_completions_);
  EXPECT_EQ(0, num_get_origins_completions_);
  EXPECT_EQ(0, num_delete_origins_completions_);

  // Kill the quota manager.
  Call_OnQuotaManagerDestroyed(client);
  Call_NotifyAppCacheReady(client);

  // Callbacks should be deleted and not called.
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, num_get_origin_usage_completions_);
  EXPECT_EQ(0, num_get_origins_completions_);
  EXPECT_EQ(0, num_delete_origins_completions_);

  Call_NotifyAppCacheDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, DestroyWithDeleteInProgress) {
  AppCacheQuotaClient* client = CreateClient();
  Call_NotifyAppCacheReady(client);

  // Start an async delete.
  AsyncDeleteOriginData(client, kTemp, kOriginB);
  EXPECT_EQ(0, num_delete_origins_completions_);

  // Kill the service.
  Call_NotifyAppCacheDestroyed(client);

  // Should have been aborted.
  EXPECT_EQ(1, num_delete_origins_completions_);
  EXPECT_EQ(quota::kQuotaErrorAbort, delete_status_);

  // A real completion callback from the service should
  // be dropped if it comes in after NotifyAppCacheDestroyed.
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(1, num_delete_origins_completions_);
  EXPECT_EQ(quota::kQuotaErrorAbort, delete_status_);

  Call_OnQuotaManagerDestroyed(client);
}

}  // namespace appcache
