// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/stl_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/configuration_policy_provider_mac.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/preferences_mock_mac.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Holds parameters for the parametrized policy tests.
class PolicyTestParams {
 public:
  // Takes ownership of |test_value|.
  PolicyTestParams(ConfigurationPolicyType type,
                   const char* policy_name,
                   Value* test_value)
      : type_(type),
        policy_name_(policy_name),
        test_value_(test_value) {}

  // testing::TestWithParam does copying, so provide copy constructor and
  // assignment operator.
  PolicyTestParams(const PolicyTestParams& other)
      : type_(other.type_),
        policy_name_(other.policy_name_),
        test_value_(other.test_value_->DeepCopy()) {}

  const PolicyTestParams& operator=(PolicyTestParams other) {
    swap(other);
    return *this;
  }

  void swap(PolicyTestParams& other) {
    std::swap(type_, other.type_);
    std::swap(policy_name_, other.policy_name_);
    test_value_.swap(other.test_value_);
  }

  ConfigurationPolicyType type() const { return type_; }
  const char* policy_name() const { return policy_name_; }
  const Value* test_value() const { return test_value_.get(); }

  // Get the test value in the appropriate CFPropertyListRef representation.
  CFPropertyListRef GetPropertyListValue() const {
    switch (test_value_->GetType()) {
      case Value::TYPE_BOOLEAN: {
        bool v;
        if (!test_value_->GetAsBoolean(&v))
          return NULL;
        return CFRetain(v ? kCFBooleanTrue : kCFBooleanFalse);
      }
      case Value::TYPE_INTEGER: {
        int v;
        if (!test_value_->GetAsInteger(&v))
          return NULL;
        return CFNumberCreate(NULL, kCFNumberIntType, &v);
      }
      case Value::TYPE_STRING: {
        std::string v;
        if (!test_value_->GetAsString(&v))
          return NULL;
        return base::SysUTF8ToCFStringRef(v);
      }
      case Value::TYPE_LIST: {
        const ListValue* list =
            static_cast<const ListValue*>(test_value_.get());
        base::mac::ScopedCFTypeRef<CFMutableArrayRef> array(
            CFArrayCreateMutable(NULL, list->GetSize(),
                                 &kCFTypeArrayCallBacks));
        for (ListValue::const_iterator element(list->begin());
             element != list->end(); ++element) {
          if (!(*element)->IsType(Value::TYPE_STRING))
            return NULL;
          std::string element_value;
          if (!(*element)->GetAsString(&element_value))
            return NULL;
          base::mac::ScopedCFTypeRef<CFStringRef> cf_element_value(
              base::SysUTF8ToCFStringRef(element_value));
          CFArrayAppendValue(array, cf_element_value.get());
        }
        return array.release();
      }
      default:
        return NULL;
    }
  }

  // Factory methods that create parameter objects for different value types.
  static PolicyTestParams ForStringPolicy(
      ConfigurationPolicyType type,
      const char* name) {
    return PolicyTestParams(type, name, Value::CreateStringValue("test"));
  }
  static PolicyTestParams ForBooleanPolicy(
      ConfigurationPolicyType type,
      const char* name) {
    return PolicyTestParams(type, name, Value::CreateBooleanValue(true));
  }
  static PolicyTestParams ForIntegerPolicy(
      ConfigurationPolicyType type,
      const char* name) {
    return PolicyTestParams(type, name, Value::CreateIntegerValue(42));
  }
  static PolicyTestParams ForListPolicy(
      ConfigurationPolicyType type,
      const char* name) {
    ListValue* value = new ListValue;
    value->Set(0U, Value::CreateStringValue("first"));
    value->Set(1U, Value::CreateStringValue("second"));
    return PolicyTestParams(type, name, value);
  }

 private:
  ConfigurationPolicyType type_;
  const char* policy_name_;
  scoped_ptr<Value> test_value_;
};

// Parametrized test class for testing whether ConfigurationPolicyProviderMac
// can handle all policies correctly.
class ConfigurationPolicyProviderMacTest
    : public testing::TestWithParam<PolicyTestParams> {
 public:
  virtual void SetUp() {
    prefs_ = new MockPreferences;
  }

 protected:
  MockPreferences* prefs_;
};

