// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Maps hostnames to custom content settings.  Written on the UI thread and read
// on any thread.  One instance per profile.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_HOST_CONTENT_SETTINGS_MAP_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_HOST_CONTENT_SETTINGS_MAP_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/tuple.h"
#include "chrome/browser/content_settings/content_settings_observer.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

namespace base {
class Value;
}  // namespace base

namespace content_settings {
class ProviderInterface;
}  // namespace content_settings

class ExtensionService;
class GURL;
class PrefService;

class HostContentSettingsMap
    : public content_settings::Observer,
      public NotificationObserver,
      public base::RefCountedThreadSafe<HostContentSettingsMap> {
 public:
  enum ProviderType {
    POLICY_PROVIDER = 0,
    EXTENSION_PROVIDER = 1,
    PREF_PROVIDER,
    DEFAULT_PROVIDER,
    NUM_PROVIDER_TYPES,
  };

  // TODO(markusheintz): I sold my soul to the devil on order to add this tuple.
  // I really want my soul back, so I really will change this ASAP.
  typedef Tuple5<ContentSettingsPattern,
                 ContentSettingsPattern,
                 ContentSetting,
                 std::string,
                 bool> PatternSettingSourceTuple;
  typedef std::vector<PatternSettingSourceTuple> SettingsForOneType;

  HostContentSettingsMap(PrefService* prefs,
                         ExtensionService* extension_service,
                         bool incognito);

  static void RegisterUserPrefs(PrefService* prefs);

  // Returns the default setting for a particular content type.
  //
  // This may be called on any thread.
  ContentSetting GetDefaultContentSetting(
      ContentSettingsType content_type) const;

  // Returns the default settings for all content types.
  //
  // This may be called on any thread.
  ContentSettings GetDefaultContentSettings() const;

  // Returns a single |ContentSetting| which applies to the given URLs.
  // Note that certain internal schemes are whitelisted.
  // For |CONTENT_TYPE_COOKIES|, |GetCookieContentSetting| should be called,
  // and for content types that can't be converted to a ContentSetting,
  // |GetContentSettingValue| should be called.
  // If there is no content setting, returns CONTENT_SETTING_DEFAULT.
  // May be called on any thread.
  ContentSetting GetContentSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      const std::string& resource_identifier) const;

  // Returns a single content setting |Value| which applies to the given URLs.
  // If |primary_pattern| and |secondary_pattern| are not NULL, they are set to
  // the patterns of the applying rule.
  // Note that certain internal schemes are whitelisted.
  // If there is no content setting, returns NULL and leaves |primary_pattern|
  // and |secondary_pattern| unchanged.
  // Otherwise transfers ownership of the resulting |Value| to the caller.
  // May be called on any thread.
  base::Value* GetContentSettingValue(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      const std::string& resource_identifier,
      ContentSettingsPattern* primary_pattern,
      ContentSettingsPattern* secondary_pattern) const;

  // Gets the content setting for cookies. This takes the third party cookie
  // flag into account, and therefore needs to know whether we read or write a
  // cookie.
  //
  // This may be called on any thread.
  ContentSetting GetCookieContentSetting(
      const GURL& url,
      const GURL& first_party_url,
      bool setting_cookie) const;

  // Returns all ContentSettings which apply to the given URLs. For content
  // setting types that require an additional resource identifier, the default
  // content setting is returned.
  //
  // This may be called on any thread.
  ContentSettings GetContentSettings(
      const GURL& primary_url,
      const GURL& secondary_url) const;

  // For a given content type, returns all patterns with a non-default setting,
  // mapped to their actual settings, in lexicographical order.  |settings|
  // must be a non-NULL outparam. If this map was created for the
  // incognito profile, it will only return those settings differing from
  // the main map. For ContentSettingsTypes that require an resource identifier
  // to be specified, the |resource_identifier| must be non-empty.
  //
  // This may be called on any thread.
  void GetSettingsForOneType(ContentSettingsType content_type,
                             const std::string& resource_identifier,
                             SettingsForOneType* settings) const;

  // Sets the default setting for a particular content type. This method must
  // not be invoked on an incognito map.
  //
  // This should only be called on the UI thread.
  void SetDefaultContentSetting(ContentSettingsType content_type,
                                ContentSetting setting);

  // Sets the content setting for the given patterns and content type.
  // Setting the value to CONTENT_SETTING_DEFAULT causes the default setting
  // for that type to be used when loading pages matching this pattern. For
  // ContentSettingsTypes that require an resource identifier to be specified,
  // the |resource_identifier| must be non-empty.
  //
  // This should only be called on the UI thread.
  void SetContentSetting(const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern,
                         ContentSettingsType content_type,
                         const std::string& resource_identifier,
                         ContentSetting setting);

  // Convenience method to add a content setting for the given URLs, making sure
  // that there is no setting overriding it. For ContentSettingsTypes that
  // require an resource identifier to be specified, the |resource_identifier|
  // must be non-empty.
  //
  // This should only be called on the UI thread.
  void AddExceptionForURL(const GURL& primary_url,
                          const GURL& secondary_url,
                          ContentSettingsType content_type,
                          const std::string& resource_identifier,
                          ContentSetting setting);

  // Clears all host-specific settings for one content type.
  //
  // This should only be called on the UI thread.
  void ClearSettingsForOneType(ContentSettingsType content_type);

  static bool IsSettingAllowedForType(ContentSetting setting,
                                      ContentSettingsType content_type);

  // This setting trumps any host-specific settings.
  bool BlockThirdPartyCookies() const { return block_third_party_cookies_; }
  bool IsBlockThirdPartyCookiesManaged() const {
    return is_block_third_party_cookies_managed_;
  }

  // Sets whether we block all third-party cookies. This method must not be
  // invoked on an incognito map.
  //
  // This should only be called on the UI thread.
  void SetBlockThirdPartyCookies(bool block);

  // Returns true if the default setting for the |content_type| is managed.
  bool IsDefaultContentSettingManaged(ContentSettingsType content_type) const;

  // Detaches the HostContentSettingsMap from all Profile-related objects like
  // PrefService. This methods needs to be called before destroying the Profile.
  // Afterwards, none of the methods above that should only be called on the UI
  // thread should be called anymore.
  void ShutdownOnUIThread();

  // content_settings::Observer implementation.
  virtual void OnContentSettingChanged(
      ContentSettingsPattern primary_pattern,
      ContentSettingsPattern secondary_pattern,
      ContentSettingsType content_type,
      std::string resource_identifier);

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  friend class base::RefCountedThreadSafe<HostContentSettingsMap>;
  friend class HostContentSettingsMapTest_NonDefaultSettings_Test;

  typedef std::map<ProviderType, content_settings::ProviderInterface*>
      ProviderMap;
  typedef ProviderMap::iterator ProviderIterator;
  typedef ProviderMap::const_iterator ConstProviderIterator;

  virtual ~HostContentSettingsMap();

  ContentSetting GetDefaultContentSettingFromProvider(
      ContentSettingsType content_type,
      ProviderType provider_type) const;

  // Various migration methods (old cookie, popup and per-host data gets
  // migrated to the new format).
  void MigrateObsoleteCookiePref();

  // Adds content settings for |content_type| and |resource_identifier|,
  // provided by |provider|, into |settings|. If |incognito| is true, adds only
  // the content settings which are applicable to the incognito mode and differ
  // from the normal mode. Otherwise, adds the content settings for the normal
  // mode.
  void AddSettingsForOneType(
      const content_settings::ProviderInterface* provider,
      ProviderType provider_type,
      ContentSettingsType content_type,
      const std::string& resource_identifier,
      SettingsForOneType* settings,
      bool incognito) const;

  // Weak; owned by the Profile.
  PrefService* prefs_;

  PrefChangeRegistrar pref_change_registrar_;

  // Whether this settings map is for an OTR session.
  bool is_off_the_record_;

  // Whether we are currently updating preferences, this is used to ignore
  // notifications from the preferences service that we triggered ourself.
  bool updating_preferences_;

  // Content setting providers.
  ProviderMap content_settings_providers_;

  // Used around accesses to the following objects to guarantee thread safety.
  mutable base::Lock lock_;

  // Misc global settings.
  bool block_third_party_cookies_;
  bool is_block_third_party_cookies_managed_;

  DISALLOW_COPY_AND_ASSIGN(HostContentSettingsMap);
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_HOST_CONTENT_SETTINGS_MAP_H_
