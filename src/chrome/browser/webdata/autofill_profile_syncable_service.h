// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_WEBDATA_AUTOFILL_PROFILE_SYNCABLE_SERVICE_H_
#define CHROME_BROWSER_WEBDATA_AUTOFILL_PROFILE_SYNCABLE_SERVICE_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/sync/api/sync_change.h"
#include "chrome/browser/sync/api/sync_data.h"
#include "chrome/browser/sync/api/sync_error.h"
#include "chrome/browser/sync/api/syncable_service.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/public/browser/notification_types.h"

class AutofillProfile;
class AutofillTable;
class ProfileSyncServiceAutofillTest;
class WebDataService;

namespace browser_sync {
class UnrecoverableErrorHandler;
}

extern const char kAutofillProfileTag[];

// The sync implementation for AutofillProfiles.
// MergeDataAndStartSyncing() called first, it does cloud->local and
// local->cloud syncs. Then for each cloud change we receive
// ProcessSyncChanges() and for each local change Observe() is called.
class AutofillProfileSyncableService
    : public SyncableService,
      public NotificationObserver,
      public base::NonThreadSafe {
 public:
  explicit AutofillProfileSyncableService(WebDataService* web_data_service);
  virtual ~AutofillProfileSyncableService();

  static syncable::ModelType model_type() { return syncable::AUTOFILL_PROFILE; }

  // SyncableService implementation.
  virtual SyncError MergeDataAndStartSyncing(
      syncable::ModelType type,
      const SyncDataList& initial_sync_data,
      SyncChangeProcessor* sync_processor) OVERRIDE;
  virtual void StopSyncing(syncable::ModelType type) OVERRIDE;
  virtual SyncDataList GetAllSyncData(syncable::ModelType type) const OVERRIDE;
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE;

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 protected:
  // A convenience wrapper of a bunch of state we pass around while
  // associating models, and send to the WebDatabase for persistence.
  // We do this so we hold the write lock for only a small period.
  // When storing the web db we are out of the write lock.
  struct DataBundle;

  // Helper to query WebDatabase for the current autofill state.
  // Made virtual for ease of mocking in the unit-test.
  // Caller owns returned |profiles|.
  virtual bool LoadAutofillData(std::vector<AutofillProfile*>* profiles);

  // Helper to persist any changes that occured during model association to
  // the WebDatabase.
  // Made virtual for ease of mocking in the unit-test.
  virtual bool SaveChangesToWebData(const DataBundle& bundle);

 private:
  friend class ProfileSyncServiceAutofillTest;
  friend class MockAutofillProfileSyncableService;
  FRIEND_TEST_ALL_PREFIXES(AutofillProfileSyncableServiceTest,
                           MergeDataAndStartSyncing);
  FRIEND_TEST_ALL_PREFIXES(AutofillProfileSyncableServiceTest, GetAllSyncData);
  FRIEND_TEST_ALL_PREFIXES(AutofillProfileSyncableServiceTest,
                           ProcessSyncChanges);
  FRIEND_TEST_ALL_PREFIXES(AutofillProfileSyncableServiceTest,
                           ActOnChange);

  // The map of the guid to profiles owned by the |profiles_| vector.
  typedef std::map<std::string, AutofillProfile*> GUIDToProfileMap;

  // Helper function that overwrites |profile| with data from proto-buffer
  // |specifics|.
  static bool OverwriteProfileWithServerData(
      const sync_pb::AutofillProfileSpecifics& specifics,
      AutofillProfile* profile);

  // Writes |profile| data into supplied |profile_specifics|.
  static void WriteAutofillProfile(const AutofillProfile& profile,
                                   sync_pb::EntitySpecifics* profile_specifics);

  // Creates |profile_map| from the supplied |profiles| vector. Necessary for
  // fast processing of the changes.
  void CreateGUIDToProfileMap(const std::vector<AutofillProfile*>& profiles,
                              GUIDToProfileMap* profile_map);

  // Creates or updates a profile based on |data|. Looks at the guid of the data
  // and if a profile with such guid is present in |profile_map| updates it. If
  // not, searches through it for similar profiles. If similar profile is
  // found substitutes it for the new one, otherwise adds a new profile. Returns
  // iterator pointing to added/updated profile.
  GUIDToProfileMap::iterator CreateOrUpdateProfile(
      const SyncData& data, GUIDToProfileMap* profile_map, DataBundle* bundle);

  // Syncs |change| to the cloud.
  void ActOnChange(const AutofillProfileChange& change);

  // Creates SyncData based on supplied |profile|.
  static SyncData CreateData(const AutofillProfile& profile);

  AutofillTable* GetAutofillTable() const;

  // For unit-tests.
  AutofillProfileSyncableService();
  void set_sync_processor(SyncChangeProcessor* sync_processor) {
    sync_processor_.reset(sync_processor);
  }

  WebDataService* web_data_service_;  // WEAK
  NotificationRegistrar notification_registrar_;

  // Cached Autofill profiles. *Warning* deleted profiles are still in the
  // vector - use the |profiles_map_| to iterate through actual profiles.
  ScopedVector<AutofillProfile> profiles_;
  GUIDToProfileMap profiles_map_;

  scoped_ptr<SyncChangeProcessor> sync_processor_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileSyncableService);
};

// This object is used in unit-tests as well, so it defined here.
struct AutofillProfileSyncableService::DataBundle {
  DataBundle();
  ~DataBundle();

  std::vector<std::string> profiles_to_delete;
  std::vector<AutofillProfile*> profiles_to_update;
  std::vector<AutofillProfile*> profiles_to_add;
};

#endif  // CHROME_BROWSER_WEBDATA_AUTOFILL_PROFILE_SYNCABLE_SERVICE_H_
