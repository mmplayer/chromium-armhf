// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/wrench_menu_model.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_error.h"
#include "chrome/browser/ui/global_error_service.h"
#include "chrome/browser/ui/global_error_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/menu_model_test.h"
#include "chrome/test/base/testing_profile.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Error class has a menu item.
class MenuError : public GlobalError {
 public:
  explicit MenuError(int command_id)
      : command_id_(command_id),
        execute_count_(0) {
  }

  int execute_count() { return execute_count_; }

  bool HasBadge() OVERRIDE { return false; }
  virtual int GetBadgeResourceID() OVERRIDE {
    ADD_FAILURE();
    return 0;
  }

  virtual bool HasMenuItem() OVERRIDE { return true; }
  virtual int MenuItemCommandID() OVERRIDE { return command_id_; }
  virtual string16 MenuItemLabel() OVERRIDE { return string16(); }
  virtual void ExecuteMenuItem(Browser* browser) OVERRIDE { execute_count_++; }

  virtual bool HasBubbleView() OVERRIDE { return false; }
  virtual void ShowBubbleView(Browser* browser) OVERRIDE { ADD_FAILURE(); }
  virtual int GetBubbleViewIconResourceID() OVERRIDE {
    ADD_FAILURE();
    return 0;
  }
  virtual string16 GetBubbleViewTitle() OVERRIDE {
    ADD_FAILURE();
    return string16();
  }
  virtual string16 GetBubbleViewMessage() OVERRIDE {
    ADD_FAILURE();
    return string16();
  }
  virtual string16 GetBubbleViewAcceptButtonLabel() OVERRIDE {
    ADD_FAILURE();
    return string16();
  }
  virtual string16 GetBubbleViewCancelButtonLabel() OVERRIDE {
    ADD_FAILURE();
    return string16();
  }
  virtual void BubbleViewDidClose() OVERRIDE { ADD_FAILURE(); }
  virtual void BubbleViewAcceptButtonPressed() OVERRIDE { ADD_FAILURE(); }
  virtual void BubbleViewCancelButtonPressed() OVERRIDE { ADD_FAILURE(); }

 private:
  int command_id_;
  int execute_count_;

  DISALLOW_COPY_AND_ASSIGN(MenuError);
};

} // namespace

class WrenchMenuModelTest : public BrowserWithTestWindowTest,
                            public ui::AcceleratorProvider {
 public:
  // Don't handle accelerators.
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) { return false; }
};

// Copies parts of MenuModelTest::Delegate and combines them with the
// WrenchMenuModel since WrenchMenuModel is now a SimpleMenuModel::Delegate and
// not derived from SimpleMenuModel.
class TestWrenchMenuModel : public WrenchMenuModel {
 public:
  TestWrenchMenuModel(ui::AcceleratorProvider* provider,
                      Browser* browser)
      : WrenchMenuModel(provider, browser),
        execute_count_(0),
        checked_count_(0),
        enable_count_(0) {
  }

  // Testing overrides to ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const {
    bool val = WrenchMenuModel::IsCommandIdChecked(command_id);
    if (val)
      checked_count_++;
    return val;
  }

  virtual bool IsCommandIdEnabled(int command_id) const {
    ++enable_count_;
    return true;
  }

  virtual void ExecuteCommand(int command_id) { ++execute_count_; }

  int execute_count_;
  mutable int checked_count_;
  mutable int enable_count_;
};

TEST_F(WrenchMenuModelTest, Basics) {
  TestWrenchMenuModel model(this, browser());
  int itemCount = model.GetItemCount();

  // Verify it has items. The number varies by platform, so we don't check
  // the exact number.
  EXPECT_GT(itemCount, 10);

  // Execute a couple of the items and make sure it gets back to our delegate.
  // We can't use CountEnabledExecutable() here because the encoding menu's
  // delegate is internal, it doesn't use the one we pass in.
  model.ActivatedAt(0);
  EXPECT_TRUE(model.IsEnabledAt(0));
  // Make sure to use the index that is not separator in all configurations.
  model.ActivatedAt(2);
  EXPECT_TRUE(model.IsEnabledAt(2));
  EXPECT_EQ(model.execute_count_, 2);
  EXPECT_EQ(model.enable_count_, 2);

  model.execute_count_ = 0;
  model.enable_count_ = 0;

  // Choose something from the bookmark submenu and make sure it makes it back
  // to the delegate as well.
  int bookmarksModelIndex = -1;
  for (int i = 0; i < itemCount; ++i) {
    if (model.GetTypeAt(i) == ui::MenuModel::TYPE_SUBMENU) {
      bookmarksModelIndex = i;
      break;
    }
  }
  EXPECT_GT(bookmarksModelIndex, -1);
  ui::MenuModel* bookmarksModel = model.GetSubmenuModelAt(bookmarksModelIndex);
  EXPECT_TRUE(bookmarksModel);
  // The bookmarks model may be empty until we tell it we're going to show it.
  bookmarksModel->MenuWillShow();
  EXPECT_GT(bookmarksModel->GetItemCount(), 1);
  bookmarksModel->ActivatedAt(1);
  EXPECT_TRUE(bookmarksModel->IsEnabledAt(1));
  EXPECT_EQ(model.execute_count_, 1);
  EXPECT_EQ(model.enable_count_, 1);
}

// Tests global error menu items in the wrench menu.
TEST_F(WrenchMenuModelTest, GlobalError) {
  GlobalErrorService* service =
      GlobalErrorServiceFactory::GetForProfile(browser()->profile());
  const int command1 = 1234567;
  // AddGlobalError takes ownership of error1.
  MenuError* error1 = new MenuError(command1);
  service->AddGlobalError(error1);
  const int command2 = 1234568;
  // AddGlobalError takes ownership of error2.
  MenuError* error2 = new MenuError(command2);
  service->AddGlobalError(error2);

  WrenchMenuModel model(this, browser());
  int index1 = model.GetIndexOfCommandId(command1);
  EXPECT_GT(index1, -1);
  int index2 = model.GetIndexOfCommandId(command2);
  EXPECT_GT(index2, -1);

  EXPECT_TRUE(model.IsEnabledAt(index1));
  EXPECT_EQ(0, error1->execute_count());
  model.ActivatedAt(index1);
  EXPECT_EQ(1, error1->execute_count());

  EXPECT_TRUE(model.IsEnabledAt(index2));
  EXPECT_EQ(0, error2->execute_count());
  model.ActivatedAt(index2);
  EXPECT_EQ(1, error1->execute_count());
}

class EncodingMenuModelTest : public BrowserWithTestWindowTest,
                              public MenuModelTest {
};

TEST_F(EncodingMenuModelTest, IsCommandIdCheckedWithNoTabs) {
  EncodingMenuModel model(browser());
  ASSERT_EQ(NULL, browser()->GetSelectedTabContents());
  EXPECT_FALSE(model.IsCommandIdChecked(IDC_ENCODING_ISO88591));
}
