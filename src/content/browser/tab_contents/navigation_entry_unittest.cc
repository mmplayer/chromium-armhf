// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

class NavigationEntryTest : public testing::Test {
 public:
  NavigationEntryTest() : instance_(NULL) {
  }

  virtual void SetUp() {
    entry1_.reset(new NavigationEntry);

    instance_ = SiteInstance::CreateSiteInstance(NULL);
    entry2_.reset(new NavigationEntry(instance_, 3,
                                      GURL("test:url"),
                                      GURL("from"),
                                      ASCIIToUTF16("title"),
                                      content::PAGE_TRANSITION_TYPED,
                                      false));
  }

  virtual void TearDown() {
  }

 protected:
  scoped_ptr<NavigationEntry> entry1_;
  scoped_ptr<NavigationEntry> entry2_;
  // SiteInstances are deleted when their NavigationEntries are gone.
  SiteInstance* instance_;
};

// Test unique ID accessors
TEST_F(NavigationEntryTest, NavigationEntryUniqueIDs) {
  // Two entries should have different IDs by default
  EXPECT_NE(entry1_.get()->unique_id(), entry2_.get()->unique_id());

  // Can set an entry to have the same ID as another
  entry2_.get()->set_unique_id(entry1_.get()->unique_id());
  EXPECT_EQ(entry1_.get()->unique_id(), entry2_.get()->unique_id());
}

// Test URL accessors
TEST_F(NavigationEntryTest, NavigationEntryURLs) {
  // Start with no virtual_url (even if a url is set)
  EXPECT_FALSE(entry1_.get()->has_virtual_url());
  EXPECT_FALSE(entry2_.get()->has_virtual_url());

  EXPECT_EQ(GURL(), entry1_.get()->url());
  EXPECT_EQ(GURL(), entry1_.get()->virtual_url());
  EXPECT_TRUE(entry1_.get()->GetTitleForDisplay("").empty());

  // Setting URL affects virtual_url and GetTitleForDisplay
  entry1_.get()->set_url(GURL("http://www.google.com"));
  EXPECT_EQ(GURL("http://www.google.com"), entry1_.get()->url());
  EXPECT_EQ(GURL("http://www.google.com"), entry1_.get()->virtual_url());
  EXPECT_EQ(ASCIIToUTF16("www.google.com"),
            entry1_.get()->GetTitleForDisplay(""));

  // file:/// URLs should only show the filename.
  entry1_.get()->set_url(GURL("file:///foo/bar baz.txt"));
  EXPECT_EQ(ASCIIToUTF16("bar baz.txt"),
            entry1_.get()->GetTitleForDisplay(""));

  // Title affects GetTitleForDisplay
  entry1_.get()->set_title(ASCIIToUTF16("Google"));
  EXPECT_EQ(ASCIIToUTF16("Google"), entry1_.get()->GetTitleForDisplay(""));

  // Setting virtual_url doesn't affect URL
  entry2_.get()->set_virtual_url(GURL("display:url"));
  EXPECT_TRUE(entry2_.get()->has_virtual_url());
  EXPECT_EQ(GURL("test:url"), entry2_.get()->url());
  EXPECT_EQ(GURL("display:url"), entry2_.get()->virtual_url());

  // Having a title set in constructor overrides virtual URL
  EXPECT_EQ(ASCIIToUTF16("title"), entry2_.get()->GetTitleForDisplay(""));

  // User typed URL is independent of the others
  EXPECT_EQ(GURL(), entry1_.get()->user_typed_url());
  EXPECT_EQ(GURL(), entry2_.get()->user_typed_url());
  entry2_.get()->set_user_typed_url(GURL("typedurl"));
  EXPECT_EQ(GURL("typedurl"), entry2_.get()->user_typed_url());
}

// Test Favicon inner class
TEST_F(NavigationEntryTest, NavigationEntryFavicons) {
  EXPECT_EQ(GURL(), entry1_.get()->favicon().url());
  entry1_.get()->favicon().set_url(GURL("icon"));
  EXPECT_EQ(GURL("icon"), entry1_.get()->favicon().url());

  // Validity not affected by setting URL
  EXPECT_FALSE(entry1_.get()->favicon().is_valid());
  entry1_.get()->favicon().set_is_valid(true);
  EXPECT_TRUE(entry1_.get()->favicon().is_valid());
}

