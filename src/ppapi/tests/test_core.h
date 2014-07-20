// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TEST_TEST_CORE_H_
#define PPAPI_TEST_TEST_CORE_H_

#include <string>

#include "ppapi/tests/test_case.h"

struct PPB_Core;

class TestCore : public TestCase {
 public:
  explicit TestCore(TestingInstance* instance) : TestCase(instance) {}

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTest();

  std::string TestTime();
  std::string TestTimeTicks();

};

#endif  // PPAPI_TEST_TEST_CORE_H_

