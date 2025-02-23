// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_FILE_SYSTEM_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_FILE_SYSTEM_HELPER_H_
#pragma once

#include <list>
#include <string>

#include "base/callback_old.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_types.h"

class Profile;

// Defines an interface for classes that deal with aggregating and deleting
// browsing data stored in an origin's file systems.
// BrowsingDataFileSystemHelper instances for a specific profile should be
// created via the static Create method. Each instance will lazily fetch file
// system data when a client calls StartFetching from the UI thread, and will
// notify the client via a supplied callback when the data is available.
// Only one StartFetching task can run at a time: executing StartFetching while
// another StartFetching task is running will DCHECK. If the client must abort
// the process before completion (it's destroyed, for example) then it must call
// CancelNotification.
//
// The client's callback is passed a list of FileSystemInfo objects containing
// usage information for each origin's temporary and persistent file systems.
//
// Clients may remove an origin's file systems at any time (even before fetching
// data) by calling DeleteFileSystemOrigin() on the UI thread. Calling
// DeleteFileSystemOrigin() for an origin that doesn't have any is safe; it's
// just an expensive NOOP.
class BrowsingDataFileSystemHelper
    : public base::RefCountedThreadSafe<BrowsingDataFileSystemHelper> {
 public:
  // Detailed information about a file system, including it's origin GURL,
  // the presence or absence of persistent and temporary storage, and the
  // amount of data (in bytes) each contains.
  struct FileSystemInfo {
    FileSystemInfo(
        const GURL& origin,
        bool has_persistent,
        bool has_temporary,
        int64 usage_persistent,
        int64 usage_temporary);
    ~FileSystemInfo();

    // The origin for which the information is relevant.
    GURL origin;
    // True if the origin has a persistent file system.
    bool has_persistent;
    // True if the origin has a temporary file system.
    bool has_temporary;
    // Persistent file system usage, in bytes.
    int64 usage_persistent;
    // Temporary file system usage, in bytes.
    int64 usage_temporary;
  };

  // Creates a BrowsingDataFileSystemHelper instance for the file systems
  // stored in |profile|'s user data directory. The BrowsingDataFileSystemHelper
  // object will hold a reference to the Profile that's passed in, but is not
  // responsible for destroying it.
  //
  // The BrowsingDataFileSystemHelper will not change the profile itself, but
  // can modify data it contains (by removing file systems).
  static BrowsingDataFileSystemHelper* Create(Profile* profile);

  // Starts the process of fetching file system data, which will call |callback|
  // upon completion, passing it a constant list of FileSystemInfo objects.
  // StartFetching must be called only in the UI thread; the provided Callback1
  // will likewise be executed asynchronously on the UI thread.
  //
  // BrowsingDataFileSystemHelper takes ownership of the Callback1, and is
  // responsible for deleting it once it's no longer needed.
  virtual void StartFetching(
      Callback1<const std::list<FileSystemInfo>& >::Type* callback) = 0;

  // Cancels the notification callback associated with StartFetching. Clients
  // that are destroyed before the callback is triggered must call this, and
  // it must be called only on the UI thread.
  virtual void CancelNotification() = 0;

  // Deletes any temporary or persistent file systems associated with |origin|
  // from the disk. Deletion will occur asynchronously on the FILE thread, but
  // this function must be called only on the UI thread.
  virtual void DeleteFileSystemOrigin(const GURL& origin) = 0;

 protected:
  friend class base::RefCountedThreadSafe<BrowsingDataFileSystemHelper>;

  BrowsingDataFileSystemHelper() {}
  virtual ~BrowsingDataFileSystemHelper() {}
};

// An implementation of the BrowsingDataFileSystemHelper interface that can
// be manually populated with data, rather than fetching data from the file
// systems created in a particular Profile.
class CannedBrowsingDataFileSystemHelper
    : public BrowsingDataFileSystemHelper {
 public:
  // |profile| is unused in this canned implementation, but it's the interface
  // we're writing to, so we'll accept it, but not store it.
  explicit CannedBrowsingDataFileSystemHelper(Profile* profile);

  // Creates a copy of the file system helper. StartFetching can only respond
  // to one client at a time; we need to be able to act on multiple parallel
  // requests in certain situations (see CookiesTreeModel and its clients). For
  // these cases, simply clone the object and fire off another fetching process.
  //
  // Clone() is safe to call while StartFetching() is running. Clients of the
  // newly created object must themselves execute StartFetching(), however: the
  // copy will not have a pending fetch.
  CannedBrowsingDataFileSystemHelper* Clone();

  // Manually adds a filesystem to the set of canned file systems that this
  // helper returns via StartFetching. If an origin contains both a temporary
  // and a persistent filesystem, AddFileSystem must be called twice (once for
  // each file system type).
  void AddFileSystem(const GURL& origin,
                     fileapi::FileSystemType type,
                     int64 size);

  // Clear this helper's list of canned filesystems.
  void Reset();

  // True if no filesystems are currently stored.
  bool empty() const;

  // BrowsingDataFileSystemHelper methods.
  virtual void StartFetching(
      Callback1<const std::list<FileSystemInfo>& >::Type* callback);
  virtual void CancelNotification();

  // Note that this doesn't actually have an implementation for this canned
  // class. It hasn't been necessary for anything that uses the canned
  // implementation, as the canned class is only used in tests, or in read-only
  // contexts (like the non-modal cookie dialog).
  virtual void DeleteFileSystemOrigin(const GURL& origin) {}

 private:
  // Used by Clone() to create an object without a Profile
  CannedBrowsingDataFileSystemHelper();
  virtual ~CannedBrowsingDataFileSystemHelper();

  // Triggers the success callback as the end of a StartFetching workflow. This
  // must be called on the UI thread.
  void NotifyOnUIThread();

  // Holds the current list of file systems returned to the client after
  // StartFetching is called.
  std::list<FileSystemInfo> file_system_info_;

  // Holds the callback passed in at the beginning of the StartFetching workflow
  // so that it can be triggered via NotifyOnUIThread.
  scoped_ptr<Callback1<const std::list<FileSystemInfo>& >::Type >
      completion_callback_;

  // Indicates whether or not we're currently fetching information: set to true
  // when StartFetching is called on the UI thread, and reset to false when
  // NotifyOnUIThread triggers the success callback.
  // This property only mutates on the UI thread.
  bool is_fetching_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataFileSystemHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_FILE_SYSTEM_HELPER_H_
