// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_POWER_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_POWER_LIBRARY_H_
#pragma once

#include "base/time.h"
#include "chrome/browser/chromeos/cros/power_library.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockPowerLibrary : public PowerLibrary {
 public:
  MockPowerLibrary();
  virtual ~MockPowerLibrary();

  MOCK_METHOD0(Init, void(void));

  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));

  MOCK_CONST_METHOD0(IsLinePowerOn, bool(void));
  MOCK_CONST_METHOD0(IsBatteryFullyCharged, bool(void));
  MOCK_CONST_METHOD0(GetBatteryPercentage, double(void));
  MOCK_CONST_METHOD0(IsBatteryPresent, bool(void));
  MOCK_CONST_METHOD0(GetBatteryTimeToEmpty, base::TimeDelta(void));
  MOCK_CONST_METHOD0(GetBatteryTimeToFull, base::TimeDelta(void));

  MOCK_METHOD1(CalculateIdleTime, void(CalculateIdleTimeCallback*));

  MOCK_METHOD1(EnableScreenLock, void(bool));
  MOCK_METHOD0(RequestRestart, void(void));
  MOCK_METHOD0(RequestShutdown, void(void));
  MOCK_METHOD0(RequestStatusUpdate, void(void));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_POWER_LIBRARY_H_
