// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TABS_PINNED_TAB_SERVICE_H_
#define CHROME_BROWSER_TABS_PINNED_TAB_SERVICE_H_
#pragma once

#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class Profile;

// PinnedTabService is responsible for updating preferences with the set of
// pinned tabs to restore at startup. PinnedTabService listens for the
// appropriate set of notifications to know it should update preferences.
class PinnedTabService : public NotificationObserver,
                         public ProfileKeyedService {
 public:
  explicit PinnedTabService(Profile* profile);

 private:
  // Invoked when we're about to exit.
  void GotExit();

  // NotificationObserver.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  Profile* profile_;

  // If true we've seen an exit event (or the last browser is closing which
  // triggers an exit) and can ignore all other events.
  bool got_exiting_;

  // True if there is at least one normal browser for our profile.
  bool has_normal_browser_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PinnedTabService);
};

#endif  // CHROME_BROWSER_TABS_PINNED_TAB_SERVICE_H_
