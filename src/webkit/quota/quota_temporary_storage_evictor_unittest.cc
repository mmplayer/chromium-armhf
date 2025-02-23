// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include <list>
#include <map>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/task.h"
#include "webkit/quota/mock_storage_client.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/quota_temporary_storage_evictor.h"

namespace quota {

class QuotaTemporaryStorageEvictorTest;

namespace {

class MockQuotaEvictionHandler : public quota::QuotaEvictionHandler {
 public:
  MockQuotaEvictionHandler(QuotaTemporaryStorageEvictorTest *test)
      : quota_(0),
        unlimited_usage_(0),
        available_space_(0),
        error_on_evict_origin_data_(false),
        error_on_get_usage_and_quota_(false),
        test_(test) {}

  virtual void EvictOriginData(
      const GURL& origin,
      StorageType type,
      EvictOriginDataCallback* callback) OVERRIDE {
    if (error_on_evict_origin_data_) {
      callback->Run(quota::kQuotaErrorInvalidModification);
      delete callback;
      return;
    }
    int64 origin_usage = EnsureOriginRemoved(origin);
    if (origin_usage >= 0)
      available_space_ += origin_usage;
    callback->Run(quota::kQuotaStatusOk);
    delete callback;
  }

  virtual void GetUsageAndQuotaForEviction(
      const GetUsageAndQuotaForEvictionCallback& callback) OVERRIDE {
    if (error_on_get_usage_and_quota_) {
      callback.Run(quota::kQuotaErrorInvalidAccess, QuotaAndUsage());
      return;
    }
    if (task_for_get_usage_and_quota_.get())
      task_for_get_usage_and_quota_->Run();
    QuotaAndUsage quota_and_usage = {
        GetUsage(), unlimited_usage_, quota_, available_space_ };
    callback.Run(quota::kQuotaStatusOk, quota_and_usage);
  }

  virtual void GetLRUOrigin(
      StorageType type,
      const GetLRUOriginCallback& callback) OVERRIDE {
    if (origin_order_.empty())
      callback.Run(GURL());
    else
      callback.Run(GURL(origin_order_.front()));
  }

  int64 GetUsage() const {
    int64 total_usage = 0;
    for (std::map<GURL, int64>::const_iterator p = origins_.begin();
         p != origins_.end();
         ++p)
      total_usage += p->second;
    return total_usage;
  }

  void set_quota(int64 quota) {
    quota_ = quota;
  }
  void set_unlimited_usage(int64 usage) {
    unlimited_usage_ = usage;
  }
  void set_available_space(int64 available_space) {
    available_space_ = available_space;
  }
  void set_task_for_get_usage_and_quota(CancelableTask* task) {
    task_for_get_usage_and_quota_.reset(task);
  }
  void set_error_on_evict_origin_data(bool error_on_evict_origin_data) {
    error_on_evict_origin_data_ = error_on_evict_origin_data;
  }
  void set_error_on_get_usage_and_quota(bool error_on_get_usage_and_quota) {
    error_on_get_usage_and_quota_ = error_on_get_usage_and_quota;
  }

  // Simulates an access to |origin|.  It reorders the internal LRU list.
  // It internally uses AddOrigin().
  void AccessOrigin(const GURL& origin) {
    std::map<GURL, int64>::iterator found = origins_.find(origin);
    EXPECT_TRUE(origins_.end() != found);
    AddOrigin(origin, found->second);
  }

  // Simulates adding or overwriting the |origin| to the internal origin set
  // with the |usage|.  It also adds or moves the |origin| to the end of the
  // LRU list.
  void AddOrigin(const GURL& origin, int64 usage) {
    EnsureOriginRemoved(origin);
    origin_order_.push_back(origin);
    origins_[origin] = usage;
  }

 private:
  int64 EnsureOriginRemoved(const GURL& origin) {
    int64 origin_usage;
    if (origins_.find(origin) == origins_.end())
      return -1;
    else
      origin_usage = origins_[origin];

    origins_.erase(origin);
    origin_order_.remove(origin);
    return origin_usage;
  }

  int64 quota_;
  int64 unlimited_usage_;
  int64 available_space_;
  std::list<GURL> origin_order_;
  std::map<GURL, int64> origins_;
  bool error_on_evict_origin_data_;
  bool error_on_get_usage_and_quota_;

  QuotaTemporaryStorageEvictorTest *test_;
  scoped_ptr<CancelableTask> task_for_get_usage_and_quota_;
};

}  // anonymous namespace

class QuotaTemporaryStorageEvictorTest : public testing::Test {
 public:
  QuotaTemporaryStorageEvictorTest()
      : num_get_usage_and_quota_for_eviction_(0),
        runnable_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

