// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TEST_TEST_MEMORY_H_
#define PPAPI_TEST_TEST_MEMORY_H_

#include <string>

#include "ppapi/tests/test_case.h"

struct PPB_Memory_Dev;

class TestMemory : public TestCase {
 public:
  explicit TestMemory(TestingInstance* instance) : TestCase(instance) {}

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTest();

  std::string TestMemAlloc();
  std::string TestNullMemFree();

  // Used by the tests that access the C API directly.
  const PPB_Memory_Dev* memory_dev_interface_;
};

#endif  // PPAPI_TEST_TEST_VAR_H_

