// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TEST_TEST_VAR_DEPRECATED_H_
#define PPAPI_TEST_TEST_VAR_DEPRECATED_H_

#include <string>

#include "ppapi/cpp/private/var_private.h"
#include "ppapi/tests/test_case.h"

struct PPB_Var_Deprecated;

class TestVarDeprecated : public TestCase {
 public:
  explicit TestVarDeprecated(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTest();

  void set_var_from_page(const pp::VarPrivate& v) { var_from_page_ = v; }

 protected:
  // Test case protected overrides.
  virtual pp::deprecated::ScriptableObject* CreateTestObject();

 private:
  std::string TestBasicString();
  std::string TestInvalidAndEmpty();
  std::string TestInvalidUtf8();
  std::string TestNullInputInUtf8Conversion();
  std::string TestValidUtf8();
  std::string TestUtf8WithEmbeddedNulls();
  std::string TestVarToUtf8ForWrongType();
  std::string TestHasPropertyAndMethod();
  std::string TestPassReference();

  // Used by the tests that access the C API directly.
  const PPB_Var_Deprecated* var_interface_;

  // Saves the var from when a value is set on the test from the page.
  pp::VarPrivate var_from_page_;
};

#endif  // PPAPI_TEST_TEST_VAR_DEPRECATED_H_

