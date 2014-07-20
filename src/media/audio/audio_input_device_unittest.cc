// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Test that devices can be enumerated.
TEST(AudioInputDeviceTest, EnumerateDevices) {
  AudioDeviceNames device_names;
  ASSERT_TRUE(AudioManager::GetAudioManager() != NULL);
  AudioManager::GetAudioManager()->GetAudioInputDeviceNames(
      &device_names);
  if (!device_names.empty()) {
    AudioDeviceNames::const_iterator it = device_names.begin();
    // The first device in the list is the prepended default device.
    EXPECT_EQ("Default", it->device_name);
    EXPECT_EQ("0", it->unique_id);
    ++it;

    // Other devices should have non-empty name and id.
    while (it != device_names.end()) {
      EXPECT_EQ("", it->device_name);
      EXPECT_EQ("", it->unique_id);
      ++it;
    }
  }
}

}  // namespace media
