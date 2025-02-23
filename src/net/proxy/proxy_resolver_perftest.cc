// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/perftimer.h"
#include "base/string_util.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_resolver_js_bindings.h"
#include "net/proxy/proxy_resolver_v8.h"
#include "net/proxy/sync_host_resolver.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "net/proxy/proxy_resolver_winhttp.h"
#elif defined(OS_MACOSX)
#include "net/proxy/proxy_resolver_mac.h"
#endif

class MockSyncHostResolver : public net::SyncHostResolver {
 public:
  virtual int Resolve(const net::HostResolver::RequestInfo& info,
                      net::AddressList* addresses) OVERRIDE {
    return net::ERR_NAME_NOT_RESOLVED;
  }

  virtual void Shutdown() OVERRIDE {}
};

// This class holds the URL to use for resolving, and the expected result.
// We track the expected result in order to make sure the performance
// test is actually resolving URLs properly, otherwise the perf numbers
// are meaningless :-)
struct PacQuery {
  const char* query_url;
  const char* expected_result;
};

// Entry listing which PAC scripts to load, and which URLs to try resolving.
// |queries| should be terminated by {NULL, NULL}. A sentinel is used
// rather than a length, to simplify using initializer lists.
struct PacPerfTest {
  const char* pac_name;
  PacQuery queries[100];

  // Returns the actual number of entries in |queries| (assumes NULL sentinel).
  int NumQueries() const;
};

// List of performance tests.
static PacPerfTest kPerfTests[] = {
  // This test uses an ad-blocker PAC script. This script is very heavily
  // regular expression oriented, and has no dependencies on the current
  // IP address, or DNS resolving of hosts.
  { "no-ads.pac",
    { // queries:
      {"http://www.google.com", "DIRECT"},
      {"http://www.imdb.com/photos/cmsicons/x", "PROXY 0.0.0.0:3421"},
      {"http://www.imdb.com/x", "DIRECT"},
      {"http://www.staples.com/", "DIRECT"},
      {"http://www.staples.com/pixeltracker/x", "PROXY 0.0.0.0:3421"},
      {"http://www.staples.com/pixel/x", "DIRECT"},
      {"http://www.foobar.com", "DIRECT"},
      {"http://www.foobarbaz.com/x/y/z", "DIRECT"},
      {"http://www.testurl1.com/index.html", "DIRECT"},
      {"http://www.testurl2.com", "DIRECT"},
      {"https://www.sample/pirate/arrrrrr", "DIRECT"},
      {NULL, NULL}
    },
  },
};

int PacPerfTest::NumQueries() const {
  for (size_t i = 0; i < arraysize(queries); ++i) {
    if (queries[i].query_url == NULL)
      return i;
  }
  NOTREACHED();  // Bad definition.
  return 0;
}

// The number of URLs to resolve when testing a PAC script.
const int kNumIterations = 500;

// Helper class to run through all the performance tests using the specified
// proxy resolver implementation.
class PacPerfSuiteRunner {
 public:
  // |resolver_name| is the label used when logging the results.
  PacPerfSuiteRunner(net::ProxyResolver* resolver,
                     const std::string& resolver_name)
      : resolver_(resolver),
        resolver_name_(resolver_name),
        test_server_(net::TestServer::TYPE_HTTP,
            FilePath(FILE_PATH_LITERAL("net/data/proxy_resolver_perftest"))) {
  }

  void RunAllTests() {
    ASSERT_TRUE(test_server_.Start());
    for (size_t i = 0; i < arraysize(kPerfTests); ++i) {
      const PacPerfTest& test_data = kPerfTests[i];
      RunTest(test_data.pac_name,
              test_data.queries,
              test_data.NumQueries());
    }
  }

 private:
  void RunTest(const std::string& script_name,
               const PacQuery* queries,
               int queries_len) {
    if (!resolver_->expects_pac_bytes()) {
      GURL pac_url =
          test_server_.GetURL(std::string("files/") + script_name);
      int rv = resolver_->SetPacScript(
          net::ProxyResolverScriptData::FromURL(pac_url), NULL);
      EXPECT_EQ(net::OK, rv);
    } else {
      LoadPacScriptIntoResolver(script_name);
    }

    // Do a query to warm things up. In the case of internal-fetch proxy
    // resolvers, the first resolve will be slow since it has to download
    // the PAC script.
    {
      net::ProxyInfo proxy_info;
      int result = resolver_->GetProxyForURL(
          GURL("http://www.warmup.com"), &proxy_info, NULL, NULL,
          net::BoundNetLog());
      ASSERT_EQ(net::OK, result);
    }

    // Start the perf timer.
    std::string perf_test_name = resolver_name_ + "_" + script_name;
    PerfTimeLogger timer(perf_test_name.c_str());

    for (int i = 0; i < kNumIterations; ++i) {
      // Round-robin between URLs to resolve.
      const PacQuery& query = queries[i % queries_len];

      // Resolve.
      net::ProxyInfo proxy_info;
      int result = resolver_->GetProxyForURL(GURL(query.query_url),
                                             &proxy_info, NULL, NULL,
                                             net::BoundNetLog());

      // Check that the result was correct. Note that ToPacString() and
      // ASSERT_EQ() are fast, so they won't skew the results.
      ASSERT_EQ(net::OK, result);
      ASSERT_EQ(query.expected_result, proxy_info.ToPacString());
    }

    // Print how long the test ran for.
    timer.Done();
  }

  // Read the PAC script from disk and initialize the proxy resolver with it.
  void LoadPacScriptIntoResolver(const std::string& script_name) {
    FilePath path;
    PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.AppendASCII("net");
    path = path.AppendASCII("data");
    path = path.AppendASCII("proxy_resolver_perftest");
    path = path.AppendASCII(script_name);

    // Try to read the file from disk.
    std::string file_contents;
    bool ok = file_util::ReadFileToString(path, &file_contents);

    // If we can't load the file from disk, something is misconfigured.
    LOG_IF(ERROR, !ok) << "Failed to read file: " << path.value();
    ASSERT_TRUE(ok);

    // Load the PAC script into the ProxyResolver.
    int rv = resolver_->SetPacScript(
        net::ProxyResolverScriptData::FromUTF8(file_contents), NULL);
    EXPECT_EQ(net::OK, rv);
  }

  net::ProxyResolver* resolver_;
  std::string resolver_name_;
  net::TestServer test_server_;
};

#if defined(OS_WIN)
TEST(ProxyResolverPerfTest, ProxyResolverWinHttp) {
  net::ProxyResolverWinHttp resolver;
  PacPerfSuiteRunner runner(&resolver, "ProxyResolverWinHttp");
  runner.RunAllTests();
}
#elif defined(OS_MACOSX)
TEST(ProxyResolverPerfTest, ProxyResolverMac) {
  net::ProxyResolverMac resolver;
  PacPerfSuiteRunner runner(&resolver, "ProxyResolverMac");
  runner.RunAllTests();
}
#endif

TEST(ProxyResolverPerfTest, ProxyResolverV8) {
  net::ProxyResolverJSBindings* js_bindings =
      net::ProxyResolverJSBindings::CreateDefault(
          new MockSyncHostResolver, NULL, NULL);

  net::ProxyResolverV8 resolver(js_bindings);
  PacPerfSuiteRunner runner(&resolver, "ProxyResolverV8");
  runner.RunAllTests();
}

