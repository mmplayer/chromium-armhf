// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_ENTRY_IMPL_H_
#define NET_DISK_CACHE_ENTRY_IMPL_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "net/base/net_log.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/storage_block.h"
#include "net/disk_cache/storage_block-inl.h"

namespace disk_cache {

class BackendImpl;
class SparseControl;

// This class implements the Entry interface. An object of this
// class represents a single entry on the cache.
class NET_EXPORT_PRIVATE EntryImpl
    : public Entry,
      public base::RefCounted<EntryImpl> {
  friend class base::RefCounted<EntryImpl>;
  friend class SparseControl;
 public:
  enum Operation {
    kRead,
    kWrite,
    kSparseRead,
    kSparseWrite,
    kAsyncIO
  };

  EntryImpl(BackendImpl* backend, Addr address, bool read_only);

  // Background implementation of the Entry interface.
  void DoomImpl();
  int ReadDataImpl(int index, int offset, net::IOBuffer* buf, int buf_len,
                   OldCompletionCallback* callback);
  int WriteDataImpl(int index, int offset, net::IOBuffer* buf, int buf_len,
                    OldCompletionCallback* callback, bool truncate);
  int ReadSparseDataImpl(int64 offset, net::IOBuffer* buf, int buf_len,
                         OldCompletionCallback* callback);
  int WriteSparseDataImpl(int64 offset, net::IOBuffer* buf, int buf_len,
                          OldCompletionCallback* callback);
  int GetAvailableRangeImpl(int64 offset, int len, int64* start);
  void CancelSparseIOImpl();
  int ReadyForSparseIOImpl(OldCompletionCallback* callback);

  inline CacheEntryBlock* entry() {
    return &entry_;
  }

  inline CacheRankingsBlock* rankings() {
    return &node_;
  }

  uint32 GetHash();

  // Performs the initialization of a EntryImpl that will be added to the
  // cache.
  bool CreateEntry(Addr node_address, const std::string& key, uint32 hash);

  // Returns true if this entry matches the lookup arguments.
  bool IsSameEntry(const std::string& key, uint32 hash);

  // Permamently destroys this entry.
  void InternalDoom();

  // Deletes this entry from disk. If |everything| is false, only the user data
  // will be removed, leaving the key and control data intact.
  void DeleteEntryData(bool everything);

  // Returns the address of the next entry on the list of entries with the same
  // hash.
  CacheAddr GetNextAddress();

  // Sets the address of the next entry on the list of entries with the same
  // hash.
  void SetNextAddress(Addr address);

  // Reloads the rankings node information.
  bool LoadNodeAddress();

  // Updates the stored data to reflect the run-time information for this entry.
  // Returns false if the data could not be updated. The purpose of this method
  // is to be able to detect entries that are currently in use.
  bool Update();

  bool dirty() {
    return dirty_;
  }

  bool doomed() {
    return doomed_;
  }

  // Marks this entry as dirty (in memory) if needed. This is intended only for
  // entries that are being read from disk, to be called during loading.
  void SetDirtyFlag(int32 current_id);

  // Fixes this entry so it can be treated as valid (to delete it).
  void SetPointerForInvalidEntry(int32 new_id);

  // Returns true if this entry is so meesed up that not everything is going to
  // be removed.
  bool LeaveRankingsBehind();

  // Returns false if the entry is clearly invalid.
  bool SanityCheck();
  bool DataSanityCheck();

  // Attempts to make this entry reachable though the key.
  void FixForDelete();

  // Handle the pending asynchronous IO count.
  void IncrementIoCount();
  void DecrementIoCount();

  // Set the access times for this entry. This method provides support for
  // the upgrade tool.
  void SetTimes(base::Time last_used, base::Time last_modified);

  // Generates a histogram for the time spent working on this operation.
  void ReportIOTime(Operation op, const base::TimeTicks& start);

  // Logs a begin event and enables logging for the EntryImpl.  Will also cause
  // an end event to be logged on destruction.  The EntryImpl must have its key
  // initialized before this is called.  |created| is true if the Entry was
  // created rather than opened.
  void BeginLogging(net::NetLog* net_log, bool created);

  const net::BoundNetLog& net_log() const;

  // Returns the number of blocks needed to store an EntryStore.
  static int NumBlocksForEntry(int key_size);

  // Entry interface.
  virtual void Doom();
  virtual void Close();
  virtual std::string GetKey() const;
  virtual base::Time GetLastUsed() const;
  virtual base::Time GetLastModified() const;
  virtual int32 GetDataSize(int index) const;
  virtual int ReadData(int index, int offset, net::IOBuffer* buf, int buf_len,
                       net::OldCompletionCallback* completion_callback);
  virtual int WriteData(int index, int offset, net::IOBuffer* buf, int buf_len,
                        net::OldCompletionCallback* completion_callback,
                        bool truncate);
  virtual int ReadSparseData(int64 offset, net::IOBuffer* buf, int buf_len,
                             net::OldCompletionCallback* completion_callback);
  virtual int WriteSparseData(int64 offset, net::IOBuffer* buf, int buf_len,
                              net::OldCompletionCallback* completion_callback);
  virtual int GetAvailableRange(int64 offset, int len, int64* start,
                                OldCompletionCallback* callback);
  virtual bool CouldBeSparse() const;
  virtual void CancelSparseIO();
  virtual int ReadyForSparseIO(net::OldCompletionCallback* completion_callback);

 private:
  enum {
     kNumStreams = 3
  };
  class UserBuffer;

  virtual ~EntryImpl();

  // Do all the work for ReadDataImpl and WriteDataImpl.  Implemented as
  // separate functions to make logging of results simpler.
  int InternalReadData(int index, int offset, net::IOBuffer* buf,
                       int buf_len, OldCompletionCallback* callback);
  int InternalWriteData(int index, int offset, net::IOBuffer* buf, int buf_len,
                        OldCompletionCallback* callback, bool truncate);

  // Initializes the storage for an internal or external data block.
  bool CreateDataBlock(int index, int size);

  // Initializes the storage for an internal or external generic block.
  bool CreateBlock(int size, Addr* address);

  // Deletes the data pointed by address, maybe backed by files_[index].
  // Note that most likely the caller should delete (and store) the reference to
  // |address| *before* calling this method because we don't want to have an
  // entry using an address that is already free.
  void DeleteData(Addr address, int index);

  // Updates ranking information.
  void UpdateRank(bool modified);

  // Returns a pointer to the file that stores the given address.
  File* GetBackingFile(Addr address, int index);

  // Returns a pointer to the file that stores external data.
  File* GetExternalFile(Addr address, int index);

  // Prepares the target file or buffer for a write of buf_len bytes at the
  // given offset.
  bool PrepareTarget(int index, int offset, int buf_len, bool truncate);

  // Adjusts the internal buffer and file handle for a write that truncates this
  // stream.
  bool HandleTruncation(int index, int offset, int buf_len);

  // Copies data from disk to the internal buffer.
  bool CopyToLocalBuffer(int index);

  // Reads from a block data file to this object's memory buffer.
  bool MoveToLocalBuffer(int index);

  // Loads the external file to this object's memory buffer.
  bool ImportSeparateFile(int index, int new_size);

  // Makes sure that the internal buffer can handle the a write of |buf_len|
  // bytes to |offset|.
  bool PrepareBuffer(int index, int offset, int buf_len);

  // Flushes the in-memory data to the backing storage. The data destination
  // is determined based on the current data length and |min_len|.
  bool Flush(int index, int min_len);

  // Updates the size of a given data stream.
  void UpdateSize(int index, int old_size, int new_size);

  // Initializes the sparse control object. Returns a net error code.
  int InitSparseData();

  // Adds the provided |flags| to the current EntryFlags for this entry.
  void SetEntryFlags(uint32 flags);

  // Returns the current EntryFlags for this entry.
  uint32 GetEntryFlags();

  // Gets the data stored at the given index. If the information is in memory,
  // a buffer will be allocated and the data will be copied to it (the caller
  // can find out the size of the buffer before making this call). Otherwise,
  // the cache address of the data will be returned, and that address will be
  // removed from the regular book keeping of this entry so the caller is
  // responsible for deleting the block (or file) from the backing store at some
  // point; there is no need to report any storage-size change, only to do the
  // actual cleanup.
  void GetData(int index, char** buffer, Addr* address);

  // Logs this entry to the internal trace buffer.
  void Log(const char* msg);

  CacheEntryBlock entry_;     // Key related information for this entry.
  CacheRankingsBlock node_;   // Rankings related information for this entry.
  BackendImpl* backend_;      // Back pointer to the cache.
  scoped_ptr<UserBuffer> user_buffers_[kNumStreams];  // Stores user data.
  // Files to store external user data and key.
  scoped_refptr<File> files_[kNumStreams + 1];
  mutable std::string key_;           // Copy of the key.
  int unreported_size_[kNumStreams];  // Bytes not reported yet to the backend.
  bool doomed_;               // True if this entry was removed from the cache.
  bool read_only_;            // True if not yet writing.
  bool dirty_;                // True if we detected that this is a dirty entry.
  scoped_ptr<SparseControl> sparse_;  // Support for sparse entries.

  net::BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(EntryImpl);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_ENTRY_IMPL_H_
