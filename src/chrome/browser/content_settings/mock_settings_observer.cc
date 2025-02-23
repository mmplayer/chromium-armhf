// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/mock_settings_observer.h"

#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"
#include "googleurl/src/gurl.h"

MockSettingsObserver::MockSettingsObserver() {
  registrar_.Add(this, chrome::NOTIFICATION_CONTENT_SETTINGS_CHANGED,
                 NotificationService::AllSources());
}

MockSettingsObserver::~MockSettingsObserver() {}

void MockSettingsObserver::Observe(int type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  HostContentSettingsMap* map =
      Source<HostContentSettingsMap>(source).ptr();
  ContentSettingsDetails* settings_details =
      Details<ContentSettingsDetails>(details).ptr();
  OnContentSettingsChanged(map,
                           settings_details->type(),
                           settings_details->update_all_types(),
                           settings_details->primary_pattern(),
                           settings_details->secondary_pattern(),
                           settings_details->update_all());
  // This checks that calling a Get function from an observer doesn't
  // deadlock.
  map->GetContentSettings(GURL("http://random-hostname.com/"),
                          GURL("http://random-hostname.com/"));
}
