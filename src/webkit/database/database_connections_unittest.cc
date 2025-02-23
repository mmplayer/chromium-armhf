// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/database/database_connections.h"

namespace webkit_database {

namespace {

void RemoveConnectionTask(
    const string16& origin_id, const string16& database_name,
    scoped_refptr<DatabaseConnectionsWrapper> obj,
    bool* did_task_execute) {
  *did_task_execute = true;
  obj->RemoveOpenConnection(origin_id, database_name);
}

void ScheduleRemoveConnectionTask(
    base::Thread* thread,  const string16& origin_id,
    const string16& database_name,
    scoped_refptr<DatabaseConnectionsWrapper> obj,
    bool* did_task_execute) {
  thread->message_loop()->PostTask(FROM_HERE,
      NewRunnableFunction(RemoveConnectionTask,
                          origin_id, database_name, obj, did_task_execute));
}

}  // anonymous namespace

TEST(DatabaseConnectionsTest, DatabaseConnectionsTest) {
  const string16 kOriginId(ASCIIToUTF16("origin_id"));
  const string16 kName(ASCIIToUTF16("database_name"));
  const string16 kName2(ASCIIToUTF16("database_name2"));
  const int64 kSize = 1000;

  DatabaseConnections connections;

  EXPECT_TRUE(connections.IsEmpty());
  EXPECT_FALSE(connections.IsDatabaseOpened(kOriginId, kName));
  EXPECT_FALSE(connections.IsOriginUsed(kOriginId));

  connections.AddConnection(kOriginId, kName);
  EXPECT_FALSE(connections.IsEmpty());
  EXPECT_TRUE(connections.IsDatabaseOpened(kOriginId, kName));
  EXPECT_TRUE(connections.IsOriginUsed(kOriginId));
  EXPECT_EQ(0, connections.GetOpenDatabaseSize(kOriginId, kName));
  connections.SetOpenDatabaseSize(kOriginId, kName, kSize);
  EXPECT_EQ(kSize, connections.GetOpenDatabaseSize(kOriginId, kName));

  connections.RemoveConnection(kOriginId, kName);
  EXPECT_TRUE(connections.IsEmpty());
  EXPECT_FALSE(connections.IsDatabaseOpened(kOriginId, kName));
  EXPECT_FALSE(connections.IsOriginUsed(kOriginId));

  connections.AddConnection(kOriginId, kName);
  connections.SetOpenDatabaseSize(kOriginId, kName, kSize);
  EXPECT_EQ(kSize, connections.GetOpenDatabaseSize(kOriginId, kName));
  connections.AddConnection(kOriginId, kName);
  EXPECT_EQ(kSize, connections.GetOpenDatabaseSize(kOriginId, kName));
  EXPECT_FALSE(connections.IsEmpty());
  EXPECT_TRUE(connections.IsDatabaseOpened(kOriginId, kName));
  EXPECT_TRUE(connections.IsOriginUsed(kOriginId));
  connections.AddConnection(kOriginId, kName2);
  EXPECT_TRUE(connections.IsDatabaseOpened(kOriginId, kName2));

  DatabaseConnections another;
  another.AddConnection(kOriginId, kName);
  another.AddConnection(kOriginId, kName2);

  std::vector<std::pair<string16, string16> > closed_dbs;
  connections.RemoveConnections(another, &closed_dbs);
  EXPECT_EQ(1u, closed_dbs.size());
  EXPECT_EQ(kOriginId, closed_dbs[0].first);
  EXPECT_EQ(kName2, closed_dbs[0].second);
  EXPECT_FALSE(connections.IsDatabaseOpened(kOriginId, kName2));
  EXPECT_TRUE(connections.IsDatabaseOpened(kOriginId, kName));
  EXPECT_EQ(kSize, connections.GetOpenDatabaseSize(kOriginId, kName));
  another.RemoveAllConnections();
  connections.RemoveAllConnections();
  EXPECT_TRUE(connections.IsEmpty());

  // Ensure the return value properly indicates the initial
  // addition and final removal.
  EXPECT_TRUE(connections.AddConnection(kOriginId, kName));
  EXPECT_FALSE(connections.AddConnection(kOriginId, kName));
  EXPECT_FALSE(connections.AddConnection(kOriginId, kName));
  EXPECT_FALSE(connections.RemoveConnection(kOriginId, kName));
  EXPECT_FALSE(connections.RemoveConnection(kOriginId, kName));
  EXPECT_TRUE(connections.RemoveConnection(kOriginId, kName));
}

TEST(DatabaseConnectionsTest, DatabaseConnectionsWrapperTest) {
  const string16 kOriginId(ASCIIToUTF16("origin_id"));
  const string16 kName(ASCIIToUTF16("database_name"));

  scoped_refptr<DatabaseConnectionsWrapper> obj(
      new DatabaseConnectionsWrapper);
  EXPECT_FALSE(obj->HasOpenConnections());
  obj->AddOpenConnection(kOriginId, kName);
  EXPECT_TRUE(obj->HasOpenConnections());
  obj->AddOpenConnection(kOriginId, kName);
  EXPECT_TRUE(obj->HasOpenConnections());
  obj->RemoveOpenConnection(kOriginId, kName);
  EXPECT_TRUE(obj->HasOpenConnections());
  obj->RemoveOpenConnection(kOriginId, kName);
  EXPECT_FALSE(obj->HasOpenConnections());
  obj->WaitForAllDatabasesToClose();  // should return immediately

  // Test WaitForAllDatabasesToClose with the last connection
  // being removed on the current thread.
  obj->AddOpenConnection(kOriginId, kName);
  bool did_task_execute = false;
  MessageLoop::current()->PostTask(FROM_HERE,
      NewRunnableFunction(&RemoveConnectionTask,
                          kOriginId, kName, obj, &did_task_execute));
  obj->WaitForAllDatabasesToClose();  // should return after the task executes
  EXPECT_TRUE(did_task_execute);
  EXPECT_FALSE(obj->HasOpenConnections());

  // Test WaitForAllDatabasesToClose with the last connection
  // being removed on another thread.
  obj->AddOpenConnection(kOriginId, kName);
  base::Thread thread("WrapperTestThread");
  thread.Start();
  did_task_execute = false;
  MessageLoop::current()->PostTask(FROM_HERE,
      NewRunnableFunction(ScheduleRemoveConnectionTask,
                          &thread, kOriginId, kName,
                          obj, &did_task_execute));
  obj->WaitForAllDatabasesToClose();  // should return after the task executes
  EXPECT_TRUE(did_task_execute);
  EXPECT_FALSE(obj->HasOpenConnections());
}

}  // namespace webkit_database