  void SetUp() {
    quota_eviction_handler_.reset(new MockQuotaEvictionHandler(this));

    // Run multiple evictions in a single RunAllPending() when interval_ms == 0
    temporary_storage_evictor_.reset(new QuotaTemporaryStorageEvictor(
        quota_eviction_handler_.get(), 0));
  }

  void TearDown() {
    temporary_storage_evictor_.reset();
    quota_eviction_handler_.reset();
    MessageLoop::current()->RunAllPending();
  }

  void TaskForRepeatedEvictionTest(
      const std::pair<GURL, int64>& origin_to_be_added,
      const GURL& origin_to_be_accessed,
      int expected_usage_after_first,
      int expected_usage_after_second) {
    EXPECT_GE(4, num_get_usage_and_quota_for_eviction_);
    switch (num_get_usage_and_quota_for_eviction_) {
    case 2:
      EXPECT_EQ(expected_usage_after_first,
                quota_eviction_handler()->GetUsage());
      if (!origin_to_be_added.first.is_empty())
        quota_eviction_handler()->AddOrigin(origin_to_be_added.first,
                                            origin_to_be_added.second);
      if (!origin_to_be_accessed.is_empty())
        quota_eviction_handler()->AccessOrigin(origin_to_be_accessed);
      break;
    case 3:
      EXPECT_EQ(expected_usage_after_second,
                quota_eviction_handler()->GetUsage());
      temporary_storage_evictor()->set_repeated_eviction(false);
      break;
    }
    ++num_get_usage_and_quota_for_eviction_;
  }

 protected:
  MockQuotaEvictionHandler* quota_eviction_handler() const {
    return static_cast<MockQuotaEvictionHandler*>(
        quota_eviction_handler_.get());
  }

  QuotaTemporaryStorageEvictor* temporary_storage_evictor() const {
    return temporary_storage_evictor_.get();
  }

  const QuotaTemporaryStorageEvictor::Statistics& statistics() const {
    return temporary_storage_evictor()->statistics_;
  }

  void set_repeated_eviction(bool repeated_eviction) const {
    return temporary_storage_evictor_->set_repeated_eviction(repeated_eviction);
  }

  int num_get_usage_and_quota_for_eviction() const {
    return num_get_usage_and_quota_for_eviction_;
  }

  int64 default_min_available_disk_space_to_start_eviction() const {
    return 1000 * 1000 * 500;
  }

  void set_min_available_disk_space_to_start_eviction(int64 value) const {
    temporary_storage_evictor_->set_min_available_disk_space_to_start_eviction(
        value);
  }

  void reset_min_available_disk_space_to_start_eviction() const {
    temporary_storage_evictor_->
        reset_min_available_disk_space_to_start_eviction();
  }

  scoped_ptr<QuotaEvictionHandler> quota_eviction_handler_;
  scoped_ptr<QuotaTemporaryStorageEvictor> temporary_storage_evictor_;

  int num_get_usage_and_quota_for_eviction_;

  ScopedRunnableMethodFactory<QuotaTemporaryStorageEvictorTest>
      runnable_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuotaTemporaryStorageEvictorTest);
};

TEST_F(QuotaTemporaryStorageEvictorTest, SimpleEvictionTest) {
  quota_eviction_handler()->AddOrigin(GURL("http://www.z.com"), 3000);
  quota_eviction_handler()->AddOrigin(GURL("http://www.y.com"), 200);
  quota_eviction_handler()->AddOrigin(GURL("http://www.x.com"), 500);
  quota_eviction_handler()->set_quota(4000);
  quota_eviction_handler()->set_available_space(1000000000);
  EXPECT_EQ(3000 + 200 + 500, quota_eviction_handler()->GetUsage());
  set_repeated_eviction(false);
  temporary_storage_evictor()->Start();
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(200 + 500, quota_eviction_handler()->GetUsage());

  EXPECT_EQ(0, statistics().num_errors_on_evicting_origin);
  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(1, statistics().num_evicted_origins);
  EXPECT_EQ(1, statistics().num_eviction_rounds);
  EXPECT_EQ(0, statistics().num_skipped_eviction_rounds);
}

