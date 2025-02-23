// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_QUOTA_HELPER_IMPL_H_
#define CHROME_BROWSER_BROWSING_DATA_QUOTA_HELPER_IMPL_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/callback_old.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/browsing_data_quota_helper.h"
#include "content/browser/browser_thread.h"
#include "webkit/quota/quota_types.h"

namespace quota {
class QuotaManager;
}

// Implementation of BrowsingDataQuotaHelper.  Since a client of
// BrowsingDataQuotaHelper should live in UI thread and QuotaManager lives in
// IO thread, we have to communicate over thread using PostTask.
class BrowsingDataQuotaHelperImpl : public BrowsingDataQuotaHelper {
 public:
  virtual void StartFetching(FetchResultCallback* callback) OVERRIDE;
  virtual void CancelNotification() OVERRIDE;
  virtual void RevokeHostQuota(const std::string& host) OVERRIDE;

 private:
  BrowsingDataQuotaHelperImpl(base::MessageLoopProxy* ui_thread,
                              base::MessageLoopProxy* io_thread,
                              quota::QuotaManager* quota_manager);
  virtual ~BrowsingDataQuotaHelperImpl();

  void FetchQuotaInfo();

  // Callback function for GetOriginModifiedSince.
  void GotOrigins(const std::set<GURL>& origins, quota::StorageType type);

  void ProcessPendingHosts();
  void GetHostUsage(const std::string& host, quota::StorageType type);

  // Callback function for GetHostUsage.
  void GotHostUsage(const std::string& host,
                    quota::StorageType type,
                    int64 usage);

  void OnComplete();
  void DidRevokeHostQuota(quota::QuotaStatusCode status,
                          const std::string& host,
                          quota::StorageType type,
                          int64 quota);

  scoped_refptr<quota::QuotaManager> quota_manager_;
  scoped_ptr<FetchResultCallback> callback_;

  typedef std::set<std::pair<std::string, quota::StorageType> > PendingHosts;
  PendingHosts pending_hosts_;
  std::map<std::string, QuotaInfo> quota_info_;

  bool is_fetching_;

  scoped_refptr<base::MessageLoopProxy> ui_thread_;
  scoped_refptr<base::MessageLoopProxy> io_thread_;
  base::ScopedCallbackFactory<BrowsingDataQuotaHelperImpl> callback_factory_;

  friend class BrowsingDataQuotaHelper;
  friend class BrowsingDataQuotaHelperTest;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataQuotaHelperImpl);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_QUOTA_HELPER_IMPL_H_
