// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_USER_POLICY_DISK_CACHE_H_
#define CHROME_BROWSER_POLICY_USER_POLICY_DISK_CACHE_H_
#pragma once

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace enterprise_management {

class CachedCloudPolicyResponse;

}

namespace policy {

namespace em = enterprise_management;

// Handles the on-disk cache file used by UserPolicyCache. This class handles
// the necessary thread switching and may outlive the associated UserPolicyCache
// instance.
class UserPolicyDiskCache
    : public base::RefCountedThreadSafe<UserPolicyDiskCache> {
 public:
  // Indicates the status of a completed load operation.
  enum LoadResult {
    // Policy was loaded successfully.
    LOAD_RESULT_SUCCESS,
    // Cache file missing.
    LOAD_RESULT_NOT_FOUND,
    // Failed to read cache file.
    LOAD_RESULT_READ_ERROR,
    // Failed to parse cache file.
    LOAD_RESULT_PARSE_ERROR,
  };

  // Delegate interface for observing loads operations.
  class Delegate {
   public:
    virtual ~Delegate();
    virtual void OnDiskCacheLoaded(
        LoadResult result,
        const em::CachedCloudPolicyResponse& policy) = 0;
  };

  UserPolicyDiskCache(const base::WeakPtr<Delegate>& delegate,
                      const FilePath& backing_file_path);

  // Starts reading the policy cache from disk. Passes the read policy
  // information back to the hosting UserPolicyCache after a successful cache
  // load through UserPolicyCache::OnDiskCacheLoaded().
  void Load();

  // Triggers a write operation to the disk cache on the FILE thread.
  void Store(const em::CachedCloudPolicyResponse& policy);

 private:
  friend class base::RefCountedThreadSafe<UserPolicyDiskCache>;
  ~UserPolicyDiskCache();

  // Tries to load the cache file on the FILE thread.
  void LoadOnFileThread();

  // Forwards the result to the UI thread.
  void LoadDone(LoadResult result,
                const em::CachedCloudPolicyResponse& policy);

  // Passes back the successfully read policy to the cache on the UI thread.
  void ReportResultOnUIThread(LoadResult result,
                              const em::CachedCloudPolicyResponse& policy);

  // Saves a policy blob on the FILE thread.
  void StoreOnFileThread(const em::CachedCloudPolicyResponse& policy);

  base::WeakPtr<Delegate> delegate_;
  const FilePath backing_file_path_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicyDiskCache);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_USER_POLICY_DISK_CACHE_H_
