// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the public interface of the disk cache. For more details see
// http://dev.chromium.org/developers/design-documents/network-stack/disk-cache

#ifndef NET_DISK_CACHE_DISK_CACHE_H_
#define NET_DISK_CACHE_DISK_CACHE_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/time.h"
#include "net/base/cache_type.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"

class FilePath;

namespace base {
class MessageLoopProxy;
}

namespace net {
class IOBuffer;
class NetLog;
}

namespace disk_cache {

class Entry;
class Backend;
typedef net::OldCompletionCallback OldCompletionCallback;

// Returns an instance of a Backend of the given |type|. |path| points to a
// folder where the cached data will be stored (if appropriate). This cache
// instance must be the only object that will be reading or writing files to
// that folder. The returned object should be deleted when not needed anymore.
// If |force| is true, and there is a problem with the cache initialization, the
// files will be deleted and a new set will be created. |max_bytes| is the
// maximum size the cache can grow to. If zero is passed in as |max_bytes|, the
// cache will determine the value to use. |thread| can be used to perform IO
// operations if a dedicated thread is required; a valid value is expected for
// any backend that performs operations on a disk. The returned pointer can be
// NULL if a fatal error is found. The actual return value of the function is a
// net error code. If this function returns ERR_IO_PENDING, the |callback| will
// be invoked when a backend is available or a fatal error condition is reached.
// The pointer to receive the |backend| must remain valid until the operation
// completes (the callback is notified).
NET_EXPORT int CreateCacheBackend(net::CacheType type, const FilePath& path,
                                  int max_bytes, bool force,
                                  base::MessageLoopProxy* thread,
                                  net::NetLog* net_log, Backend** backend,
                                  OldCompletionCallback* callback);

// The root interface for a disk cache instance.
class NET_EXPORT Backend {
 public:
  // If the backend is destroyed when there are operations in progress (any
  // callback that has not been invoked yet), this method cancels said
  // operations so the callbacks are not invoked, possibly leaving the work
  // half way (for instance, dooming just a few entries). Note that pending IO
  // for a given Entry (as opposed to the Backend) will still generate a
  // callback from within this method.
  virtual ~Backend() {}

  // Returns the number of entries in the cache.
  virtual int32 GetEntryCount() const = 0;

  // Opens an existing entry. Upon success, |entry| holds a pointer to an Entry
  // object representing the specified disk cache entry. When the entry pointer
  // is no longer needed, its Close method should be called. The return value is
  // a net error code. If this method returns ERR_IO_PENDING, the |callback|
  // will be invoked when the entry is available. The pointer to receive the
  // |entry| must remain valid until the operation completes.
  virtual int OpenEntry(const std::string& key, Entry** entry,
                        OldCompletionCallback* callback) = 0;

  // Creates a new entry. Upon success, the out param holds a pointer to an
  // Entry object representing the newly created disk cache entry. When the
  // entry pointer is no longer needed, its Close method should be called. The
  // return value is a net error code. If this method returns ERR_IO_PENDING,
  // the |callback| will be invoked when the entry is available. The pointer to
  // receive the |entry| must remain valid until the operation completes.
  virtual int CreateEntry(const std::string& key, Entry** entry,
                          OldCompletionCallback* callback) = 0;

  // Marks the entry, specified by the given key, for deletion. The return value
  // is a net error code. If this method returns ERR_IO_PENDING, the |callback|
  // will be invoked after the entry is doomed.
  virtual int DoomEntry(const std::string& key,
                        OldCompletionCallback* callback) = 0;

  // Marks all entries for deletion. The return value is a net error code. If
  // this method returns ERR_IO_PENDING, the |callback| will be invoked when the
  // operation completes.
  virtual int DoomAllEntries(OldCompletionCallback* callback) = 0;

  // Marks a range of entries for deletion. This supports unbounded deletes in
  // either direction by using null Time values for either argument. The return
  // value is a net error code. If this method returns ERR_IO_PENDING, the
  // |callback| will be invoked when the operation completes.
  virtual int DoomEntriesBetween(const base::Time initial_time,
                                 const base::Time end_time,
                                 OldCompletionCallback* callback) = 0;

  // Marks all entries accessed since |initial_time| for deletion. The return
  // value is a net error code. If this method returns ERR_IO_PENDING, the
  // |callback| will be invoked when the operation completes.
  virtual int DoomEntriesSince(const base::Time initial_time,
                               OldCompletionCallback* callback) = 0;

