// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_FILE_REF_H_
#define PAPPI_TESTS_TEST_FILE_REF_H_

#include <string>

#include "ppapi/tests/test_case.h"

class TestFileRef : public TestCase {
 public:
  explicit TestFileRef(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTest();

 private:
  std::string TestCreate();
  std::string TestGetFileSystemType();
  std::string TestGetName();
  std::string TestGetPath();
  std::string TestGetParent();
  std::string TestMakeDirectory();
  std::string TestQueryAndTouchFile();
  std::string TestDeleteFileAndDirectory();
  std::string TestRenameFileAndDirectory();
};

#endif  // PAPPI_TESTS_TEST_FILE_REF_H_
