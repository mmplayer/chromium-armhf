// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/product_unittest.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::win::RegKey;
using registry_util::RegistryOverrideManager;
using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

class MockRegistryValuePredicate : public InstallUtil::RegistryValuePredicate {
 public:
  MOCK_CONST_METHOD1(Evaluate, bool(const std::wstring&));
};

class InstallUtilTest : public TestWithTempDirAndDeleteTempOverrideKeys {
 protected:
};

TEST_F(InstallUtilTest, MakeUninstallCommand) {
  CommandLine command_line(CommandLine::NO_PROGRAM);

  std::pair<std::wstring, std::wstring> params[] = {
    std::make_pair(std::wstring(L""), std::wstring(L"")),
    std::make_pair(std::wstring(L""), std::wstring(L"--do-something --silly")),
    std::make_pair(std::wstring(L"spam.exe"), std::wstring(L"")),
    std::make_pair(std::wstring(L"spam.exe"),
                   std::wstring(L"--do-something --silly")),
  };
  for (int i = 0; i < arraysize(params); ++i) {
    std::pair<std::wstring, std::wstring>& param = params[i];
    InstallUtil::MakeUninstallCommand(param.first, param.second, &command_line);
    EXPECT_EQ(param.first, command_line.GetProgram().value());
    if (param.second.empty()) {
      EXPECT_TRUE(command_line.GetSwitches().empty());
    } else {
      EXPECT_EQ(2U, command_line.GetSwitches().size());
      EXPECT_TRUE(command_line.HasSwitch("do-something"));
      EXPECT_TRUE(command_line.HasSwitch("silly"));
    }
  }
}

TEST_F(InstallUtilTest, GetCurrentDate) {
  std::wstring date(InstallUtil::GetCurrentDate());
  EXPECT_EQ(8, date.length());
  if (date.length() == 8) {
    // For an invalid date value, SystemTimeToFileTime will fail.
    // We use this to validate that we have a correct date string.
    SYSTEMTIME systime = {0};
    FILETIME ft = {0};
    // Just to make sure our assumption holds.
    EXPECT_FALSE(SystemTimeToFileTime(&systime, &ft));
    // Now fill in the values from our string.
    systime.wYear = _wtoi(date.substr(0, 4).c_str());
    systime.wMonth = _wtoi(date.substr(4, 2).c_str());
    systime.wDay = _wtoi(date.substr(6, 2).c_str());
    // Check if they make sense.
    EXPECT_TRUE(SystemTimeToFileTime(&systime, &ft));
  }
}

TEST_F(InstallUtilTest, UpdateInstallerStageAP) {
  const bool system_level = false;
  const HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  std::wstring state_key_path(L"PhonyClientState");

  // Update the stage when there's no "ap" value.
  {
    RegistryOverrideManager override_manager;
    override_manager.OverrideRegistry(root, L"root_inst_res");
    RegKey(root, state_key_path.c_str(), KEY_SET_VALUE);
    InstallUtil::UpdateInstallerStage(system_level, state_key_path,
                                      installer::BUILDING);
    std::wstring value;
    EXPECT_EQ(ERROR_SUCCESS,
              RegKey(root, state_key_path.c_str(), KEY_QUERY_VALUE)
                  .ReadValue(google_update::kRegApField, &value));
    EXPECT_EQ(L"-stage:building", value);
  }

  // Update the stage when there is an "ap" value.
  {
    RegistryOverrideManager override_manager;
    override_manager.OverrideRegistry(root, L"root_inst_res");
    RegKey(root, state_key_path.c_str(), KEY_SET_VALUE)
        .WriteValue(google_update::kRegApField, L"2.0-dev");
    InstallUtil::UpdateInstallerStage(system_level, state_key_path,
                                      installer::BUILDING);
    std::wstring value;
    EXPECT_EQ(ERROR_SUCCESS,
              RegKey(root, state_key_path.c_str(), KEY_QUERY_VALUE)
                  .ReadValue(google_update::kRegApField, &value));
    EXPECT_EQ(L"2.0-dev-stage:building", value);
  }

  // Clear the stage.
  {
    RegistryOverrideManager override_manager;
    override_manager.OverrideRegistry(root, L"root_inst_res");
    RegKey(root, state_key_path.c_str(), KEY_SET_VALUE)
      .WriteValue(google_update::kRegApField, L"2.0-dev-stage:building");
    InstallUtil::UpdateInstallerStage(system_level, state_key_path,
                                      installer::NO_STAGE);
    std::wstring value;
    EXPECT_EQ(ERROR_SUCCESS,
              RegKey(root, state_key_path.c_str(), KEY_QUERY_VALUE)
                  .ReadValue(google_update::kRegApField, &value));
    EXPECT_EQ(L"2.0-dev", value);
  }
}