  // Enumerates the cache. Initialize |iter| to NULL before calling this method
  // the first time. That will cause the enumeration to start at the head of
  // the cache. For subsequent calls, pass the same |iter| pointer again without
  // changing its value. This method returns ERR_FAILED when there are no more
  // entries to enumerate. When the entry pointer is no longer needed, its
  // Close method should be called. The return value is a net error code. If
  // this method returns ERR_IO_PENDING, the |callback| will be invoked when the
  // |next_entry| is available. The pointer to receive the |next_entry| must
  // remain valid until the operation completes.
  //
  // NOTE: This method does not modify the last_used field of the entry, and
  // therefore it does not impact the eviction ranking of the entry.
  virtual int OpenNextEntry(void** iter, Entry** next_entry,
                            OldCompletionCallback* callback) = 0;

  // Releases iter without returning the next entry. Whenever OpenNextEntry()
  // returns true, but the caller is not interested in continuing the
  // enumeration by calling OpenNextEntry() again, the enumeration must be
  // ended by calling this method with iter returned by OpenNextEntry().
  virtual void EndEnumeration(void** iter) = 0;

  // Return a list of cache statistics.
  virtual void GetStats(
      std::vector<std::pair<std::string, std::string> >* stats) = 0;

  // Called whenever an external cache in the system reuses the resource
  // referred to by |key|.
  virtual void OnExternalCacheHit(const std::string& key) = 0;
};

// This interface represents an entry in the disk cache.
class NET_EXPORT Entry {
 public:
  // Marks this cache entry for deletion.
  virtual void Doom() = 0;

  // Releases this entry. Calling this method does not cancel pending IO
  // operations on this entry. Even after the last reference to this object has
  // been released, pending completion callbacks may be invoked.
  virtual void Close() = 0;

  // Returns the key associated with this cache entry.
  virtual std::string GetKey() const = 0;

  // Returns the time when this cache entry was last used.
  virtual base::Time GetLastUsed() const = 0;

  // Returns the time when this cache entry was last modified.
  virtual base::Time GetLastModified() const = 0;

  // Returns the size of the cache data with the given index.
  virtual int32 GetDataSize(int index) const = 0;

  // Copies cache data into the given buffer of length |buf_len|.  If
  // completion_callback is null, then this call blocks until the read
  // operation is complete.  Otherwise, completion_callback will be
  // called on the current thread once the read completes.  Returns the
  // number of bytes read or a network error code. If a completion callback is
  // provided then it will be called if this function returns ERR_IO_PENDING,
  // and a reference to |buf| will be retained until the callback is called.
  // Note that the callback will be invoked in any case, even after Close has
  // been called; in other words, the caller may close this entry without
  // having to wait for all the callbacks, and still rely on the cleanup
  // performed from the callback code.
  virtual int ReadData(int index, int offset, net::IOBuffer* buf, int buf_len,
                       OldCompletionCallback* completion_callback) = 0;

  // Copies cache data from the given buffer of length |buf_len|.  If
  // completion_callback is null, then this call blocks until the write
  // operation is complete.  Otherwise, completion_callback will be
  // called on the current thread once the write completes.  Returns the
  // number of bytes written or a network error code. If a completion callback
  // is provided then it will be called if this function returns ERR_IO_PENDING,
  // and a reference to |buf| will be retained until the callback is called.
  // Note that the callback will be invoked in any case, even after Close has
  // been called; in other words, the caller may close this entry without
  // having to wait for all the callbacks, and still rely on the cleanup
  // performed from the callback code.
  // If truncate is true, this call will truncate the stored data at the end of
  // what we are writing here.
  virtual int WriteData(int index, int offset, net::IOBuffer* buf, int buf_len,
                        OldCompletionCallback* completion_callback,
                        bool truncate) = 0;

