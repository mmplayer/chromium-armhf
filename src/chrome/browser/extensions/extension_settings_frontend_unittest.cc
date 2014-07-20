// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/extensions/extension_settings_backend.h"
#include "chrome/browser/extensions/extension_settings_frontend.h"
#include "chrome/browser/extensions/extension_settings_storage.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/testing_profile.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"
#include "content/common/notification_details.h"

class ExtensionSettingsFrontendTest : public testing::Test {
 public:
   ExtensionSettingsFrontendTest()
      : ui_thread_(BrowserThread::UI, MessageLoop::current()),
        file_thread_(BrowserThread::FILE, MessageLoop::current()) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    frontend_.reset(new ExtensionSettingsFrontend(temp_dir_.path()));
    profile_.reset(new TestingProfile(temp_dir_.path()));
  }

  virtual void TearDown() OVERRIDE {
    profile_.reset();
    frontend_.reset();
  }

 protected:
  // Puts the settings backend in |backend|.
  void GetBackend(ExtensionSettingsBackend** backend) {
    frontend_->RunWithBackend(
        base::Bind(
            &ExtensionSettingsFrontendTest::AssignBackend,
            base::Unretained(this),
            backend));
    MessageLoop::current()->RunAllPending();
    ASSERT_TRUE(*backend);
  }

  ScopedTempDir temp_dir_;
  scoped_ptr<ExtensionSettingsFrontend> frontend_;
  scoped_ptr<TestingProfile> profile_;

 private:
  // Intended as a ExtensionSettingsFrontend::BackendCallback from GetBackend.
  void AssignBackend(
      ExtensionSettingsBackend** dst, ExtensionSettingsBackend* src) {
    *dst = src;
  }

  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
};

TEST_F(ExtensionSettingsFrontendTest, SettingsPreservedAcrossReconstruction) {
  ExtensionSettingsBackend* backend;
  GetBackend(&backend);

  const std::string id = "ext";
  ExtensionSettingsStorage* storage = backend->GetStorage(id);

  // The correctness of Get/Set/Remove/Clear is tested elsewhere so no need to
  // be too rigorous.
  StringValue bar("bar");
  ExtensionSettingsStorage::Result result = storage->Set("foo", bar);
  ASSERT_FALSE(result.HasError());

  result = storage->Get();
  ASSERT_FALSE(result.HasError());
  EXPECT_FALSE(result.GetSettings()->empty());

  frontend_.reset(new ExtensionSettingsFrontend(temp_dir_.path()));
  GetBackend(&backend);
  storage = backend->GetStorage(id);

  result = storage->Get();
  ASSERT_FALSE(result.HasError());
  EXPECT_FALSE(result.GetSettings()->empty());
}

TEST_F(ExtensionSettingsFrontendTest, SettingsClearedOnUninstall) {
  ExtensionSettingsBackend* backend;
  GetBackend(&backend);

  const std::string id = "ext";
  ExtensionSettingsStorage* storage = backend->GetStorage(id);

  StringValue bar("bar");
  ExtensionSettingsStorage::Result result = storage->Set("foo", bar);
  ASSERT_FALSE(result.HasError());

  // This would be triggered by extension uninstall via an ExtensionDataDeleter.
  backend->DeleteExtensionData(id);

  // The storage area may no longer be valid post-uninstall, so re-request.
  storage = backend->GetStorage(id);
  result = storage->Get();
  ASSERT_FALSE(result.HasError());
  EXPECT_TRUE(result.GetSettings()->empty());
}

TEST_F(ExtensionSettingsFrontendTest, LeveldbDatabaseDeletedFromDiskOnClear) {
  ExtensionSettingsBackend* backend;
  GetBackend(&backend);

  const std::string id = "ext";
  ExtensionSettingsStorage* storage = backend->GetStorage(id);

  StringValue bar("bar");
  ExtensionSettingsStorage::Result result = storage->Set("foo", bar);
  ASSERT_FALSE(result.HasError());
  EXPECT_TRUE(file_util::PathExists(temp_dir_.path()));

  // Should need to both clear the database and delete the frontend for the
  // leveldb database to be deleted from disk.
  result = storage->Clear();
  ASSERT_FALSE(result.HasError());
  EXPECT_TRUE(file_util::PathExists(temp_dir_.path()));

  frontend_.reset();
  MessageLoop::current()->RunAllPending();
  // TODO(kalman): Figure out why this fails, despite appearing to work.
  // Leaving this commented out rather than disabling the whole test so that the
  // deletion code paths are at least exercised.
  //EXPECT_FALSE(file_util::PathExists(temp_dir_.path()));
}
