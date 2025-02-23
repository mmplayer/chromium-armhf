// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_content_settings_store.h"

#include <set>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/content_settings/content_settings_origin_identifier_value_map.h"
#include "chrome/browser/content_settings/content_settings_rule.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/extensions/extension_content_settings_api_constants.h"
#include "chrome/browser/extensions/extension_content_settings_helpers.h"
#include "content/browser/browser_thread.h"

namespace helpers = extension_content_settings_helpers;
namespace keys = extension_content_settings_api_constants;

using content_settings::ConcatenationIterator;
using content_settings::Rule;
using content_settings::RuleIterator;
using content_settings::OriginIdentifierValueMap;
using content_settings::ResourceIdentifier;
using content_settings::ValueToContentSetting;

struct ExtensionContentSettingsStore::ExtensionEntry {
  // Extension id
  std::string id;
  // Whether extension is enabled in the profile.
  bool enabled;
  // Content settings.
  OriginIdentifierValueMap settings;
  // Persistent incognito content settings.
  OriginIdentifierValueMap incognito_persistent_settings;
  // Session-only incognito content settings.
  OriginIdentifierValueMap incognito_session_only_settings;
};

ExtensionContentSettingsStore::ExtensionContentSettingsStore() {
  DCHECK(OnCorrectThread());
}

ExtensionContentSettingsStore::~ExtensionContentSettingsStore() {
  STLDeleteValues(&entries_);
}

RuleIterator* ExtensionContentSettingsStore::GetRuleIterator(
    ContentSettingsType type,
    const content_settings::ResourceIdentifier& identifier,
    bool incognito) const {
  ScopedVector<RuleIterator> iterators;
  // Iterate the extensions based on install time (last installed extensions
  // first).
  ExtensionEntryMap::const_reverse_iterator entry;

  // The individual |RuleIterators| shouldn't lock; pass |lock_| to the
  // |ConcatenationIterator| in a locked state.
  scoped_ptr<base::AutoLock> auto_lock(new base::AutoLock(lock_));

  for (entry = entries_.rbegin(); entry != entries_.rend(); ++entry) {
    if (!entry->second->enabled)
      continue;

    if (incognito) {
      iterators.push_back(
          entry->second->incognito_session_only_settings.GetRuleIterator(
              type,
              identifier,
              NULL));
      iterators.push_back(
          entry->second->incognito_persistent_settings.GetRuleIterator(
              type,
              identifier,
              NULL));
    } else {
      iterators.push_back(
          entry->second->settings.GetRuleIterator(type, identifier, NULL));
    }
  }
  return new ConcatenationIterator(&iterators, auto_lock.release());
}

void ExtensionContentSettingsStore::SetExtensionContentSetting(
    const std::string& ext_id,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType type,
    const content_settings::ResourceIdentifier& identifier,
    ContentSetting setting,
    ExtensionPrefsScope scope) {
  {
    base::AutoLock lock(lock_);
    OriginIdentifierValueMap* map = GetValueMap(ext_id, scope);
    if (setting == CONTENT_SETTING_DEFAULT) {
      map->DeleteValue(primary_pattern, secondary_pattern, type, identifier);
    } else {
      map->SetValue(primary_pattern, secondary_pattern, type, identifier,
                    base::Value::CreateIntegerValue(setting));
    }
  }

  // Send notification that content settings changed.
  // TODO(markusheintz): Notifications should only be sent if the set content
  // setting is effective and not hidden by another setting of another
  // extension installed more recently.
  NotifyOfContentSettingChanged(ext_id,
                                scope != kExtensionPrefsScopeRegular);
}

void ExtensionContentSettingsStore::RegisterExtension(
    const std::string& ext_id,
    const base::Time& install_time,
    bool is_enabled) {
  base::AutoLock lock(lock_);
  ExtensionEntryMap::iterator i = FindEntry(ext_id);
  if (i != entries_.end()) {
    delete i->second;
    entries_.erase(i);
  }

  ExtensionEntry* entry = new ExtensionEntry;
  entry->id = ext_id;
  entry->enabled = is_enabled;
  entries_.insert(std::make_pair(install_time, entry));
}