  // Sparse entries support:
  //
  // A Backend implementation can support sparse entries, so the cache keeps
  // track of which parts of the entry have been written before. The backend
  // will never return data that was not written previously, so reading from
  // such region will return 0 bytes read (or actually the number of bytes read
  // before reaching that region).
  //
  // There are only two streams for sparse entries: a regular control stream
  // (index 0) that must be accessed through the regular API (ReadData and
  // WriteData), and one sparse stream that must me accessed through the sparse-
  // aware API that follows. Calling a non-sparse aware method with an index
  // argument other than 0 is a mistake that results in implementation specific
  // behavior. Using a sparse-aware method with an entry that was not stored
  // using the same API, or with a backend that doesn't support sparse entries
  // will return ERR_CACHE_OPERATION_NOT_SUPPORTED.
  //
  // The storage granularity of the implementation should be at least 1 KB. In
  // other words, storing less than 1 KB may result in an implementation
  // dropping the data completely, and writing at offsets not aligned with 1 KB,
  // or with lengths not a multiple of 1 KB may result in the first or last part
  // of the data being discarded. However, two consecutive writes should not
  // result in a hole in between the two parts as long as they are sequential
  // (the second one starts where the first one ended), and there is no other
  // write between them.
  //
  // The Backend implementation is free to evict any range from the cache at any
  // moment, so in practice, the previously stated granularity of 1 KB is not
  // as bad as it sounds.
  //
  // The sparse methods don't support multiple simultaneous IO operations to the
  // same physical entry, so in practice a single object should be instantiated
  // for a given key at any given time. Once an operation has been issued, the
  // caller should wait until it completes before starting another one. This
  // requirement includes the case when an entry is closed while some operation
  // is in progress and another object is instantiated; any IO operation will
  // fail while the previous operation is still in-flight. In order to deal with
  // this requirement, the caller could either wait until the operation
  // completes before closing the entry, or call CancelSparseIO() before closing
  // the entry, and call ReadyForSparseIO() on the new entry and wait for the
  // callback before issuing new operations.

  // Behaves like ReadData() except that this method is used to access sparse
  // entries.
  virtual int ReadSparseData(int64 offset, net::IOBuffer* buf, int buf_len,
                             OldCompletionCallback* completion_callback) = 0;

  // Behaves like WriteData() except that this method is used to access sparse
  // entries. |truncate| is not part of this interface because a sparse entry
  // is not expected to be reused with new data. To delete the old data and
  // start again, or to reduce the total size of the stream data (which implies
  // that the content has changed), the whole entry should be doomed and
  // re-created.
  virtual int WriteSparseData(int64 offset, net::IOBuffer* buf, int buf_len,
                              OldCompletionCallback* completion_callback) = 0;

  // Returns information about the currently stored portion of a sparse entry.
  // |offset| and |len| describe a particular range that should be scanned to
  // find out if it is stored or not. |start| will contain the offset of the
  // first byte that is stored within this range, and the return value is the
  // minimum number of consecutive stored bytes. Note that it is possible that
  // this entry has stored more than the returned value. This method returns a
  // net error code whenever the request cannot be completed successfully. If
  // this method returns ERR_IO_PENDING, the |callback| will be invoked when the
  // operation completes, and |start| must remain valid until that point.
  virtual int GetAvailableRange(int64 offset, int len, int64* start,
                                OldCompletionCallback* callback) = 0;

  // Returns true if this entry could be a sparse entry or false otherwise. This
  // is a quick test that may return true even if the entry is not really
  // sparse. This method doesn't modify the state of this entry (it will not
  // create sparse tracking data). GetAvailableRange or ReadSparseData can be
  // used to perfom a definitive test of wether an existing entry is sparse or
  // not, but that method may modify the current state of the entry (making it
  // sparse, for instance). The purpose of this method is to test an existing
  // entry, but without generating actual IO to perform a thorough check.
  virtual bool CouldBeSparse() const = 0;

  // Cancels any pending sparse IO operation (if any). The completion callback
  // of the operation in question will still be called when the operation
  // finishes, but the operation will finish sooner when this method is used.
  virtual void CancelSparseIO() = 0;

  // Returns OK if this entry can be used immediately. If that is not the
  // case, returns ERR_IO_PENDING and invokes the provided callback when this
  // entry is ready to use. This method always returns OK for non-sparse
  // entries, and returns ERR_IO_PENDING when a previous operation was cancelled
  // (by calling CancelSparseIO), but the cache is still busy with it. If there
  // is a pending operation that has not been cancelled, this method will return
  // OK although another IO operation cannot be issued at this time; in this
  // case the caller should just wait for the regular callback to be invoked
  // instead of using this method to provide another callback.
  //
  // Note that CancelSparseIO may have been called on another instance of this
  // object that refers to the same physical disk entry.
  // Note: This method is deprecated.
  virtual int ReadyForSparseIO(OldCompletionCallback* completion_callback) = 0;

 protected:
  virtual ~Entry() {}
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_DISK_CACHE_H_
