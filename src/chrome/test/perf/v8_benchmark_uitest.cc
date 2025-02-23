// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_value_serializer.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/javascript_test_util.h"
#include "chrome/test/ui/ui_perf_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

namespace {

static const FilePath::CharType kStartFile[] =
    FILE_PATH_LITERAL("run.html");

const char kRunV8Benchmark[] = "run-v8-benchmark";

class V8BenchmarkTest : public UIPerfTest {
 public:
  typedef std::map<std::string, std::string> ResultsMap;

  V8BenchmarkTest() : reference_(false) {
    dom_automation_enabled_ = true;
    show_window_ = true;
  }

  void RunTest() {
    FilePath::StringType start_file(kStartFile);
    FilePath test_path = GetV8BenchmarkDir();
    test_path = test_path.Append(start_file);
    GURL test_url(net::FilePathToFileURL(test_path));

    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_TRUE(tab.get());
    ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(test_url));

    // Wait for the test to finish.
    ASSERT_TRUE(WaitUntilTestCompletes(tab.get(), test_url));

    PrintResults(tab.get());
  }

 protected:
  bool reference_;  // True if this is a reference build.

 private:
  // Return the path to the V8 benchmark directory on the local filesystem.
  FilePath GetV8BenchmarkDir() {
    FilePath test_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
    return test_dir.AppendASCII("v8_benchmark");
  }

  bool WaitUntilTestCompletes(TabProxy* tab, const GURL& test_url) {
    return WaitUntilCookieValue(tab, test_url, "__done",
                                TestTimeouts::large_test_timeout_ms(), "1");
  }

  bool GetScore(TabProxy* tab, std::string* score) {
    std::wstring score_wide;
    bool succeeded = tab->ExecuteAndExtractString(L"",
        L"window.domAutomationController.send(automation.GetScore());",
        &score_wide);

    // Note that we don't use ASSERT_TRUE here (and in some other places) as it
    // doesn't work inside a function with a return type other than void.
    EXPECT_TRUE(succeeded);
    if (!succeeded)
      return false;

    score->assign(WideToUTF8(score_wide));
    return true;
  }

  bool GetResults(TabProxy* tab, ResultsMap* results) {
    std::wstring json_wide;
    bool succeeded = tab->ExecuteAndExtractString(L"",
        L"window.domAutomationController.send("
        L"    JSON.stringify(automation.GetResults()));",
        &json_wide);

    EXPECT_TRUE(succeeded);
    if (!succeeded)
      return false;

    std::string json = WideToUTF8(json_wide);
    return JsonDictionaryToMap(json, results);
  }

  void PrintResults(TabProxy* tab) {
    std::string score;
    ASSERT_TRUE(GetScore(tab, &score));

    ResultsMap results;
    ASSERT_TRUE(GetResults(tab, &results));

    std::string trace_name = reference_ ? "score_ref" : "score";
    std::string unit_name = "score (bigger is better)";

    PrintResult("score", "", trace_name, score, unit_name, true);

    ResultsMap::const_iterator it = results.begin();
    for (; it != results.end(); ++it)
      PrintResult(it->first, "", trace_name, it->second, unit_name, false);
  }

  DISALLOW_COPY_AND_ASSIGN(V8BenchmarkTest);
};

class V8BenchmarkReferenceTest : public V8BenchmarkTest {
 public:
  V8BenchmarkReferenceTest() : V8BenchmarkTest() {
    reference_ = true;
  }

  void SetUp() {
    UseReferenceBuild();
    V8BenchmarkTest::SetUp();
  }
};

TEST_F(V8BenchmarkTest, Perf) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kRunV8Benchmark))
    return;

  RunTest();
}

// Bug 87162: Disable Perf for ChromeOS
#if defined(OS_CHROMEOS)
#define MAYBE_Perf DISABLED_Perf
#else
#define MAYBE_Perf Perf
#endif

TEST_F(V8BenchmarkReferenceTest, MAYBE_Perf) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kRunV8Benchmark))
    return;

  RunTest();
}

}  // namespace
