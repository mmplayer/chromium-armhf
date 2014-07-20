// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/asynchronous_policy_loader.h"
#include "chrome/browser/policy/asynchronous_policy_provider.h"
#include "chrome/browser/policy/asynchronous_policy_test_base.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/policy_map.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

namespace policy {

// Creating the provider should provide initial policy.
TEST_F(AsynchronousPolicyTestBase, Provide) {
  InSequence s;
  DictionaryValue* policies = new DictionaryValue();
  policies->SetBoolean(policy::key::kSyncDisabled, true);
  EXPECT_CALL(*delegate_, Load()).WillOnce(Return(policies));
  AsynchronousPolicyProvider provider(
      GetChromePolicyDefinitionList(),
      new AsynchronousPolicyLoader(delegate_.release(), 10));
  PolicyMap policy_map;
  provider.Provide(&policy_map);
  EXPECT_TRUE(policy_map.Get(policy::kPolicySyncDisabled));
  EXPECT_EQ(1U, policy_map.size());
}


// Trigger a refresh manually and ensure that policy gets reloaded.
TEST_F(AsynchronousPolicyTestBase, ProvideAfterRefresh) {
  InSequence s;
  DictionaryValue* original_policies = new DictionaryValue();
  original_policies->SetBoolean(policy::key::kSyncDisabled, true);
  EXPECT_CALL(*delegate_, Load()).WillOnce(Return(original_policies));
  DictionaryValue* refresh_policies = new DictionaryValue();
  refresh_policies->SetBoolean(
      policy::key::kJavascriptEnabled,
      true);
  EXPECT_CALL(*delegate_, Load()).WillOnce(Return(refresh_policies));
  AsynchronousPolicyLoader* loader =
      new AsynchronousPolicyLoader(delegate_.release(), 10);
  AsynchronousPolicyProvider provider(GetChromePolicyDefinitionList(), loader);
  loop_.RunAllPending();
  loader->Reload();
  PolicyMap policy_map;
  provider.Provide(&policy_map);
  EXPECT_TRUE(policy_map.Get(policy::kPolicySyncDisabled));
  EXPECT_EQ(1U, policy_map.size());
}

}  // namespace policy
