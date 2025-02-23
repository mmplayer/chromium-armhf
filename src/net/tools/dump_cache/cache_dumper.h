// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_DUMP_CACHE_CACHE_DUMPER_H_
#define NET_TOOLS_DUMP_CACHE_CACHE_DUMPER_H_
#pragma once

#include <string>
#include "base/file_path.h"
#include "base/file_util.h"
#include "net/disk_cache/backend_impl.h"

#ifdef WIN32
// Dumping the cache often creates very large filenames, which are tricky
// on windows.  Most API calls don't support large filenames, including
// most of the base library functions.  Unfortunately, adding "\\?\" into
// the filename support is tricky.  Instead, if WIN32_LARGE_FILENAME_SUPPORT
// is set, we use direct WIN32 APIs for manipulating the files.
#define WIN32_LARGE_FILENAME_SUPPORT
#endif

// An abstract class for writing cache dump data.
class CacheDumpWriter {
 public:
  virtual ~CacheDumpWriter() {}

  // Creates an entry to be written.
  // On success, populates the |entry|.
  // Returns a net error code.
  virtual int CreateEntry(const std::string& key,
                          disk_cache::Entry** entry,
                          net::OldCompletionCallback* callback) = 0;

  // Write to the current entry.
  // Returns a net error code.
  virtual int WriteEntry(disk_cache::Entry* entry, int stream, int offset,
                         net::IOBuffer* buf, int buf_len,
                         net::OldCompletionCallback* callback) = 0;

  // Close the current entry.
  virtual void CloseEntry(disk_cache::Entry* entry, base::Time last_used,
                          base::Time last_modified) = 0;
};

// Writes data to a cache.
class CacheDumper : public CacheDumpWriter {
 public:
  explicit CacheDumper(disk_cache::Backend* cache) : cache_(cache) {}

  virtual int CreateEntry(const std::string& key, disk_cache::Entry** entry,
                          net::OldCompletionCallback* callback);
  virtual int WriteEntry(disk_cache::Entry* entry, int stream, int offset,
                         net::IOBuffer* buf, int buf_len,
                         net::OldCompletionCallback* callback);
  virtual void CloseEntry(disk_cache::Entry* entry, base::Time last_used,
                          base::Time last_modified);

 private:
  disk_cache::Backend* cache_;
};

// Writes data to a disk.
class DiskDumper : public CacheDumpWriter {
 public:
  explicit DiskDumper(const std::wstring& path) : path_(path), entry_(NULL) {
    file_util::CreateDirectory(FilePath(path));
  }
  virtual int CreateEntry(const std::string& key, disk_cache::Entry** entry,
                          net::OldCompletionCallback* callback);
  virtual int WriteEntry(disk_cache::Entry* entry, int stream, int offset,
                         net::IOBuffer* buf, int buf_len,
                         net::OldCompletionCallback* callback);
  virtual void CloseEntry(disk_cache::Entry* entry, base::Time last_used,
                          base::Time last_modified);

 private:
  std::wstring path_;
  // This is a bit of a hack.  As we get a CreateEntry, we coin the current
  // entry_path_ where we write that entry to disk.  Subsequent calls to
  // WriteEntry() utilize this path for writing to disk.
  FilePath entry_path_;
  std::string entry_url_;
#ifdef WIN32_LARGE_FILENAME_SUPPORT
  HANDLE entry_;
#else
  FILE* entry_;
#endif
};

#endif  // NET_TOOLS_DUMP_CACHE_CACHE_DUMPER_H_
