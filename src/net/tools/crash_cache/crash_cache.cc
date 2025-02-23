// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This command-line program generates the set of files needed for the crash-
// cache unit tests (DiskCacheTest,CacheBackend_Recover*). This program only
// works properly on debug mode, because the crash functionality is not compiled
// on release builds of the cache.

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/backend_impl.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/rankings.h"

using base::Time;

enum Errors {
  GENERIC = -1,
  ALL_GOOD = 0,
  INVALID_ARGUMENT = 1,
  CRASH_OVERWRITE,
  NOT_REACHED
};

using disk_cache::RankCrashes;

// Starts a new process, to generate the files.
int RunSlave(RankCrashes action) {
  FilePath exe;
  PathService::Get(base::FILE_EXE, &exe);

  CommandLine cmdline(exe);
  cmdline.AppendArg(base::IntToString(action));

  base::ProcessHandle handle;
  if (!base::LaunchProcess(cmdline, base::LaunchOptions(), &handle)) {
    printf("Unable to run test %d\n", action);
    return GENERIC;
  }

  int exit_code;

  if (!base::WaitForExitCode(handle, &exit_code)) {
    printf("Unable to get return code, test %d\n", action);
    return GENERIC;
  }
  if (ALL_GOOD != exit_code)
    printf("Test %d failed, code %d\n", action, exit_code);

  return exit_code;
}

// Main loop for the master process.
int MasterCode() {
  for (int i = disk_cache::NO_CRASH + 1; i < disk_cache::MAX_CRASH; i++) {
    int ret = RunSlave(static_cast<RankCrashes>(i));
    if (ALL_GOOD != ret)
      return ret;
  }

  return ALL_GOOD;
}

// -----------------------------------------------------------------------

namespace disk_cache {
NET_EXPORT_PRIVATE extern RankCrashes g_rankings_crash;
}

const char* kCrashEntryName = "the first key";

// Creates the destinaton folder for this run, and returns it on full_path.
bool CreateTargetFolder(const FilePath& path, RankCrashes action,
                        FilePath* full_path) {
  const char* folders[] = {
    "",
    "insert_empty1",
    "insert_empty2",
    "insert_empty3",
    "insert_one1",
    "insert_one2",
    "insert_one3",
    "insert_load1",
    "insert_load2",
    "remove_one1",
    "remove_one2",
    "remove_one3",
    "remove_one4",
    "remove_head1",
    "remove_head2",
    "remove_head3",
    "remove_head4",
    "remove_tail1",
    "remove_tail2",
    "remove_tail3",
    "remove_load1",
    "remove_load2",
    "remove_load3"
  };
  COMPILE_ASSERT(arraysize(folders) == disk_cache::MAX_CRASH, sync_folders);
  DCHECK(action > disk_cache::NO_CRASH && action < disk_cache::MAX_CRASH);

  *full_path = path.AppendASCII(folders[action]);

  if (file_util::PathExists(*full_path))
    return false;

  return file_util::CreateDirectory(*full_path);
}

// Makes sure that any pending task is processed.
void FlushQueue(disk_cache::Backend* cache) {
  TestOldCompletionCallback cb;
  int rv =
      reinterpret_cast<disk_cache::BackendImpl*>(cache)->FlushQueueForTest(&cb);
  cb.GetResult(rv);  // Ignore the result;
}

// Generates the files for an empty and one item cache.
int SimpleInsert(const FilePath& path, RankCrashes action,
                 base::Thread* cache_thread) {
  TestOldCompletionCallback cb;
  disk_cache::Backend* cache;
  int rv = disk_cache::CreateCacheBackend(net::DISK_CACHE, path, 0, false,
                                          cache_thread->message_loop_proxy(),
                                          NULL, &cache, &cb);
  if (cb.GetResult(rv) != net::OK || cache->GetEntryCount())
    return GENERIC;

  const char* test_name = "some other key";

  if (action <= disk_cache::INSERT_EMPTY_3) {
    test_name = kCrashEntryName;
    disk_cache::g_rankings_crash = action;
  }

  disk_cache::Entry* entry;
  rv = cache->CreateEntry(test_name, &entry, &cb);
  if (cb.GetResult(rv) != net::OK)
    return GENERIC;

  entry->Close();
  FlushQueue(cache);

  DCHECK(action <= disk_cache::INSERT_ONE_3);
  disk_cache::g_rankings_crash = action;
  test_name = kCrashEntryName;

  rv = cache->CreateEntry(test_name, &entry, &cb);
  if (cb.GetResult(rv) != net::OK)
    return GENERIC;

  return NOT_REACHED;
}

// Generates the files for a one item cache, and removing the head.
int SimpleRemove(const FilePath& path, RankCrashes action,
                 base::Thread* cache_thread) {
  DCHECK(action >= disk_cache::REMOVE_ONE_1);
  DCHECK(action <= disk_cache::REMOVE_TAIL_3);

  TestOldCompletionCallback cb;
  disk_cache::Backend* cache;
  // Use a simple LRU for eviction.
  int rv = disk_cache::CreateCacheBackend(net::MEDIA_CACHE, path, 0, false,
                                          cache_thread->message_loop_proxy(),
                                          NULL, &cache, &cb);
  if (cb.GetResult(rv) != net::OK || cache->GetEntryCount())
    return GENERIC;

  disk_cache::Entry* entry;
  rv = cache->CreateEntry(kCrashEntryName, &entry, &cb);
  if (cb.GetResult(rv) != net::OK)
    return GENERIC;

  entry->Close();
  FlushQueue(cache);

  if (action >= disk_cache::REMOVE_TAIL_1) {
    rv = cache->CreateEntry("some other key", &entry, &cb);
    if (cb.GetResult(rv) != net::OK)
      return GENERIC;

    entry->Close();
    FlushQueue(cache);
  }

  rv = cache->OpenEntry(kCrashEntryName, &entry, &cb);
  if (cb.GetResult(rv) != net::OK)
    return GENERIC;

  disk_cache::g_rankings_crash = action;
  entry->Doom();
  entry->Close();
  FlushQueue(cache);

  return NOT_REACHED;
}

