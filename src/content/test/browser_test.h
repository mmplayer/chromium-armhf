// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_BROWSER_TEST_H_
#define CONTENT_TEST_BROWSER_TEST_H_
#pragma once

// We only want to use InProcessBrowserTest in test targets which properly
// isolate each test case by running each test in a separate process.
// This way if a test hangs the test launcher can reliably terminate it.
//
// InProcessBrowserTest cannot be run more than once in the same address space
// anyway - otherwise the second test crashes.
#if defined(HAS_OUT_OF_PROC_TEST_RUNNER)

#if defined(BROWSER_TESTS_HEADER_OVERRIDE)
#include BROWSER_TESTS_HEADER_OVERRIDE
#else
#include "content/test/content_browser_test.h"
typedef ContentBrowserTest InProcessBrowserTest;
#endif

#define IN_PROC_BROWSER_TEST_(test_case_name, test_name, parent_class,\
                              parent_id)\
class GTEST_TEST_CLASS_NAME_(test_case_name, test_name) : public parent_class {\
 public:\
  GTEST_TEST_CLASS_NAME_(test_case_name, test_name)() {}\
 protected:\
  virtual void RunTestOnMainThread();\
 private:\
  virtual void TestBody() {}\
  static ::testing::TestInfo* const test_info_;\
  GTEST_DISALLOW_COPY_AND_ASSIGN_(\
      GTEST_TEST_CLASS_NAME_(test_case_name, test_name));\
};\
\
::testing::TestInfo* const GTEST_TEST_CLASS_NAME_(test_case_name, test_name)\
  ::test_info_ =\
    ::testing::internal::MakeAndRegisterTestInfo(\
        #test_case_name, #test_name, "", "", \
        (parent_id), \
        parent_class::SetUpTestCase, \
        parent_class::TearDownTestCase, \
        new ::testing::internal::TestFactoryImpl<\
            GTEST_TEST_CLASS_NAME_(test_case_name, test_name)>);\
void GTEST_TEST_CLASS_NAME_(test_case_name, test_name)::RunTestOnMainThread()

#define IN_PROC_BROWSER_TEST_F(test_fixture, test_name)\
  IN_PROC_BROWSER_TEST_(test_fixture, test_name, test_fixture,\
                    ::testing::internal::GetTypeId<test_fixture>())

#define IN_PROC_BROWSER_TEST_P(test_case_name, test_name) \
  class GTEST_TEST_CLASS_NAME_(test_case_name, test_name) \
      : public test_case_name { \
   public: \
    GTEST_TEST_CLASS_NAME_(test_case_name, test_name)() {} \
   protected: \
    virtual void RunTestOnMainThread(); \
   private: \
    virtual void TestBody() {} \
    static int AddToRegistry() { \
      ::testing::UnitTest::GetInstance()->parameterized_test_registry(). \
          GetTestCasePatternHolder<test_case_name>(\
              #test_case_name, __FILE__, __LINE__)->AddTestPattern(\
                  #test_case_name, \
                  #test_name, \
                  new ::testing::internal::TestMetaFactory< \
                      GTEST_TEST_CLASS_NAME_(test_case_name, test_name)>()); \
      return 0; \
    } \
    static int gtest_registering_dummy_; \
    GTEST_DISALLOW_COPY_AND_ASSIGN_(\
        GTEST_TEST_CLASS_NAME_(test_case_name, test_name)); \
  }; \
  int GTEST_TEST_CLASS_NAME_(test_case_name, \
                             test_name)::gtest_registering_dummy_ = \
      GTEST_TEST_CLASS_NAME_(test_case_name, test_name)::AddToRegistry(); \
  void GTEST_TEST_CLASS_NAME_(test_case_name, test_name)::RunTestOnMainThread()

#endif  // defined(HAS_OUT_OF_PROC_TEST_RUNNER)

#endif  // CONTENT_TEST_BROWSER_TEST_H_
