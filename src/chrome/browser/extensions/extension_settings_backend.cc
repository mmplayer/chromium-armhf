// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_settings_backend.h"

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_settings_leveldb_storage.h"
#include "chrome/browser/extensions/extension_settings_storage_cache.h"
#include "chrome/browser/extensions/extension_settings_storage_quota_enforcer.h"
#include "chrome/browser/extensions/extension_settings_sync_util.h"
#include "chrome/browser/extensions/in_memory_extension_settings_storage.h"
#include "chrome/common/extensions/extension.h"
#include "content/browser/browser_thread.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace {

// Total quota for all settings per extension, in bytes.  100K should be enough
// for most uses, but this can be increased as demand increases.
const size_t kTotalQuotaBytes = 100 * 1024;

// Quota for each setting.  Sync supports 5k per setting, so be a bit more
// restrictive than that.
const size_t kQuotaPerSettingBytes = 2048;

// Max number of settings per extension.  Keep low for sync.
const size_t kMaxSettingKeys = 512;

}  // namespace

ExtensionSettingsBackend::ExtensionSettingsBackend(const FilePath& base_path)
    : base_path_(base_path),
      sync_processor_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

ExtensionSettingsBackend::~ExtensionSettingsBackend() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

ExtensionSettingsStorage* ExtensionSettingsBackend::GetStorage(
    const std::string& extension_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DictionaryValue empty;
  return GetOrCreateStorageWithSyncData(extension_id, empty);
}

SyncableExtensionSettingsStorage*
ExtensionSettingsBackend::GetOrCreateStorageWithSyncData(
    const std::string& extension_id, const DictionaryValue& sync_data) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  linked_ptr<SyncableExtensionSettingsStorage> syncable_storage =
      storage_objs_[extension_id];
  if (syncable_storage.get()) {
    return syncable_storage.get();
  }

  ExtensionSettingsStorage* storage =
      ExtensionSettingsLeveldbStorage::Create(base_path_, extension_id);
  if (storage) {
    storage = new ExtensionSettingsStorageCache(storage);
  } else {
    // Failed to create a leveldb storage area, create an in-memory one.
    // It's ok for these to be synced, it just means that on next starting up
    // extensions will see the "old" settings, then overwritten (and notified)
    // when the sync changes come through.
    storage = new InMemoryExtensionSettingsStorage();
  }

  // It's fine to create the quota enforcer underneath the sync later, since
  // sync will only go ahead if each underlying storage operation is successful.
  storage = new ExtensionSettingsStorageQuotaEnforcer(
      kTotalQuotaBytes, kQuotaPerSettingBytes, kMaxSettingKeys, storage);

  syncable_storage =
      linked_ptr<SyncableExtensionSettingsStorage>(
          new SyncableExtensionSettingsStorage(extension_id, storage));
  if (sync_processor_) {
    // TODO(kalman): do something if StartSyncing fails.
    ignore_result(syncable_storage->StartSyncing(sync_data, sync_processor_));
  }

  storage_objs_[extension_id] = syncable_storage;
  return syncable_storage.get();
}

void ExtensionSettingsBackend::DeleteExtensionData(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  StorageObjMap::iterator maybe_storage = storage_objs_.find(extension_id);
  if (maybe_storage == storage_objs_.end()) {
    return;
  }

  // Clear settings when the extension is uninstalled.  Leveldb implementations
  // will also delete the database from disk when the object is destroyed as
  // a result of being removed from |storage_objs_|.
  maybe_storage->second->Clear();
  storage_objs_.erase(maybe_storage);
}

std::set<std::string> ExtensionSettingsBackend::GetKnownExtensionIDs() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::set<std::string> result;

  // TODO(kalman): we will need to do something to disambiguate between app
  // settings and extension settings, since settings for apps should be synced
  // iff app sync is turned on, ditto for extensions.

  // Extension IDs live in-memory and/or on disk.  The cache will contain all
  // that are in-memory.
  for (StorageObjMap::iterator it = storage_objs_.begin();
      it != storage_objs_.end(); ++it) {
    result.insert(it->first);
  }

  // Leveldb databases are directories inside base_path_.
  file_util::FileEnumerator::FindInfo find_info;
  file_util::FileEnumerator extension_dirs(
      base_path_, false, file_util::FileEnumerator::DIRECTORIES);
  while (!extension_dirs.Next().empty()) {
    extension_dirs.GetFindInfo(&find_info);
    FilePath extension_dir(file_util::FileEnumerator::GetFilename(find_info));
    DCHECK(!extension_dir.IsAbsolute());
    // Extension IDs are created as std::strings so they *should* be ASCII.
    std::string maybe_as_ascii(extension_dir.MaybeAsASCII());
    if (!maybe_as_ascii.empty()) {
      result.insert(maybe_as_ascii);
    }
  }

  return result;
}

