// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_INCOGNITO_MODE_PREFS_H_
#define CHROME_BROWSER_PREFS_INCOGNITO_MODE_PREFS_H_
#pragma once

#include "base/basictypes.h"

class PrefService;

// Specifies Incognito mode availability preferences.
class IncognitoModePrefs {
 public:
  // Possible values for Incognito mode availability. Please, do not change
  // the order of entries since numeric values are exposed to users.
  enum Availability {
    // Incognito mode enabled. Users may open pages in both Incognito mode and
    // normal mode (the default behaviour).
    ENABLED = 0,
    // Incognito mode disabled. Users may not open pages in Incognito mode.
    // Only normal mode is available for browsing.
    DISABLED,
    // Incognito mode forced. Users may open pages *ONLY* in Incognito mode.
    // Normal mode is not available for browsing.
    FORCED,

    AVAILABILITY_NUM_TYPES
  };

  // Register incognito related preferences.
  static void RegisterUserPrefs(PrefService* prefs);

  // Returns kIncognitoModeAvailability preference value stored
  // in the given pref service.
  static Availability GetAvailability(const PrefService* prefs);

  // Sets kIncognitoModeAvailability preference to the specified availability
  // value.
  static void SetAvailability(PrefService* prefs,
                              const Availability availability);

  // Converts in_value into the corresponding Availability value. Returns true
  // if conversion is successful (in_value is valid). Otherwise, returns false
  // and *out_value is set to ENABLED.
  static bool IntToAvailability(int in_value, Availability* out_value);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(IncognitoModePrefs);
};

#endif  // CHROME_BROWSER_PREFS_INCOGNITO_MODE_PREFS_H_