void ExtensionContentSettingsStore::UnregisterExtension(
    const std::string& ext_id) {
  bool notify = false;
  bool notify_incognito = false;
  {
    base::AutoLock lock(lock_);
    ExtensionEntryMap::iterator i = FindEntry(ext_id);
    if (i == entries_.end())
      return;
    notify = !i->second->settings.empty();
    notify_incognito = !i->second->incognito_persistent_settings.empty() ||
                       !i->second->incognito_session_only_settings.empty();

    delete i->second;
    entries_.erase(i);
  }
  if (notify)
    NotifyOfContentSettingChanged(ext_id, false);
  if (notify_incognito)
    NotifyOfContentSettingChanged(ext_id, true);
}

void ExtensionContentSettingsStore::SetExtensionState(
    const std::string& ext_id, bool is_enabled) {
  bool notify = false;
  bool notify_incognito = false;
  {
    base::AutoLock lock(lock_);
    ExtensionEntryMap::const_iterator i = FindEntry(ext_id);
    if (i == entries_.end())
      return;
    notify = !i->second->settings.empty();
    notify_incognito = !i->second->incognito_persistent_settings.empty() ||
                       !i->second->incognito_session_only_settings.empty();

    i->second->enabled = is_enabled;
  }
  if (notify)
    NotifyOfContentSettingChanged(ext_id, false);
  if (notify_incognito)
    NotifyOfContentSettingChanged(ext_id, true);
}

OriginIdentifierValueMap* ExtensionContentSettingsStore::GetValueMap(
    const std::string& ext_id,
    ExtensionPrefsScope scope) {
  ExtensionEntryMap::const_iterator i = FindEntry(ext_id);
  if (i != entries_.end()) {
    switch (scope) {
      case kExtensionPrefsScopeRegular:
        return &(i->second->settings);
      case kExtensionPrefsScopeIncognitoPersistent:
        return &(i->second->incognito_persistent_settings);
      case kExtensionPrefsScopeIncognitoSessionOnly:
        return &(i->second->incognito_session_only_settings);
    }
  }
  return NULL;
}

const OriginIdentifierValueMap* ExtensionContentSettingsStore::GetValueMap(
    const std::string& ext_id,
    ExtensionPrefsScope scope) const {
  ExtensionEntryMap::const_iterator i = FindEntry(ext_id);
  if (i != entries_.end()) {
    switch (scope) {
      case kExtensionPrefsScopeRegular:
        return &(i->second->settings);
      case kExtensionPrefsScopeIncognitoPersistent:
        return &(i->second->incognito_persistent_settings);
      case kExtensionPrefsScopeIncognitoSessionOnly:
        return &(i->second->incognito_session_only_settings);
    }
  }
  return NULL;
}

void ExtensionContentSettingsStore::ClearContentSettingsForExtension(
    const std::string& ext_id,
    ExtensionPrefsScope scope) {
  bool notify = false;
  {
    base::AutoLock lock(lock_);
    OriginIdentifierValueMap* map = GetValueMap(ext_id, scope);
    notify = !map->empty();
    map->clear();
  }
  if (notify) {
    NotifyOfContentSettingChanged(ext_id, scope != kExtensionPrefsScopeRegular);
  }
}

base::ListValue* ExtensionContentSettingsStore::GetSettingsForExtension(
    const std::string& extension_id,
    ExtensionPrefsScope scope) const {
  base::AutoLock lock(lock_);
  const OriginIdentifierValueMap* map = GetValueMap(extension_id, scope);
  if (!map)
    return NULL;
  base::ListValue* settings = new base::ListValue();
  OriginIdentifierValueMap::EntryMap::const_iterator it;
  for (it = map->begin(); it != map->end(); ++it) {
    scoped_ptr<RuleIterator> rule_iterator(
        map->GetRuleIterator(it->first.content_type,
                             it->first.resource_identifier,
                             NULL));  // We already hold the lock.
    while (rule_iterator->HasNext()) {
      const Rule& rule = rule_iterator->Next();
      base::DictionaryValue* setting_dict = new base::DictionaryValue();
      setting_dict->SetString(keys::kPrimaryPatternKey,
                              rule.primary_pattern.ToString());
      setting_dict->SetString(keys::kSecondaryPatternKey,
                              rule.secondary_pattern.ToString());
      setting_dict->SetString(
          keys::kContentSettingsTypeKey,
          helpers::ContentSettingsTypeToString(it->first.content_type));
      setting_dict->SetString(keys::kResourceIdentifierKey,
                              it->first.resource_identifier);
      ContentSetting content_setting = ValueToContentSetting(rule.value.get());
      DCHECK_NE(CONTENT_SETTING_DEFAULT, content_setting);
      setting_dict->SetString(
          keys::kContentSettingKey,
          helpers::ContentSettingToString(content_setting));
      settings->Append(setting_dict);
    }
  }
  return settings;
}

