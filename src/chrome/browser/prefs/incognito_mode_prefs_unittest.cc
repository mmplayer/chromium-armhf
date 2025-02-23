// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/incognito_mode_prefs.h"

#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class IncognitoModePrefsTest : public testing::Test {
 protected:
  virtual void SetUp() {
    IncognitoModePrefs::RegisterUserPrefs(&prefs_);
  }

  TestingPrefService prefs_;
};

TEST_F(IncognitoModePrefsTest, IntToAvailability) {
  ASSERT_EQ(0, IncognitoModePrefs::ENABLED);
  ASSERT_EQ(1, IncognitoModePrefs::DISABLED);
  ASSERT_EQ(2, IncognitoModePrefs::FORCED);

  IncognitoModePrefs::Availability incognito;
  EXPECT_TRUE(IncognitoModePrefs::IntToAvailability(0, &incognito));
  EXPECT_EQ(IncognitoModePrefs::ENABLED, incognito);
  EXPECT_TRUE(IncognitoModePrefs::IntToAvailability(1, &incognito));
  EXPECT_EQ(IncognitoModePrefs::DISABLED, incognito);
  EXPECT_TRUE(IncognitoModePrefs::IntToAvailability(2, &incognito));
  EXPECT_EQ(IncognitoModePrefs::FORCED, incognito);

  EXPECT_FALSE(IncognitoModePrefs::IntToAvailability(10, &incognito));
  EXPECT_EQ(IncognitoModePrefs::ENABLED, incognito);
  EXPECT_FALSE(IncognitoModePrefs::IntToAvailability(-1, &incognito));
  EXPECT_EQ(IncognitoModePrefs::ENABLED, incognito);
}

TEST_F(IncognitoModePrefsTest, GetAvailability) {
  prefs_.SetUserPref(prefs::kIncognitoModeAvailability,
                     Value::CreateIntegerValue(IncognitoModePrefs::ENABLED));
  EXPECT_EQ(IncognitoModePrefs::ENABLED,
            IncognitoModePrefs::GetAvailability(&prefs_));

  prefs_.SetUserPref(prefs::kIncognitoModeAvailability,
                     Value::CreateIntegerValue(IncognitoModePrefs::DISABLED));
  EXPECT_EQ(IncognitoModePrefs::DISABLED,
            IncognitoModePrefs::GetAvailability(&prefs_));

  prefs_.SetUserPref(prefs::kIncognitoModeAvailability,
                     Value::CreateIntegerValue(IncognitoModePrefs::FORCED));
  EXPECT_EQ(IncognitoModePrefs::FORCED,
            IncognitoModePrefs::GetAvailability(&prefs_));
}

typedef IncognitoModePrefsTest IncognitoModePrefsDeathTest;

TEST_F(IncognitoModePrefsDeathTest, GetAvailabilityBadValue) {
  prefs_.SetUserPref(prefs::kIncognitoModeAvailability,
                     Value::CreateIntegerValue(-1));
#if defined(NDEBUG) && defined(DCHECK_ALWAYS_ON)
  EXPECT_DEATH({
    IncognitoModePrefs::Availability availability =
        IncognitoModePrefs::GetAvailability(&prefs_);
    EXPECT_EQ(IncognitoModePrefs::ENABLED, availability);
  }, "");
#else
  EXPECT_DEBUG_DEATH({
    IncognitoModePrefs::Availability availability =
        IncognitoModePrefs::GetAvailability(&prefs_);
    EXPECT_EQ(IncognitoModePrefs::ENABLED, availability);
  }, "");
#endif
}
