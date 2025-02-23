// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNCED_SESSION_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNCED_SESSION_H_
#pragma once

#include <map>
#include <string>

#include "base/time.h"
#include "chrome/browser/sessions/session_id.h"

struct SessionTab;
struct SessionWindow;

namespace browser_sync {

// Defines a synced session for use by session sync. A synced session is a
// list of windows along with a unique session identifer (tag) and meta-data
// about the device being synced.
struct SyncedSession {
  typedef std::map<SessionID::id_type, SessionWindow*> SyncedWindowMap;

  // The type of device.
  enum DeviceType {
    TYPE_UNSET = 0,
    TYPE_WIN = 1,
    TYPE_MACOSX = 2,
    TYPE_LINUX = 3,
    TYPE_CHROMEOS = 4,
    TYPE_OTHER = 5
  };

  SyncedSession();
  ~SyncedSession();

  // Unique tag for each session.
  std::string session_tag;
  // User-visible name
  std::string session_name;

  // Type of device this session is from.
  DeviceType device_type;

  // Last time this session was modified remotely.
  base::Time modified_time;

  // Map of windows that make up this session. Windowws are owned by the session
  // itself and free'd on destruction.
  SyncedWindowMap windows;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncedSession);
};

// Control which foreign tabs we're interested in syncing/displaying. Checks
// that the tab has navigations and is not a new tab (url == NTP).
// Note: A new tab page with back/forward history is valid.
bool IsValidSessionTab(const SessionTab& tab);

// Checks whether the window has tabs to sync. If no tabs to sync, it returns
// true, false otherwise.
bool SessionWindowHasNoTabsToSync(const SessionWindow& window);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNCED_SESSION_H_