void ExtensionContentSettingsStore::SetExtensionContentSettingsFromList(
    const std::string& extension_id,
    const base::ListValue* list,
    ExtensionPrefsScope scope) {
  for (base::ListValue::const_iterator it = list->begin();
       it != list->end(); ++it) {
    if ((*it)->GetType() != Value::TYPE_DICTIONARY) {
      NOTREACHED();
      continue;
    }
    base::DictionaryValue* dict = static_cast<base::DictionaryValue*>(*it);
    std::string primary_pattern_str;
    dict->GetString(keys::kPrimaryPatternKey, &primary_pattern_str);
    ContentSettingsPattern primary_pattern =
        ContentSettingsPattern::FromString(primary_pattern_str);
    DCHECK(primary_pattern.IsValid());

    std::string secondary_pattern_str;
    dict->GetString(keys::kSecondaryPatternKey, &secondary_pattern_str);
    ContentSettingsPattern secondary_pattern =
        ContentSettingsPattern::FromString(secondary_pattern_str);
    DCHECK(secondary_pattern.IsValid());

    std::string content_settings_type_str;
    dict->GetString(keys::kContentSettingsTypeKey, &content_settings_type_str);
    ContentSettingsType content_settings_type =
        helpers::StringToContentSettingsType(content_settings_type_str);
    DCHECK_NE(CONTENT_SETTINGS_TYPE_DEFAULT, content_settings_type);

    std::string resource_identifier;
    dict->GetString(keys::kResourceIdentifierKey, &resource_identifier);

    std::string content_setting_string;
    dict->GetString(keys::kContentSettingKey, &content_setting_string);
    ContentSetting setting = CONTENT_SETTING_DEFAULT;
    bool result =
        helpers::StringToContentSetting(content_setting_string, &setting);
    DCHECK(result);

    SetExtensionContentSetting(extension_id,
                               primary_pattern,
                               secondary_pattern,
                               content_settings_type,
                               resource_identifier,
                               setting,
                               scope);
  }
}

void ExtensionContentSettingsStore::AddObserver(Observer* observer) {
  DCHECK(OnCorrectThread());
  observers_.AddObserver(observer);
}

void ExtensionContentSettingsStore::RemoveObserver(Observer* observer) {
  DCHECK(OnCorrectThread());
  observers_.RemoveObserver(observer);
}

void ExtensionContentSettingsStore::NotifyOfContentSettingChanged(
    const std::string& extension_id,
    bool incognito) {
  FOR_EACH_OBSERVER(
      ExtensionContentSettingsStore::Observer,
      observers_,
      OnContentSettingChanged(extension_id, incognito));
}

bool ExtensionContentSettingsStore::OnCorrectThread() {
  // If there is no UI thread, we're most likely in a unit test.
  return !BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI);
}

ExtensionContentSettingsStore::ExtensionEntryMap::iterator
ExtensionContentSettingsStore::FindEntry(const std::string& ext_id) {
  ExtensionEntryMap::iterator i;
  for (i = entries_.begin(); i != entries_.end(); ++i) {
    if (i->second->id == ext_id)
      return i;
  }
  return entries_.end();
}

ExtensionContentSettingsStore::ExtensionEntryMap::const_iterator
ExtensionContentSettingsStore::FindEntry(const std::string& ext_id) const {
  ExtensionEntryMap::const_iterator i;
  for (i = entries_.begin(); i != entries_.end(); ++i) {
    if (i->second->id == ext_id)
      return i;
  }
  return entries_.end();
}