int HeadRemove(const FilePath& path, RankCrashes action,
               base::Thread* cache_thread) {
  DCHECK(action >= disk_cache::REMOVE_HEAD_1);
  DCHECK(action <= disk_cache::REMOVE_HEAD_4);

  TestOldCompletionCallback cb;
  disk_cache::Backend* cache;
  // Use a simple LRU for eviction.
  int rv = disk_cache::CreateCacheBackend(net::MEDIA_CACHE, path, 0, false,
                                          cache_thread->message_loop_proxy(),
                                          NULL, &cache, &cb);
  if (cb.GetResult(rv) != net::OK || cache->GetEntryCount())
    return GENERIC;

  disk_cache::Entry* entry;
  rv = cache->CreateEntry("some other key", &entry, &cb);
  if (cb.GetResult(rv) != net::OK)
    return GENERIC;

  entry->Close();
  FlushQueue(cache);
  rv = cache->CreateEntry(kCrashEntryName, &entry, &cb);
  if (cb.GetResult(rv) != net::OK)
    return GENERIC;

  entry->Close();
  FlushQueue(cache);

  rv = cache->OpenEntry(kCrashEntryName, &entry, &cb);
  if (cb.GetResult(rv) != net::OK)
    return GENERIC;

  disk_cache::g_rankings_crash = action;
  entry->Doom();
  entry->Close();
  FlushQueue(cache);

  return NOT_REACHED;
}

// Generates the files for insertion and removals on heavy loaded caches.
int LoadOperations(const FilePath& path, RankCrashes action,
                   base::Thread* cache_thread) {
  DCHECK(action >= disk_cache::INSERT_LOAD_1);

  // Work with a tiny index table (16 entries).
  disk_cache::BackendImpl* cache = new disk_cache::BackendImpl(
      path, 0xf, cache_thread->message_loop_proxy(), NULL);
  if (!cache || !cache->SetMaxSize(0x100000))
    return GENERIC;

  TestOldCompletionCallback cb;
  int rv = cache->Init(&cb);
  if (cb.GetResult(rv) != net::OK || cache->GetEntryCount())
    return GENERIC;

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  disk_cache::Entry* entry;
  for (int i = 0; i < 100; i++) {
    std::string key = GenerateKey(true);
    rv = cache->CreateEntry(key, &entry, &cb);
    if (cb.GetResult(rv) != net::OK)
      return GENERIC;
    entry->Close();
    FlushQueue(cache);
    if (50 == i && action >= disk_cache::REMOVE_LOAD_1) {
      rv = cache->CreateEntry(kCrashEntryName, &entry, &cb);
      if (cb.GetResult(rv) != net::OK)
        return GENERIC;
      entry->Close();
      FlushQueue(cache);
    }
  }

  if (action <= disk_cache::INSERT_LOAD_2) {
    disk_cache::g_rankings_crash = action;

    rv = cache->CreateEntry(kCrashEntryName, &entry, &cb);
    if (cb.GetResult(rv) != net::OK)
      return GENERIC;
  }

  rv = cache->OpenEntry(kCrashEntryName, &entry, &cb);
  if (cb.GetResult(rv) != net::OK)
    return GENERIC;

  disk_cache::g_rankings_crash = action;

  entry->Doom();
  entry->Close();
  FlushQueue(cache);

  return NOT_REACHED;
}

// Main function on the child process.
int SlaveCode(const FilePath& path, RankCrashes action) {
  MessageLoopForIO message_loop;

  FilePath full_path;
  if (!CreateTargetFolder(path, action, &full_path)) {
    printf("Destination folder found, please remove it.\n");
    return CRASH_OVERWRITE;
  }

  base::Thread cache_thread("CacheThread");
  if (!cache_thread.StartWithOptions(
          base::Thread::Options(MessageLoop::TYPE_IO, 0)))
    return GENERIC;

  if (action <= disk_cache::INSERT_ONE_3)
    return SimpleInsert(full_path, action, &cache_thread);

  if (action <= disk_cache::INSERT_LOAD_2)
    return LoadOperations(full_path, action, &cache_thread);

  if (action <= disk_cache::REMOVE_ONE_4)
    return SimpleRemove(full_path, action, &cache_thread);

  if (action <= disk_cache::REMOVE_HEAD_4)
    return HeadRemove(full_path, action, &cache_thread);

  if (action <= disk_cache::REMOVE_TAIL_3)
    return SimpleRemove(full_path, action, &cache_thread);

  if (action <= disk_cache::REMOVE_LOAD_3)
    return LoadOperations(full_path, action, &cache_thread);

  return NOT_REACHED;
}

// -----------------------------------------------------------------------

int main(int argc, const char* argv[]) {
  // Setup an AtExitManager so Singleton objects will be destructed.
  base::AtExitManager at_exit_manager;

  if (argc < 2)
    return MasterCode();

  char* end;
  RankCrashes action = static_cast<RankCrashes>(strtol(argv[1], &end, 0));
  if (action <= disk_cache::NO_CRASH || action >= disk_cache::MAX_CRASH) {
    printf("Invalid action\n");
    return INVALID_ARGUMENT;
  }

  FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("net");
  path = path.AppendASCII("data");
  path = path.AppendASCII("cache_tests");
  path = path.AppendASCII("new_crashes");

  return SlaveCode(path, action);
}