TEST_F(InstallUtilTest, UpdateInstallerStage) {
  const bool system_level = false;
  const HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  std::wstring state_key_path(L"PhonyClientState");

  // Update the stage when there's no "InstallerExtraCode1" value.
  {
    RegistryOverrideManager override_manager;
    override_manager.OverrideRegistry(root, L"root_inst_res");
    RegKey(root, state_key_path.c_str(), KEY_SET_VALUE)
        .DeleteValue(installer::kInstallerExtraCode1);
    InstallUtil::UpdateInstallerStage(system_level, state_key_path,
                                      installer::BUILDING);
    DWORD value;
    EXPECT_EQ(ERROR_SUCCESS,
              RegKey(root, state_key_path.c_str(), KEY_QUERY_VALUE)
                  .ReadValueDW(installer::kInstallerExtraCode1, &value));
    EXPECT_EQ(static_cast<DWORD>(installer::BUILDING), value);
  }

  // Update the stage when there is an "InstallerExtraCode1" value.
  {
    RegistryOverrideManager override_manager;
    override_manager.OverrideRegistry(root, L"root_inst_res");
    RegKey(root, state_key_path.c_str(), KEY_SET_VALUE)
        .WriteValue(installer::kInstallerExtraCode1,
                    static_cast<DWORD>(installer::UNPACKING));
    InstallUtil::UpdateInstallerStage(system_level, state_key_path,
                                      installer::BUILDING);
    DWORD value;
    EXPECT_EQ(ERROR_SUCCESS,
              RegKey(root, state_key_path.c_str(), KEY_QUERY_VALUE)
                  .ReadValueDW(installer::kInstallerExtraCode1, &value));
    EXPECT_EQ(static_cast<DWORD>(installer::BUILDING), value);
  }

  // Clear the stage.
  {
    RegistryOverrideManager override_manager;
    override_manager.OverrideRegistry(root, L"root_inst_res");
    RegKey(root, state_key_path.c_str(), KEY_SET_VALUE)
        .WriteValue(installer::kInstallerExtraCode1, static_cast<DWORD>(5));
    InstallUtil::UpdateInstallerStage(system_level, state_key_path,
                                      installer::NO_STAGE);
    DWORD value;
    EXPECT_EQ(ERROR_FILE_NOT_FOUND,
              RegKey(root, state_key_path.c_str(), KEY_QUERY_VALUE)
                  .ReadValueDW(installer::kInstallerExtraCode1, &value));
  }
}

TEST_F(InstallUtilTest, DeleteRegistryKeyIf) {
  const HKEY root = HKEY_CURRENT_USER;
  std::wstring parent_key_path(L"SomeKey\\ToDelete");
  std::wstring child_key_path(parent_key_path);
  child_key_path.append(L"\\ChildKey\\WithAValue");
  const wchar_t value_name[] = L"some_value_name";
  const wchar_t value[] = L"hi mom";

  {
    RegistryOverrideManager override_manager;
    override_manager.OverrideRegistry(root, L"root_key");
    // Nothing to delete if the keys aren't even there.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(_)).Times(0);
      ASSERT_FALSE(RegKey(root, parent_key_path.c_str(),
                          KEY_QUERY_VALUE).Valid());
      EXPECT_TRUE(InstallUtil::DeleteRegistryKeyIf(
          root, parent_key_path, child_key_path, value_name, pred));
      EXPECT_FALSE(RegKey(root, parent_key_path.c_str(),
                          KEY_QUERY_VALUE).Valid());
    }

    // Parent exists, but not child: no delete.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(_)).Times(0);
      ASSERT_TRUE(RegKey(root, parent_key_path.c_str(), KEY_SET_VALUE).Valid());
      EXPECT_TRUE(InstallUtil::DeleteRegistryKeyIf(
          root, parent_key_path, child_key_path, value_name, pred));
      EXPECT_TRUE(RegKey(root, parent_key_path.c_str(),
                         KEY_QUERY_VALUE).Valid());
    }

    // Child exists, but no value: no delete.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(_)).Times(0);
      ASSERT_TRUE(RegKey(root, child_key_path.c_str(), KEY_SET_VALUE).Valid());
      EXPECT_TRUE(InstallUtil::DeleteRegistryKeyIf(
          root, parent_key_path, child_key_path, value_name, pred));
      EXPECT_TRUE(RegKey(root, parent_key_path.c_str(),
                         KEY_QUERY_VALUE).Valid());
    }

    // Value exists, but doesn't match: no delete.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(StrEq(L"foosball!"))).WillOnce(Return(false));
      ASSERT_EQ(ERROR_SUCCESS,
                RegKey(root, child_key_path.c_str(),
                       KEY_SET_VALUE).WriteValue(value_name, L"foosball!"));
      EXPECT_TRUE(InstallUtil::DeleteRegistryKeyIf(
          root, parent_key_path, child_key_path, value_name, pred));
      EXPECT_TRUE(RegKey(root, parent_key_path.c_str(),
                         KEY_QUERY_VALUE).Valid());
    }

    // Value exists, and matches: delete.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(StrEq(value))).WillOnce(Return(true));
      ASSERT_EQ(ERROR_SUCCESS,
                RegKey(root, child_key_path.c_str(),
                       KEY_SET_VALUE).WriteValue(value_name, value));
      EXPECT_TRUE(InstallUtil::DeleteRegistryKeyIf(
          root, parent_key_path, child_key_path, value_name, pred));
      EXPECT_FALSE(RegKey(root, parent_key_path.c_str(),
                          KEY_QUERY_VALUE).Valid());
    }
  }
}