TEST_P(ConfigurationPolicyProviderMacTest, Default) {
  ConfigurationPolicyProviderMac provider(
      GetChromePolicyDefinitionList(), prefs_);
  PolicyMap policy_map;
  EXPECT_TRUE(provider.Provide(&policy_map));
  EXPECT_TRUE(policy_map.empty());
}

TEST_P(ConfigurationPolicyProviderMacTest, Invalid) {
  base::mac::ScopedCFTypeRef<CFStringRef> name(
      base::SysUTF8ToCFStringRef(GetParam().policy_name()));
  base::mac::ScopedCFTypeRef<CFDataRef> invalid_data(
      CFDataCreate(NULL, NULL, 0));
  prefs_->AddTestItem(name, invalid_data.get(), true);

  // Create the provider and have it read |prefs_|.
  ConfigurationPolicyProviderMac provider(
      GetChromePolicyDefinitionList(), prefs_);
  PolicyMap policy_map;
  EXPECT_TRUE(provider.Provide(&policy_map));
  EXPECT_TRUE(policy_map.empty());
}

TEST_P(ConfigurationPolicyProviderMacTest, TestNonForcedValue) {
  base::mac::ScopedCFTypeRef<CFStringRef> name(
      base::SysUTF8ToCFStringRef(GetParam().policy_name()));
  base::mac::ScopedCFTypeRef<CFPropertyListRef> test_value(
      GetParam().GetPropertyListValue());
  ASSERT_TRUE(test_value.get());
  prefs_->AddTestItem(name, test_value.get(), false);

  // Create the provider and have it read |prefs_|.
  ConfigurationPolicyProviderMac provider(
      GetChromePolicyDefinitionList(), prefs_);
  PolicyMap policy_map;
  EXPECT_TRUE(provider.Provide(&policy_map));
  EXPECT_TRUE(policy_map.empty());
}

TEST_P(ConfigurationPolicyProviderMacTest, TestValue) {
  base::mac::ScopedCFTypeRef<CFStringRef> name(
      base::SysUTF8ToCFStringRef(GetParam().policy_name()));
  base::mac::ScopedCFTypeRef<CFPropertyListRef> test_value(
      GetParam().GetPropertyListValue());
  ASSERT_TRUE(test_value.get());
  prefs_->AddTestItem(name, test_value, true);

  // Create the provider and have it read |prefs_|.
  ConfigurationPolicyProviderMac provider(
      GetChromePolicyDefinitionList(), prefs_);
  PolicyMap policy_map;
  EXPECT_TRUE(provider.Provide(&policy_map));
  ASSERT_EQ(1U, policy_map.size());
  const Value* value = policy_map.Get(GetParam().type());
  ASSERT_TRUE(value);
  EXPECT_TRUE(GetParam().test_value()->Equals(value));
}