// Test SSLStatus inner class
TEST_F(NavigationEntryTest, NavigationEntrySSLStatus) {
  // Default (unknown)
  EXPECT_EQ(SECURITY_STYLE_UNKNOWN, entry1_.get()->ssl().security_style());
  EXPECT_EQ(SECURITY_STYLE_UNKNOWN, entry2_.get()->ssl().security_style());
  EXPECT_EQ(0, entry1_.get()->ssl().cert_id());
  EXPECT_EQ(0U, entry1_.get()->ssl().cert_status());
  EXPECT_EQ(-1, entry1_.get()->ssl().security_bits());
  EXPECT_FALSE(entry1_.get()->ssl().displayed_insecure_content());
  EXPECT_FALSE(entry1_.get()->ssl().ran_insecure_content());

  // Change from the defaults
  entry2_.get()->ssl().set_security_style(SECURITY_STYLE_AUTHENTICATED);
  entry2_.get()->ssl().set_cert_id(4);
  entry2_.get()->ssl().set_cert_status(net::CERT_STATUS_COMMON_NAME_INVALID);
  entry2_.get()->ssl().set_security_bits(0);
  entry2_.get()->ssl().set_displayed_insecure_content();
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED,
            entry2_.get()->ssl().security_style());
  EXPECT_EQ(4, entry2_.get()->ssl().cert_id());
  EXPECT_EQ(net::CERT_STATUS_COMMON_NAME_INVALID,
            entry2_.get()->ssl().cert_status());
  EXPECT_EQ(0, entry2_.get()->ssl().security_bits());
  EXPECT_TRUE(entry2_.get()->ssl().displayed_insecure_content());

  entry2_.get()->ssl().set_security_style(SECURITY_STYLE_AUTHENTICATION_BROKEN);
  entry2_.get()->ssl().set_ran_insecure_content();
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN,
            entry2_.get()->ssl().security_style());
  EXPECT_TRUE(entry2_.get()->ssl().ran_insecure_content());
}

// Test other basic accessors
TEST_F(NavigationEntryTest, NavigationEntryAccessors) {
  // SiteInstance
  EXPECT_TRUE(entry1_.get()->site_instance() == NULL);
  EXPECT_EQ(instance_, entry2_.get()->site_instance());
  entry1_.get()->set_site_instance(instance_);
  EXPECT_EQ(instance_, entry1_.get()->site_instance());

  // Page type
  EXPECT_EQ(NORMAL_PAGE, entry1_.get()->page_type());
  EXPECT_EQ(NORMAL_PAGE, entry2_.get()->page_type());
  entry2_.get()->set_page_type(INTERSTITIAL_PAGE);
  EXPECT_EQ(INTERSTITIAL_PAGE, entry2_.get()->page_type());

  // Referrer
  EXPECT_EQ(GURL(), entry1_.get()->referrer());
  EXPECT_EQ(GURL("from"), entry2_.get()->referrer());
  entry2_.get()->set_referrer(GURL("from2"));
  EXPECT_EQ(GURL("from2"), entry2_.get()->referrer());

  // Title
  EXPECT_EQ(string16(), entry1_.get()->title());
  EXPECT_EQ(ASCIIToUTF16("title"), entry2_.get()->title());
  entry2_.get()->set_title(ASCIIToUTF16("title2"));
  EXPECT_EQ(ASCIIToUTF16("title2"), entry2_.get()->title());

  // State
  EXPECT_EQ(std::string(), entry1_.get()->content_state());
  EXPECT_EQ(std::string(), entry2_.get()->content_state());
  entry2_.get()->set_content_state("state");
  EXPECT_EQ("state", entry2_.get()->content_state());

  // Page ID
  EXPECT_EQ(-1, entry1_.get()->page_id());
  EXPECT_EQ(3, entry2_.get()->page_id());
  entry2_.get()->set_page_id(2);
  EXPECT_EQ(2, entry2_.get()->page_id());

  // Transition type
  EXPECT_EQ(content::PAGE_TRANSITION_LINK, entry1_.get()->transition_type());
  EXPECT_EQ(content::PAGE_TRANSITION_TYPED, entry2_.get()->transition_type());
  entry2_.get()->set_transition_type(content::PAGE_TRANSITION_RELOAD);
  EXPECT_EQ(content::PAGE_TRANSITION_RELOAD, entry2_.get()->transition_type());

  // Is renderer initiated
  EXPECT_FALSE(entry1_.get()->is_renderer_initiated());
  EXPECT_FALSE(entry2_.get()->is_renderer_initiated());
  entry2_.get()->set_is_renderer_initiated(true);
  EXPECT_TRUE(entry2_.get()->is_renderer_initiated());

  // Post Data
  EXPECT_FALSE(entry1_.get()->has_post_data());
  EXPECT_FALSE(entry2_.get()->has_post_data());
  entry2_.get()->set_has_post_data(true);
  EXPECT_TRUE(entry2_.get()->has_post_data());

  // Restored
  EXPECT_EQ(NavigationEntry::RESTORE_NONE, entry1_->restore_type());
  EXPECT_EQ(NavigationEntry::RESTORE_NONE, entry2_->restore_type());
  entry2_->set_restore_type(NavigationEntry::RESTORE_LAST_SESSION);
  EXPECT_EQ(NavigationEntry::RESTORE_LAST_SESSION, entry2_->restore_type());
}
