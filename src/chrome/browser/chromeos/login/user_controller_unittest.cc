// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_controller.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

TEST(UserControllerTest, GetNameTooltipAddUser) {
  UserController guest_user_controller(NULL, false);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ADD_USER),
            guest_user_controller.GetNameTooltip());
}

TEST(UserControllerTest, GetNameTooltipIncognitoUser) {
  UserController new_user_controller(NULL, true);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_GO_INCOGNITO_BUTTON),
            new_user_controller.GetNameTooltip());
}

TEST(UserControllerTest, GetNameTooltipExistingUser) {
  // We need to have NotificationService and g_browser_process initialized
  // before we create UserController for existing user.
  // Otherwise we crash with either SEGFAULT or DCHECK.
  UserManager::User existing_user;
  existing_user.set_email("someordinaryuser@domain.com");
  UserController existing_user_controller(NULL, existing_user);
  EXPECT_EQ(ASCIIToUTF16("someordinaryuser (domain.com)"),
            existing_user_controller.GetNameTooltip());
}

}  // namespace chromeos
