// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_LOGIN_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_LOGIN_LIBRARY_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/cros/login_library.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockLoginLibrary : public LoginLibrary {
 public:
  MockLoginLibrary();
  virtual ~MockLoginLibrary();

  MOCK_METHOD0(Init, void(void));
  MOCK_METHOD0(EmitLoginPromptReady, void(void));
  MOCK_METHOD0(EmitLoginPromptVisible, void(void));
  MOCK_METHOD2(RequestRetrievePolicy, void(RetrievePolicyCallback, void*));
  MOCK_METHOD3(RequestStorePolicy, void(const std::string&,
                                        StorePolicyCallback,
                                        void*));
  MOCK_METHOD2(StartSession, void(const std::string&, const std::string&));
  MOCK_METHOD1(StopSession, void(const std::string&));
  MOCK_METHOD0(RestartEntd, void(void));
  MOCK_METHOD2(RestartJob, void(int, const std::string&));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_LOGIN_LIBRARY_H_
