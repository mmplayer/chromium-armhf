// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SAFE_BROWSING_CLIENT_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SAFE_BROWSING_CLIENT_H_
#pragma once

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"

// This is a helper class used by DownloadManager to check a download URL with
// SafeBrowsingService. The client is refcounted and will be released once there
// is no reference to it.
// Usage:
// {
//    scoped_refptr<DownloadSBClient> client_ = new DownloadSBClient(...);
//    client_->CheckDownloadUrl(
//        ..., base::Bind(&DownloadManager::UrlCallBack,
//                        base::Unretained(this)));
//    or
//    client_->CheckDownloadHash(
//        ..., base::Bind(&DownloadManager::HashCallBack,
//                        base::Unretained(this)));
// }
// DownloadManager::UrlCallBack(...) or HashCallCall {
//    // After this, the |client_| is gone.
// }
class DownloadSBClient
    : public SafeBrowsingService::Client,
      public base::RefCountedThreadSafe<DownloadSBClient> {
 public:
  typedef base::Callback<void(int32, bool)> UrlDoneCallback;
  typedef base::Callback<void(int32, bool)> HashDoneCallback;

  DownloadSBClient(int32 download_id,
                   const std::vector<GURL>& url_chain,
                   const GURL& referrer_url,
                   bool safe_browsing_enabled);

  // Call safebrowsing service to verify the download.
  // For each DownloadSBClient instance, either CheckDownloadUrl or
  // CheckDownloadHash can be called, and be called only once.
  // DownloadSBClient instance.
  void CheckDownloadUrl(const UrlDoneCallback& callback);
  void CheckDownloadHash(const std::string& hash,
                         const HashDoneCallback& callback);

 private:
  // Call SafeBrowsingService on IO thread to verify the download URL or
  // hash of downloaded file.
  void CheckDownloadUrlOnIOThread(const std::vector<GURL>& url_chain);
  void CheckDownloadHashOnIOThread(const std::string& hash);

  // Callback interfaces for SafeBrowsingService::Client.
  virtual void OnDownloadUrlCheckResult(
      const std::vector<GURL>& url_chain,
      SafeBrowsingService::UrlCheckResult result) OVERRIDE;
  virtual void OnDownloadHashCheckResult(
      const std::string& hash,
      SafeBrowsingService::UrlCheckResult result) OVERRIDE;

  // Enumerate for histogramming purposes.
  // DO NOT CHANGE THE ORDERING OF THESE VALUES (different histogram data will
  // be mixed together based on their values).
  enum SBStatsType {
    DOWNLOAD_URL_CHECKS_TOTAL,
    DOWNLOAD_URL_CHECKS_CANCELED,
    DOWNLOAD_URL_CHECKS_MALWARE,

    DOWNLOAD_HASH_CHECKS_TOTAL,
    DOWNLOAD_HASH_CHECKS_MALWARE,

    // Memory space for histograms is determined by the max.
    // ALWAYS ADD NEW VALUES BEFORE THIS ONE.
    DOWNLOAD_CHECKS_MAX
  };

  friend class base::RefCountedThreadSafe<DownloadSBClient>;
  friend class DownloadSBClientTest_UrlHit_Test;
  friend class DownloadSBClientTest_DigestHit_Test;

  // Set the |sb_service_| manually for testing purposes.
  void SetSBService(SafeBrowsingService* service) {
    sb_service_ = service;
  }

  virtual ~DownloadSBClient();

  // Call DownloadManager on UI thread for download URL or hash check.
  void SafeBrowsingCheckUrlDone(SafeBrowsingService::UrlCheckResult result);
  void SafeBrowsingCheckHashDone(SafeBrowsingService::UrlCheckResult result,
                                 const std::string& hash);

  // Report malware hits to safebrowsing service.  |hash| should be empty if
  // this is an URL check result.
  void ReportMalware(SafeBrowsingService::UrlCheckResult result,
                     const std::string& hash);

  // Update the UMA stats.
  void UpdateDownloadCheckStats(SBStatsType stat_type);

  UrlDoneCallback url_done_callback_;
  HashDoneCallback hash_done_callback_;

  int32 download_id_;
  scoped_refptr<SafeBrowsingService> sb_service_;

  // These URLs are used to report malware to safe browsing service.
  std::vector<GURL> url_chain_;
  GURL referrer_url_;

  // When a safebrowsing check starts, for stats purpose.
  base::TimeTicks start_time_;

  // Whether the profile from which this client was created has enabled the
  // safe browsing service.
  bool safe_browsing_enabled_;

  DISALLOW_COPY_AND_ASSIGN(DownloadSBClient);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SAFE_BROWSING_CLIENT_H_