TEST_F(QuotaTemporaryStorageEvictorTest, MultipleEvictionTest) {
  quota_eviction_handler()->AddOrigin(GURL("http://www.z.com"), 20);
  quota_eviction_handler()->AddOrigin(GURL("http://www.y.com"), 2900);
  quota_eviction_handler()->AddOrigin(GURL("http://www.x.com"), 450);
  quota_eviction_handler()->AddOrigin(GURL("http://www.w.com"), 400);
  quota_eviction_handler()->set_quota(4000);
  quota_eviction_handler()->set_available_space(1000000000);
  EXPECT_EQ(20 + 2900 + 450 + 400, quota_eviction_handler()->GetUsage());
  set_repeated_eviction(false);
  temporary_storage_evictor()->Start();
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(450 + 400, quota_eviction_handler()->GetUsage());

  EXPECT_EQ(0, statistics().num_errors_on_evicting_origin);
  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(2, statistics().num_evicted_origins);
  EXPECT_EQ(1, statistics().num_eviction_rounds);
  EXPECT_EQ(0, statistics().num_skipped_eviction_rounds);
}

TEST_F(QuotaTemporaryStorageEvictorTest, RepeatedEvictionTest) {
  const int64 a_size = 400;
  const int64 b_size = 150;
  const int64 c_size = 120;
  const int64 d_size = 292;
  const int64 initial_total_size = a_size + b_size + c_size + d_size;
  const int64 e_size = 275;

  quota_eviction_handler()->AddOrigin(GURL("http://www.d.com"), d_size);
  quota_eviction_handler()->AddOrigin(GURL("http://www.c.com"), c_size);
  quota_eviction_handler()->AddOrigin(GURL("http://www.b.com"), b_size);
  quota_eviction_handler()->AddOrigin(GURL("http://www.a.com"), a_size);
  quota_eviction_handler()->set_quota(1000);
  quota_eviction_handler()->set_available_space(1000000000);
  quota_eviction_handler()->set_task_for_get_usage_and_quota(
      runnable_factory_.NewRunnableMethod(
          &QuotaTemporaryStorageEvictorTest::TaskForRepeatedEvictionTest,
          std::make_pair(GURL("http://www.e.com"), e_size),
          GURL(),
          initial_total_size - d_size,
          initial_total_size - d_size + e_size - c_size));
  EXPECT_EQ(initial_total_size, quota_eviction_handler()->GetUsage());
  temporary_storage_evictor()->Start();
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(initial_total_size - d_size + e_size - c_size - b_size,
            quota_eviction_handler()->GetUsage());
  EXPECT_EQ(5, num_get_usage_and_quota_for_eviction());

  EXPECT_EQ(0, statistics().num_errors_on_evicting_origin);
  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(3, statistics().num_evicted_origins);
  EXPECT_EQ(2, statistics().num_eviction_rounds);
  EXPECT_EQ(0, statistics().num_skipped_eviction_rounds);
}

TEST_F(QuotaTemporaryStorageEvictorTest, RepeatedEvictionSkippedTest) {
  const int64 a_size = 400;
  const int64 b_size = 150;
  const int64 c_size = 120;
  const int64 d_size = 292;
  const int64 initial_total_size = a_size + b_size + c_size + d_size;

  quota_eviction_handler()->AddOrigin(GURL("http://www.d.com"), d_size);
  quota_eviction_handler()->AddOrigin(GURL("http://www.c.com"), c_size);
  quota_eviction_handler()->AddOrigin(GURL("http://www.b.com"), b_size);
  quota_eviction_handler()->AddOrigin(GURL("http://www.a.com"), a_size);
  quota_eviction_handler()->set_quota(1000);
  quota_eviction_handler()->set_available_space(1000000000);
  quota_eviction_handler()->set_task_for_get_usage_and_quota(
      runnable_factory_.NewRunnableMethod(
          &QuotaTemporaryStorageEvictorTest::TaskForRepeatedEvictionTest,
          std::make_pair(GURL(), 0),
          GURL(),
          initial_total_size - d_size,
          initial_total_size - d_size));
  EXPECT_EQ(initial_total_size, quota_eviction_handler()->GetUsage());
  set_repeated_eviction(true);
  temporary_storage_evictor()->Start();
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(initial_total_size - d_size,
            quota_eviction_handler()->GetUsage());
  EXPECT_EQ(4, num_get_usage_and_quota_for_eviction());

  EXPECT_EQ(0, statistics().num_errors_on_evicting_origin);
  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(1, statistics().num_evicted_origins);
  EXPECT_EQ(3, statistics().num_eviction_rounds);
  EXPECT_EQ(2, statistics().num_skipped_eviction_rounds);
}