// Test parameters for all supported policies. testing::Values() has a limit of
// 50 parameters which is reached in this instantiation; new policies should go
// in the next instantiation after this one.
INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyProviderMacTestInstance,
    ConfigurationPolicyProviderMacTest,
    testing::Values(
        PolicyTestParams::ForStringPolicy(
            kPolicyHomepageLocation,
            key::kHomepageLocation),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyHomepageIsNewTabPage,
            key::kHomepageIsNewTabPage),
        PolicyTestParams::ForIntegerPolicy(
            kPolicyRestoreOnStartup,
            key::kRestoreOnStartup),
        PolicyTestParams::ForListPolicy(
            kPolicyRestoreOnStartupURLs,
            key::kRestoreOnStartupURLs),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyDefaultSearchProviderEnabled,
            key::kDefaultSearchProviderEnabled),
        PolicyTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderName,
            key::kDefaultSearchProviderName),
        PolicyTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderKeyword,
            key::kDefaultSearchProviderKeyword),
        PolicyTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderSearchURL,
            key::kDefaultSearchProviderSearchURL),
        PolicyTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderSuggestURL,
            key::kDefaultSearchProviderSuggestURL),
        PolicyTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderInstantURL,
            key::kDefaultSearchProviderInstantURL),
        PolicyTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderIconURL,
            key::kDefaultSearchProviderIconURL),
        PolicyTestParams::ForListPolicy(
            kPolicyDefaultSearchProviderEncodings,
            key::kDefaultSearchProviderEncodings),
        PolicyTestParams::ForStringPolicy(
            kPolicyProxyMode,
            key::kProxyMode),
        PolicyTestParams::ForIntegerPolicy(
            kPolicyProxyServerMode,
            key::kProxyServerMode),
        PolicyTestParams::ForStringPolicy(
            kPolicyProxyServer,
            key::kProxyServer),
        PolicyTestParams::ForStringPolicy(
            kPolicyProxyPacUrl,
            key::kProxyPacUrl),
        PolicyTestParams::ForStringPolicy(
            kPolicyProxyBypassList,
            key::kProxyBypassList),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyAlternateErrorPagesEnabled,
            key::kAlternateErrorPagesEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicySearchSuggestEnabled,
            key::kSearchSuggestEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyDnsPrefetchingEnabled,
            key::kDnsPrefetchingEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicySafeBrowsingEnabled,
            key::kSafeBrowsingEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyMetricsReportingEnabled,
            key::kMetricsReportingEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyPasswordManagerEnabled,
            key::kPasswordManagerEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyPasswordManagerAllowShowPasswords,
            key::kPasswordManagerAllowShowPasswords),
        PolicyTestParams::ForListPolicy(
            kPolicyDisabledPlugins,
            key::kDisabledPlugins),
        PolicyTestParams::ForListPolicy(
            kPolicyDisabledPluginsExceptions,
            key::kDisabledPluginsExceptions),
        PolicyTestParams::ForListPolicy(
            kPolicyEnabledPlugins,
            key::kEnabledPlugins),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyAutoFillEnabled,
            key::kAutoFillEnabled),
        PolicyTestParams::ForStringPolicy(
            kPolicyApplicationLocaleValue,
            key::kApplicationLocaleValue),
        PolicyTestParams::ForBooleanPolicy(
            kPolicySyncDisabled,
            key::kSyncDisabled),
        PolicyTestParams::ForListPolicy(
            kPolicyExtensionInstallWhitelist,
            key::kExtensionInstallWhitelist),
        PolicyTestParams::ForListPolicy(
            kPolicyExtensionInstallBlacklist,
            key::kExtensionInstallBlacklist),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyShowHomeButton,
            key::kShowHomeButton),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyPrintingEnabled,
            key::kPrintingEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyInstantEnabled,
            key::kInstantEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyDisablePluginFinder,
            key::kDisablePluginFinder),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyClearSiteDataOnExit,
            key::kClearSiteDataOnExit),
        PolicyTestParams::ForStringPolicy(
            kPolicyDownloadDirectory,
            key::kDownloadDirectory),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyDefaultBrowserSettingEnabled,
            key::kDefaultBrowserSettingEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyCloudPrintProxyEnabled,
            key::kCloudPrintProxyEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyTranslateEnabled,
            key::kTranslateEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyAllowOutdatedPlugins,
            key::kAllowOutdatedPlugins),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyAlwaysAuthorizePlugins,
            key::kAlwaysAuthorizePlugins),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyBookmarkBarEnabled,
            key::kBookmarkBarEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyEditBookmarksEnabled,
            key::kEditBookmarksEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyAllowFileSelectionDialogs,
            key::kAllowFileSelectionDialogs),
        PolicyTestParams::ForListPolicy(
            kPolicyDisabledSchemes,
            key::kDisabledSchemes),
        PolicyTestParams::ForStringPolicy(
            kPolicyDiskCacheDir,
            key::kDiskCacheDir),
        PolicyTestParams::ForIntegerPolicy(
            kPolicyMaxConnectionsPerProxy,
            key::kMaxConnectionsPerProxy),
        PolicyTestParams::ForListPolicy(
            kPolicyURLBlacklist,
            key::kURLBlacklist)));

// testing::Values has a limit of 50 test templates, which is reached by the
// instantiations above. Add tests for new policies here:
INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyProviderMacTestInstance2,
    ConfigurationPolicyProviderMacTest,
    testing::Values(
        PolicyTestParams::ForListPolicy(
            kPolicyURLWhitelist,
            key::kURLWhitelist)));

}  // namespace policy
