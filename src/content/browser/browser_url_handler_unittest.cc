// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_url_handler.h"
#include "content/test/test_browser_context.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

class BrowserURLHandlerTest : public testing::Test {
};

// Test URL rewriter that rewrites all "foo://" URLs to "bar://bar".
static bool FooRewriter(GURL* url, content::BrowserContext* browser_context) {
  if (url->scheme() == "foo") {
    *url = GURL("bar://bar");
    return true;
  }
  return false;
}

// Test URL rewriter that rewrites all "bar://" URLs to "foo://foo".
static bool BarRewriter(GURL* url, content::BrowserContext* browser_context) {
  if (url->scheme() == "bar") {
    *url = GURL("foo://foo");
    return true;
  }
  return false;
}

TEST_F(BrowserURLHandlerTest, BasicRewriteAndReverse) {
  TestBrowserContext browser_context;
  BrowserURLHandler handler;

  handler.AddHandlerPair(FooRewriter, BarRewriter);

  GURL url("foo://bar");
  GURL original_url(url);
  bool reverse_on_redirect = false;
  handler.RewriteURLIfNecessary(&url, &browser_context, &reverse_on_redirect);
  ASSERT_TRUE(reverse_on_redirect);
  ASSERT_EQ("bar://bar", url.spec());

  // Check that reversing the URL works.
  GURL saved_url(url);
  bool reversed = handler.ReverseURLRewrite(&url,
                                            original_url,
                                            &browser_context);
  ASSERT_TRUE(reversed);
  ASSERT_EQ("foo://foo", url.spec());

  // Check that reversing the URL only works with a matching |original_url|.
  url = saved_url;
  original_url = GURL("bam://bam");  // Won't be matched by FooRewriter.
  reversed = handler.ReverseURLRewrite(&url, original_url, &browser_context);
  ASSERT_FALSE(reversed);
  ASSERT_EQ(saved_url, url);
}

TEST_F(BrowserURLHandlerTest, NullHandlerReverse) {
  TestBrowserContext browser_context;
  BrowserURLHandler handler;

  GURL url("bar://foo");
  GURL original_url(url);

  handler.AddHandlerPair(BrowserURLHandler::null_handler(), FooRewriter);
  bool reversed = handler.ReverseURLRewrite(&url,
                                            original_url,
                                            &browser_context);
  ASSERT_FALSE(reversed);
  ASSERT_EQ(original_url, url);

  handler.AddHandlerPair(BrowserURLHandler::null_handler(), BarRewriter);
  reversed = handler.ReverseURLRewrite(&url, original_url, &browser_context);
  ASSERT_TRUE(reversed);
  ASSERT_EQ("foo://foo", url.spec());
}