TEST_F(QuotaTemporaryStorageEvictorTest, RepeatedEvictionWithAccessOriginTest) {
  const int64 a_size = 400;
  const int64 b_size = 150;
  const int64 c_size = 120;
  const int64 d_size = 292;
  const int64 initial_total_size = a_size + b_size + c_size + d_size;
  const int64 e_size = 275;

  quota_eviction_handler()->AddOrigin(GURL("http://www.d.com"), d_size);
  quota_eviction_handler()->AddOrigin(GURL("http://www.c.com"), c_size);
  quota_eviction_handler()->AddOrigin(GURL("http://www.b.com"), b_size);
  quota_eviction_handler()->AddOrigin(GURL("http://www.a.com"), a_size);
  quota_eviction_handler()->set_quota(1000);
  quota_eviction_handler()->set_available_space(1000000000);
  quota_eviction_handler()->set_task_for_get_usage_and_quota(
      runnable_factory_.NewRunnableMethod(
          &QuotaTemporaryStorageEvictorTest::TaskForRepeatedEvictionTest,
          std::make_pair(GURL("http://www.e.com"), e_size),
          GURL("http://www.c.com"),
          initial_total_size - d_size,
          initial_total_size - d_size + e_size - b_size));
  EXPECT_EQ(initial_total_size, quota_eviction_handler()->GetUsage());
  temporary_storage_evictor()->Start();
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(initial_total_size - d_size + e_size - b_size - a_size,
            quota_eviction_handler()->GetUsage());
  EXPECT_EQ(5, num_get_usage_and_quota_for_eviction());

  EXPECT_EQ(0, statistics().num_errors_on_evicting_origin);
  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(3, statistics().num_evicted_origins);
  EXPECT_EQ(2, statistics().num_eviction_rounds);
  EXPECT_EQ(0, statistics().num_skipped_eviction_rounds);
}

TEST_F(QuotaTemporaryStorageEvictorTest, DiskSpaceNonEvictionTest) {
  quota_eviction_handler()->AddOrigin(GURL("http://www.z.com"), 414);
  quota_eviction_handler()->AddOrigin(GURL("http://www.x.com"), 450);
  quota_eviction_handler()->set_quota(10000);
  quota_eviction_handler()->set_available_space(
      default_min_available_disk_space_to_start_eviction() - 350);
  EXPECT_EQ(414 + 450, quota_eviction_handler()->GetUsage());
  reset_min_available_disk_space_to_start_eviction();
  set_repeated_eviction(false);
  temporary_storage_evictor()->Start();
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(414 + 450, quota_eviction_handler()->GetUsage());

  EXPECT_EQ(0, statistics().num_errors_on_evicting_origin);
  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(0, statistics().num_evicted_origins);
  EXPECT_EQ(1, statistics().num_eviction_rounds);
  EXPECT_EQ(1, statistics().num_skipped_eviction_rounds);
}

TEST_F(QuotaTemporaryStorageEvictorTest, DiskSpaceEvictionTest) {
  quota_eviction_handler()->AddOrigin(GURL("http://www.z.com"), 294);
  quota_eviction_handler()->AddOrigin(GURL("http://www.y.com"), 120);
  quota_eviction_handler()->AddOrigin(GURL("http://www.x.com"), 150);
  quota_eviction_handler()->AddOrigin(GURL("http://www.w.com"), 300);
  quota_eviction_handler()->set_quota(10000);
  quota_eviction_handler()->set_available_space(
      default_min_available_disk_space_to_start_eviction() - 350);
  EXPECT_EQ(294 + 120 + 150 + 300, quota_eviction_handler()->GetUsage());
  set_min_available_disk_space_to_start_eviction(
      default_min_available_disk_space_to_start_eviction());
  set_repeated_eviction(false);
  temporary_storage_evictor()->Start();
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(150 + 300, quota_eviction_handler()->GetUsage());

  EXPECT_EQ(0, statistics().num_errors_on_evicting_origin);
  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(2, statistics().num_evicted_origins);
  EXPECT_EQ(1, statistics().num_eviction_rounds);
  EXPECT_EQ(0, statistics().num_skipped_eviction_rounds);
}

TEST_F(QuotaTemporaryStorageEvictorTest, UnlimitedExclusionEvictionTest) {
  quota_eviction_handler()->AddOrigin(GURL("http://www.z.com"), 3000);
  quota_eviction_handler()->AddOrigin(GURL("http://www.y.com"), 200);
  quota_eviction_handler()->AddOrigin(GURL("http://www.x.com"), 500000);
  quota_eviction_handler()->set_unlimited_usage(500000);
  quota_eviction_handler()->set_quota(5000);
  quota_eviction_handler()->set_available_space(1000000000);
  EXPECT_EQ(3000 + 200 + 500000, quota_eviction_handler()->GetUsage());
  set_repeated_eviction(false);
  temporary_storage_evictor()->Start();
  MessageLoop::current()->RunAllPending();
  // Nothing should have been evicted.
  EXPECT_EQ(3000 + 200 + 500000, quota_eviction_handler()->GetUsage());
}

}  // namespace quota
