// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Bogus class to act as a NotificationSource for the messages.
class TestSource {};

class TestObserver : public NotificationObserver {
public:
  TestObserver() : notification_count_(0) {}

  int notification_count() { return notification_count_; }

  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    ++notification_count_;
  }

private:
  int notification_count_;
};

}  // namespace


class NotificationServiceTest : public testing::Test {
 protected:
  NotificationRegistrar registrar_;
};

TEST_F(NotificationServiceTest, Basic) {
  TestSource test_source;
  TestSource other_source;

  // Check the equality operators defined for NotificationSource
  EXPECT_TRUE(
    Source<TestSource>(&test_source) == Source<TestSource>(&test_source));
  EXPECT_TRUE(
    Source<TestSource>(&test_source) != Source<TestSource>(&other_source));

  TestObserver all_types_all_sources;
  TestObserver idle_all_sources;
  TestObserver all_types_test_source;
  TestObserver idle_test_source;

  // Make sure it doesn't freak out when there are no observers.
  NotificationService* service = NotificationService::current();
  service->Notify(content::NOTIFICATION_IDLE,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());

  registrar_.Add(&all_types_all_sources, content::NOTIFICATION_ALL,
                 NotificationService::AllSources());
  registrar_.Add(&idle_all_sources, content::NOTIFICATION_IDLE,
                 NotificationService::AllSources());
  registrar_.Add(&all_types_test_source, content::NOTIFICATION_ALL,
                 Source<TestSource>(&test_source));
  registrar_.Add(&idle_test_source, content::NOTIFICATION_IDLE,
                 Source<TestSource>(&test_source));

  EXPECT_EQ(0, all_types_all_sources.notification_count());
  EXPECT_EQ(0, idle_all_sources.notification_count());
  EXPECT_EQ(0, all_types_test_source.notification_count());
  EXPECT_EQ(0, idle_test_source.notification_count());

  service->Notify(content::NOTIFICATION_IDLE,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(1, all_types_all_sources.notification_count());
  EXPECT_EQ(1, idle_all_sources.notification_count());
  EXPECT_EQ(1, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  service->Notify(content::NOTIFICATION_BUSY,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(2, all_types_all_sources.notification_count());
  EXPECT_EQ(1, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  service->Notify(content::NOTIFICATION_IDLE,
                  Source<TestSource>(&other_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(3, all_types_all_sources.notification_count());
  EXPECT_EQ(2, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  service->Notify(content::NOTIFICATION_BUSY,
                  Source<TestSource>(&other_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(4, all_types_all_sources.notification_count());
  EXPECT_EQ(2, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  // Try send with NULL source.
  service->Notify(content::NOTIFICATION_IDLE,
                  NotificationService::AllSources(),
                  NotificationService::NoDetails());

  EXPECT_EQ(5, all_types_all_sources.notification_count());
  EXPECT_EQ(3, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  registrar_.RemoveAll();

  service->Notify(content::NOTIFICATION_IDLE,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(5, all_types_all_sources.notification_count());
  EXPECT_EQ(3, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());
}

TEST_F(NotificationServiceTest, MultipleRegistration) {
  TestSource test_source;

  TestObserver idle_test_source;

  NotificationService* service = NotificationService::current();

  registrar_.Add(&idle_test_source, content::NOTIFICATION_IDLE,
                 Source<TestSource>(&test_source));
  registrar_.Add(&idle_test_source, content::NOTIFICATION_ALL,
                 Source<TestSource>(&test_source));

  service->Notify(content::NOTIFICATION_IDLE,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());
  EXPECT_EQ(2, idle_test_source.notification_count());

  registrar_.Remove(&idle_test_source, content::NOTIFICATION_IDLE,
                    Source<TestSource>(&test_source));

  service->Notify(content::NOTIFICATION_IDLE,
                 Source<TestSource>(&test_source),
                 NotificationService::NoDetails());
  EXPECT_EQ(3, idle_test_source.notification_count());

  registrar_.Remove(&idle_test_source, content::NOTIFICATION_ALL,
                    Source<TestSource>(&test_source));

  service->Notify(content::NOTIFICATION_IDLE,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());
  EXPECT_EQ(3, idle_test_source.notification_count());
}