TEST_F(InstallUtilTest, DeleteRegistryValueIf) {
  const HKEY root = HKEY_CURRENT_USER;
  std::wstring key_path(L"SomeKey\\ToDelete");
  const wchar_t value_name[] = L"some_value_name";
  const wchar_t value[] = L"hi mom";

  {
    RegistryOverrideManager override_manager;
    override_manager.OverrideRegistry(root, L"root_key");
    // Nothing to delete if the key isn't even there.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(_)).Times(0);
      ASSERT_FALSE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
      EXPECT_TRUE(InstallUtil::DeleteRegistryValueIf(
          root, key_path.c_str(), value_name, pred));
      EXPECT_FALSE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
    }

    // Key exists, but no value: no delete.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(_)).Times(0);
      ASSERT_TRUE(RegKey(root, key_path.c_str(), KEY_SET_VALUE).Valid());
      EXPECT_TRUE(InstallUtil::DeleteRegistryValueIf(
          root, key_path.c_str(), value_name, pred));
      EXPECT_TRUE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
    }

    // Value exists, but doesn't match: no delete.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(StrEq(L"foosball!"))).WillOnce(Return(false));
      ASSERT_EQ(ERROR_SUCCESS,
                RegKey(root, key_path.c_str(),
                       KEY_SET_VALUE).WriteValue(value_name, L"foosball!"));
      EXPECT_TRUE(InstallUtil::DeleteRegistryValueIf(
          root, key_path.c_str(), value_name, pred));
      EXPECT_TRUE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
      EXPECT_TRUE(RegKey(root, key_path.c_str(),
                         KEY_QUERY_VALUE).ValueExists(value_name));
    }

    // Value exists, and matches: delete.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(StrEq(value))).WillOnce(Return(true));
      ASSERT_EQ(ERROR_SUCCESS,
                RegKey(root, key_path.c_str(),
                       KEY_SET_VALUE).WriteValue(value_name, value));
      EXPECT_TRUE(InstallUtil::DeleteRegistryValueIf(
          root, key_path.c_str(), value_name, pred));
      EXPECT_TRUE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
      EXPECT_FALSE(RegKey(root, key_path.c_str(),
                          KEY_QUERY_VALUE).ValueExists(value_name));
    }
  }

  {
    RegistryOverrideManager override_manager;
    override_manager.OverrideRegistry(root, L"root_key");
    // Default value matches: delete.
    {
      MockRegistryValuePredicate pred;

      EXPECT_CALL(pred, Evaluate(StrEq(value))).WillOnce(Return(true));
      ASSERT_EQ(ERROR_SUCCESS,
                RegKey(root, key_path.c_str(),
                       KEY_SET_VALUE).WriteValue(L"", value));
      EXPECT_TRUE(InstallUtil::DeleteRegistryValueIf(
          root, key_path.c_str(), L"", pred));
      EXPECT_TRUE(RegKey(root, key_path.c_str(), KEY_QUERY_VALUE).Valid());
      EXPECT_FALSE(RegKey(root, key_path.c_str(),
                          KEY_QUERY_VALUE).ValueExists(L""));
    }
  }
}

TEST_F(InstallUtilTest, ValueEquals) {
  InstallUtil::ValueEquals pred(L"howdy");

  EXPECT_FALSE(pred.Evaluate(L""));
  EXPECT_FALSE(pred.Evaluate(L"Howdy"));
  EXPECT_FALSE(pred.Evaluate(L"howdy!"));
  EXPECT_FALSE(pred.Evaluate(L"!howdy"));
  EXPECT_TRUE(pred.Evaluate(L"howdy"));
}
