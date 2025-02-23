// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/perftimer.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/test/test_file_util.h"
#include "base/timer.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/block_files.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/hash.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::Time;

typedef PlatformTest DiskCacheTest;

namespace {

struct TestEntry {
  std::string key;
  int data_len;
};
typedef std::vector<TestEntry> TestEntries;

const int kMaxSize = 16 * 1024 - 1;

// Creates num_entries on the cache, and writes 200 bytes of metadata and up
// to kMaxSize of data to each entry.
bool TimeWrite(int num_entries, disk_cache::Backend* cache,
              TestEntries* entries) {
  const int kSize1 = 200;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize1));
  scoped_refptr<net::IOBuffer> buffer2(new net::IOBuffer(kMaxSize));

  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  CacheTestFillBuffer(buffer2->data(), kMaxSize, false);

  int expected = 0;

  MessageLoopHelper helper;
  CallbackTest callback(&helper, true);

  PerfTimeLogger timer("Write disk cache entries");

  for (int i = 0; i < num_entries; i++) {
    TestEntry entry;
    entry.key = GenerateKey(true);
    entry.data_len = rand() % kMaxSize;
    entries->push_back(entry);

    disk_cache::Entry* cache_entry;
    TestOldCompletionCallback cb;
    int rv = cache->CreateEntry(entry.key, &cache_entry, &cb);
    if (net::OK != cb.GetResult(rv))
      break;
    int ret = cache_entry->WriteData(0, 0, buffer1, kSize1, &callback, false);
    if (net::ERR_IO_PENDING == ret)
      expected++;
    else if (kSize1 != ret)
      break;

    ret = cache_entry->WriteData(1, 0, buffer2, entry.data_len, &callback,
                                 false);
    if (net::ERR_IO_PENDING == ret)
      expected++;
    else if (entry.data_len != ret)
      break;
    cache_entry->Close();
  }

  helper.WaitUntilCacheIoFinished(expected);
  timer.Done();

  return (expected == helper.callbacks_called());
}

// Reads the data and metadata from each entry listed on |entries|.
bool TimeRead(int num_entries, disk_cache::Backend* cache,
             const TestEntries& entries, bool cold) {
  const int kSize1 = 200;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize1));
  scoped_refptr<net::IOBuffer> buffer2(new net::IOBuffer(kMaxSize));

  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  CacheTestFillBuffer(buffer2->data(), kMaxSize, false);

  int expected = 0;

  MessageLoopHelper helper;
  CallbackTest callback(&helper, true);

  const char* message = cold ? "Read disk cache entries (cold)" :
                        "Read disk cache entries (warm)";
  PerfTimeLogger timer(message);

  for (int i = 0; i < num_entries; i++) {
    disk_cache::Entry* cache_entry;
    TestOldCompletionCallback cb;
    int rv = cache->OpenEntry(entries[i].key, &cache_entry, &cb);
    if (net::OK != cb.GetResult(rv))
      break;
    int ret = cache_entry->ReadData(0, 0, buffer1, kSize1, &callback);
    if (net::ERR_IO_PENDING == ret)
      expected++;
    else if (kSize1 != ret)
      break;

    ret = cache_entry->ReadData(1, 0, buffer2, entries[i].data_len, &callback);
    if (net::ERR_IO_PENDING == ret)
      expected++;
    else if (entries[i].data_len != ret)
      break;
    cache_entry->Close();
  }

  helper.WaitUntilCacheIoFinished(expected);
  timer.Done();

  return (expected == helper.callbacks_called());
}

int BlockSize() {
  // We can use form 1 to 4 blocks.
  return (rand() & 0x3) + 1;
}

}  // namespace

TEST_F(DiskCacheTest, Hash) {
  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  PerfTimeLogger timer("Hash disk cache keys");
  for (int i = 0; i < 300000; i++) {
    std::string key = GenerateKey(true);
    disk_cache::Hash(key);
  }
  timer.Done();
}

TEST_F(DiskCacheTest, CacheBackendPerformance) {
  MessageLoopForIO message_loop;

  base::Thread cache_thread("CacheThread");
  ASSERT_TRUE(cache_thread.StartWithOptions(
                  base::Thread::Options(MessageLoop::TYPE_IO, 0)));

  ScopedTestCache test_cache;
  TestOldCompletionCallback cb;
  disk_cache::Backend* cache;
  int rv = disk_cache::CreateCacheBackend(
               net::DISK_CACHE, test_cache.path(), 0, false,
               cache_thread.message_loop_proxy(), NULL, &cache, &cb);

  ASSERT_EQ(net::OK, cb.GetResult(rv));

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  TestEntries entries;
  int num_entries = 1000;

  EXPECT_TRUE(TimeWrite(num_entries, cache, &entries));

  MessageLoop::current()->RunAllPending();
  delete cache;

  ASSERT_TRUE(file_util::EvictFileFromSystemCache(
              test_cache.path().AppendASCII("index")));
  ASSERT_TRUE(file_util::EvictFileFromSystemCache(
              test_cache.path().AppendASCII("data_0")));
  ASSERT_TRUE(file_util::EvictFileFromSystemCache(
              test_cache.path().AppendASCII("data_1")));
  ASSERT_TRUE(file_util::EvictFileFromSystemCache(
              test_cache.path().AppendASCII("data_2")));
  ASSERT_TRUE(file_util::EvictFileFromSystemCache(
              test_cache.path().AppendASCII("data_3")));

  rv = disk_cache::CreateCacheBackend(net::DISK_CACHE, test_cache.path(), 0,
                                      false, cache_thread.message_loop_proxy(),
                                      NULL, &cache, &cb);
  ASSERT_EQ(net::OK, cb.GetResult(rv));

  EXPECT_TRUE(TimeRead(num_entries, cache, entries, true));

  EXPECT_TRUE(TimeRead(num_entries, cache, entries, false));

  MessageLoop::current()->RunAllPending();
  delete cache;
}

// Creating and deleting "entries" on a block-file is something quite frequent
// (after all, almost everything is stored on block files). The operation is
// almost free when the file is empty, but can be expensive if the file gets
// fragmented, or if we have multiple files. This test measures that scenario,
// by using multiple, highly fragmented files.
TEST_F(DiskCacheTest, BlockFilesPerformance) {
  MessageLoopForIO message_loop;

  ScopedTestCache test_cache;

  disk_cache::BlockFiles files(test_cache.path());
  ASSERT_TRUE(files.Init(true));

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  const int kNumEntries = 60000;
  disk_cache::Addr* address = new disk_cache::Addr[kNumEntries];

  PerfTimeLogger timer1("Fill three block-files");

  // Fill up the 32-byte block file (use three files).
  for (int i = 0; i < kNumEntries; i++) {
    EXPECT_TRUE(files.CreateBlock(disk_cache::RANKINGS, BlockSize(),
                                  &address[i]));
  }

  timer1.Done();
  PerfTimeLogger timer2("Create and delete blocks");

  for (int i = 0; i < 200000; i++) {
    int entry = rand() * (kNumEntries / RAND_MAX + 1);
    if (entry >= kNumEntries)
      entry = 0;

    files.DeleteBlock(address[entry], false);
    EXPECT_TRUE(files.CreateBlock(disk_cache::RANKINGS, BlockSize(),
                                  &address[entry]));
  }

  timer2.Done();
  MessageLoop::current()->RunAllPending();
  delete[] address;
}
