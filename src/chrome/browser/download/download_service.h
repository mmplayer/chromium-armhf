// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SERVICE_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SERVICE_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

class ChromeDownloadManagerDelegate;
class DownloadManager;
class Profile;
class DownloadIdFactory;

// Owning class for DownloadManager (content) and
// ChromeDownloadManagerDelegate (chrome)
class DownloadService : public ProfileKeyedService {
 public:
  explicit DownloadService(Profile* profile);
  virtual ~DownloadService();

  DownloadIdFactory* GetDownloadIdFactory() const;

  // Get the download manager.  Creates the download manager if
  // it does not already exist.
  DownloadManager* GetDownloadManager();

  // Has a download manager been created?  (By calling above function.)
  bool HasCreatedDownloadManager();

  // Sets the DownloadManagerDelegate associated with this object and
  // its DownloadManager.  Takes ownership of |delegate|, and destroys
  // the previous delegate.  For testing.
  void SetDownloadManagerDelegateForTesting(
      ChromeDownloadManagerDelegate* delegate);

  // Will be called to release references on other services as part
  // of Profile shutdown.
  virtual void Shutdown() OVERRIDE;

 private:
  scoped_refptr<DownloadIdFactory> id_factory_;

  bool download_manager_created_;
  Profile* profile_;

  // Both of these objects are owned by this class.
  // DownloadManager is RefCountedThreadSafe because of references
  // from DownloadFile objects on the FILE thread, and may need to be
  // kept alive until those objects are deleted.
  // ChromeDownloadManagerDelegate may be the target of callbacks from
  // the history service/DB thread and must be kept alive for those
  // callbacks.
  scoped_refptr<DownloadManager> manager_;
  scoped_refptr<ChromeDownloadManagerDelegate> manager_delegate_;

  DISALLOW_COPY_AND_ASSIGN(DownloadService);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SERVICE_H_
