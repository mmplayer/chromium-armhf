// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu_model.h"

#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu_model_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_init.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/url_constants.h"
#include "content/common/notification_service.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

namespace {

class ProfileSwitchObserver : public ProfileManagerObserver {
 public:
  virtual void OnProfileCreated(Profile* profile, Status status) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    if (status == STATUS_INITIALIZED) {
      ProfileManager::NewWindowWithProfile(profile,
                                           BrowserInit::IS_NOT_PROCESS_STARTUP,
                                           BrowserInit::IS_NOT_FIRST_RUN);
    }
  }

  virtual bool DeleteAfter() OVERRIDE { return true; }
};

}  // namespace

AvatarMenuModel::AvatarMenuModel(ProfileInfoInterface* profile_cache,
                                 AvatarMenuModelObserver* observer,
                                 Browser* browser)
    : profile_info_(profile_cache),
      observer_(observer),
      browser_(browser) {
  DCHECK(profile_info_);
  DCHECK(observer_);
  // Don't DCHECK(browser_) so that unit tests can reuse this ctor.

  // Register this as an observer of the info cache.
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
      NotificationService::AllSources());

  // Build the initial menu.
  RebuildMenu();
}

AvatarMenuModel::~AvatarMenuModel() {
  ClearMenu();
}

AvatarMenuModel::Item::Item(size_t model_index, const gfx::Image& icon)
    : icon(icon),
      active(false),
      model_index(model_index) {
}

AvatarMenuModel::Item::~Item() {
}

void AvatarMenuModel::SwitchToProfile(size_t index) {
  const Item& item = GetItemAt(index);
  FilePath path = profile_info_->GetPathOfProfileAtIndex(item.model_index);

  // This will be deleted by the manager after the profile is ready.
  ProfileSwitchObserver* observer = new ProfileSwitchObserver();
  g_browser_process->profile_manager()->CreateProfileAsync(
      path, observer);
  ProfileMetrics::LogProfileSwitchUser(ProfileMetrics::SWITCH_PROFILE_ICON);
}

void AvatarMenuModel::EditProfile(size_t index) {
  Browser* browser = browser_;
  if (!browser) {
    Profile* profile = g_browser_process->profile_manager()->GetProfileByPath(
        profile_info_->GetPathOfProfileAtIndex(GetItemAt(index).model_index));
    browser = Browser::Create(profile);
  }
  std::string page = chrome::kManageProfileSubPage;
  page += "#";
  page += base::IntToString(static_cast<int>(index));
  browser->ShowOptionsTab(page);
}

void AvatarMenuModel::AddNewProfile() {
  ProfileManager::CreateMultiProfileAsync();
  ProfileMetrics::LogProfileAddNewUser(ProfileMetrics::ADD_NEW_USER_ICON);
}

size_t AvatarMenuModel::GetNumberOfItems() {
  return items_.size();
}

const AvatarMenuModel::Item& AvatarMenuModel::GetItemAt(size_t index) {
  DCHECK_LT(index, items_.size());
  return *items_[index];
}

void AvatarMenuModel::Observe(int type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED, type);
  RebuildMenu();
  observer_->OnAvatarMenuModelChanged(this);
}

// static
bool AvatarMenuModel::ShouldShowAvatarMenu() {
  return ProfileManager::IsMultipleProfilesEnabled() &&
      g_browser_process->profile_manager()->GetNumberOfProfiles() > 1;
}

void AvatarMenuModel::RebuildMenu() {
  ClearMenu();

  const size_t count = profile_info_->GetNumberOfProfiles();
  for (size_t i = 0; i < count; ++i) {
    Item* item = new Item(i, profile_info_->GetAvatarIconOfProfileAtIndex(i));
    item->name = profile_info_->GetNameOfProfileAtIndex(i);
    item->sync_state = profile_info_->GetUserNameOfProfileAtIndex(i);
    if (item->sync_state.empty()) {
      item->sync_state = l10n_util::GetStringUTF16(
          IDS_PROFILES_LOCAL_PROFILE_STATE);
    }
    if (browser_) {
      FilePath path = profile_info_->GetPathOfProfileAtIndex(i);
      item->active = browser_->profile()->GetPath() == path;
    }
    items_.push_back(item);
  }
}

void AvatarMenuModel::ClearMenu() {
  STLDeleteContainerPointers(items_.begin(), items_.end());
  items_.clear();
}