SyncDataList ExtensionSettingsBackend::GetAllSyncData(
    syncable::ModelType type) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK_EQ(type, syncable::EXTENSION_SETTINGS);

  // For all extensions, get all their settings.
  // This has the effect of bringing in the entire state of extension settings
  // in memory; sad.
  SyncDataList all_sync_data;
  std::set<std::string> known_extension_ids(GetKnownExtensionIDs());

  for (std::set<std::string>::const_iterator it = known_extension_ids.begin();
      it != known_extension_ids.end(); ++it) {
    ExtensionSettingsStorage::Result maybe_settings = GetStorage(*it)->Get();
    if (maybe_settings.HasError()) {
      LOG(WARNING) << "Failed to get settings for " << *it << ": " <<
          maybe_settings.GetError();
      continue;
    }

    DictionaryValue* settings = maybe_settings.GetSettings();
    for (DictionaryValue::key_iterator key_it = settings->begin_keys();
        key_it != settings->end_keys(); ++key_it) {
      Value *value = NULL;
      settings->GetWithoutPathExpansion(*key_it, &value);
      all_sync_data.push_back(
          extension_settings_sync_util::CreateData(*it, *key_it, *value));
    }
  }

  return all_sync_data;
}

SyncError ExtensionSettingsBackend::MergeDataAndStartSyncing(
    syncable::ModelType type,
    const SyncDataList& initial_sync_data,
    SyncChangeProcessor* sync_processor) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK_EQ(type, syncable::EXTENSION_SETTINGS);
  DCHECK(!sync_processor_);
  sync_processor_ = sync_processor;

  // Group the initial sync data by extension id.
  std::map<std::string, linked_ptr<DictionaryValue> > grouped_sync_data;
  for (SyncDataList::const_iterator it = initial_sync_data.begin();
      it != initial_sync_data.end(); ++it) {
    ExtensionSettingSyncData data(*it);
    linked_ptr<DictionaryValue> sync_data =
        grouped_sync_data[data.extension_id()];
    if (!sync_data.get()) {
      sync_data = linked_ptr<DictionaryValue>(new DictionaryValue());
      grouped_sync_data[data.extension_id()] = sync_data;
    }
    DCHECK(!sync_data->HasKey(data.key())) <<
        "Duplicate settings for " << data.extension_id() << "/" << data.key();
    sync_data->Set(data.key(), data.value().DeepCopy());
  }

  // Start syncing all existing storage areas.  Any storage areas created in
  // the future will start being synced as part of the creation process.
  for (StorageObjMap::iterator it = storage_objs_.begin();
      it != storage_objs_.end(); ++it) {
    std::map<std::string, linked_ptr<DictionaryValue> >::iterator
        maybe_sync_data = grouped_sync_data.find(it->first);
    if (maybe_sync_data != grouped_sync_data.end()) {
      // TODO(kalman): do something if StartSyncing fails.
      ignore_result(
          it->second->StartSyncing(*maybe_sync_data->second, sync_processor));
      grouped_sync_data.erase(it->first);
    } else {
      DictionaryValue empty;
      // TODO(kalman): do something if StartSyncing fails.
      ignore_result(it->second->StartSyncing(empty, sync_processor));
    }
  }

  // Eagerly create and init the rest of the storage areas that have sync data.
  // Under normal circumstances (i.e. not first-time sync) this will be all of
  // them.
  for (std::map<std::string, linked_ptr<DictionaryValue> >::iterator it =
      grouped_sync_data.begin(); it != grouped_sync_data.end(); ++it) {
    GetOrCreateStorageWithSyncData(it->first, *it->second);
  }

  return SyncError();
}

SyncError ExtensionSettingsBackend::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& sync_changes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(sync_processor_);

  // Group changes by extension, to pass all changes in a single method call.
  std::map<std::string, ExtensionSettingSyncDataList> grouped_sync_data;
  for (SyncChangeList::const_iterator it = sync_changes.begin();
      it != sync_changes.end(); ++it) {
    ExtensionSettingSyncData data(*it);
    grouped_sync_data[data.extension_id()].push_back(data);
  }

  DictionaryValue empty;
  for (std::map<std::string, ExtensionSettingSyncDataList>::iterator
      it = grouped_sync_data.begin(); it != grouped_sync_data.end(); ++it) {
    // TODO(kalman): do something if ProcessSyncChanges fails.
    ignore_result(
        GetOrCreateStorageWithSyncData(it->first, empty)->
            ProcessSyncChanges(it->second));
  }

  return SyncError();
}

void ExtensionSettingsBackend::StopSyncing(syncable::ModelType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(sync_processor_);
  sync_processor_ = NULL;

  for (StorageObjMap::iterator it = storage_objs_.begin();
      it != storage_objs_.end(); ++it) {
    it->second->StopSyncing();
  }
}
