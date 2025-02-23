// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_MOCK_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_MOCK_PROVIDER_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/content_settings/content_settings_observable_provider.h"
#include "chrome/browser/content_settings/content_settings_origin_identifier_value_map.h"
#include "chrome/common/content_settings_pattern.h"

namespace content_settings {

// The class MockProvider is a mock for a non default content settings provider.
class MockProvider : public ObservableProvider {
 public:
  MockProvider();
  MockProvider(ContentSettingsPattern requesting_url_pattern,
               ContentSettingsPattern embedding_url_pattern,
               ContentSettingsType content_type,
               ResourceIdentifier resource_identifier,
               ContentSetting setting,
               bool read_only,
               bool is_managed);
  virtual ~MockProvider();

  virtual RuleIterator* GetRuleIterator(
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      bool incognito) const OVERRIDE;

  // The MockProvider is only able to store one content setting. So every time
  // this method is called the previously set content settings is overwritten.
  virtual void SetContentSetting(
      const ContentSettingsPattern& requesting_url_pattern,
      const ContentSettingsPattern& embedding_url_pattern,
      ContentSettingsType content_type,
      const ResourceIdentifier& resource_identifier,
      ContentSetting content_setting) OVERRIDE;

  virtual void ClearAllContentSettingsRules(
      ContentSettingsType content_type) OVERRIDE {}

  virtual void ShutdownOnUIThread() OVERRIDE;

  void set_read_only(bool read_only) {
    read_only_ = read_only;
  }

  bool read_only() const {
    return read_only_;
  }

 private:
  OriginIdentifierValueMap value_map_;
  bool read_only_;

  DISALLOW_COPY_AND_ASSIGN(MockProvider);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_MOCK_PROVIDER_H_
