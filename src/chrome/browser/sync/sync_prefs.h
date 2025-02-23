// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_PREFS_H_
#define CHROME_BROWSER_SYNC_SYNC_PREFS_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/sync/notifier/invalidation_version_tracker.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "content/common/notification_observer.h"

class PrefService;

namespace browser_sync {

class SyncPrefObserver {
 public:
  // Called whenever the pref that controls whether sync is managed
  // changes.
  virtual void OnSyncManagedPrefChange(bool is_sync_managed) = 0;

 protected:
  virtual ~SyncPrefObserver();
};

// SyncPrefs is a helper class that manages getting, setting, and
// persisting global sync preferences.  It is not thread-safe, and
// lives on the UI thread.
//
// TODO(akalin): Some classes still read the prefs directly.  Consider
// passing down a pointer to SyncPrefs to them.  A list of files:
//
//   profile_sync_service_startup_unittest.cc
//   profile_sync_service.cc
//   sync_setup_flow.cc
//   sync_setup_wizard.cc
//   sync_setup_wizard_unittest.cc
//   two_client_preferences_sync_test.cc
class SyncPrefs : public base::SupportsWeakPtr<SyncPrefs>,
                  public sync_notifier::InvalidationVersionTracker,
                  public NotificationObserver {
 public:
  // |pref_service| may be NULL (for unit tests), but in that case no
  // setter methods should be called.  Does not take ownership of
  // |pref_service|.
  explicit SyncPrefs(PrefService* pref_service);

  virtual ~SyncPrefs();

  void AddSyncPrefObserver(SyncPrefObserver* sync_pref_observer);
  void RemoveSyncPrefObserver(SyncPrefObserver* sync_pref_observer);

  // Clears important sync preferences.
  void ClearPreferences();

  // Getters and setters for global sync prefs.

  bool HasSyncSetupCompleted() const;
  void SetSyncSetupCompleted();

  bool IsStartSuppressed() const;
  void SetStartSuppressed(bool is_suppressed);

  base::Time GetLastSyncedTime() const;
  void SetLastSyncedTime(base::Time time);

  bool HasKeepEverythingSynced() const;
  void SetKeepEverythingSynced(bool keep_everything_synced);

  // The returned set is guaranteed to be a subset of
  // |registered_types|.  Returns |registered_types| directly if
  // HasKeepEverythingSynced() is true.
  syncable::ModelTypeSet GetPreferredDataTypes(
      const syncable::ModelTypeSet& registered_types) const;
  // |preferred_types| should be a subset of |registered_types|.  All
  // types in |preferred_types| are marked preferred, and all types in
  // |registered_types| \ |preferred_types| are marked not preferred.
  // Changes are still made to the prefs even if
  // HasKeepEverythingSynced() is true, but won't be visible until
  // SetKeepEverythingSynced(false) is called.
  void SetPreferredDataTypes(
    const syncable::ModelTypeSet& registered_types,
    const syncable::ModelTypeSet& preferred_types);

  // This pref is set outside of sync.
  bool IsManaged() const;

  std::string GetEncryptionBootstrapToken() const;
  void SetEncryptionBootstrapToken(const std::string& token);

  // InvalidationVersionTracker implementation.
  virtual sync_notifier::InvalidationVersionMap
      GetAllMaxVersions() const OVERRIDE;
  virtual void SetMaxVersion(syncable::ModelType model_type,
                             int64 max_version) OVERRIDE;

  // Merges the given set of types with the set of acknowledged types.
  void AcknowledgeSyncedTypes(const syncable::ModelTypeSet& types);

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // For testing.

  void SetManagedForTest(bool is_managed);
  syncable::ModelTypeSet GetAcknowledgeSyncedTypesForTest() const;

 private:
  void RegisterPreferences();

  void RegisterDataTypePreferredPref(
      syncable::ModelType type, bool is_preferred);
  bool GetDataTypePreferred(syncable::ModelType type) const;
  void SetDataTypePreferred(syncable::ModelType type, bool is_preferred);

  base::NonThreadSafe non_thread_safe_;

  // May be NULL.
  PrefService* const pref_service_;

  ObserverList<SyncPrefObserver> sync_pref_observers_;

  // The preference that controls whether sync is under control by
  // configuration management.
  BooleanPrefMember pref_sync_managed_;

  DISALLOW_COPY_AND_ASSIGN(SyncPrefs);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_SYNC_PREFS_H_
