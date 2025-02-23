// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_memory.h"

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"

namespace {

size_t kTestBufferSize = 1000;

}  // namespace

REGISTER_TEST_CASE(Memory);

bool TestMemory::Init() {
  memory_dev_interface_ = static_cast<const PPB_Memory_Dev*>(
      pp::Module::Get()->GetBrowserInterface(PPB_MEMORY_DEV_INTERFACE));
  return memory_dev_interface_ && InitTestingInterface();
}

void TestMemory::RunTest() {
  RUN_TEST(MemAlloc);
  RUN_TEST(NullMemFree);
}

std::string TestMemory::TestMemAlloc() {
  char* buffer = static_cast<char*>(
      memory_dev_interface_->MemAlloc(kTestBufferSize));
  // Touch a couple of locations.  Failure will crash the test.
  buffer[0] = '1';
  buffer[kTestBufferSize - 1] = '1';
  memory_dev_interface_->MemFree(buffer);

  PASS();
}

std::string TestMemory::TestNullMemFree() {
  // Failure crashes the test.
  memory_dev_interface_->MemFree(NULL);

  PASS();
}

