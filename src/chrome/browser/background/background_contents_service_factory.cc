// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_contents_service_factory.h"

#include "base/command_line.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/pref_names.h"

// static
BackgroundContentsService* BackgroundContentsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BackgroundContentsService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
BackgroundContentsServiceFactory* BackgroundContentsServiceFactory::
    GetInstance() {
  return Singleton<BackgroundContentsServiceFactory>::get();
}

BackgroundContentsServiceFactory::BackgroundContentsServiceFactory()
    : ProfileKeyedServiceFactory(ProfileDependencyManager::GetInstance()) {
}

BackgroundContentsServiceFactory::~BackgroundContentsServiceFactory() {
}

ProfileKeyedService* BackgroundContentsServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new BackgroundContentsService(profile,
                                       CommandLine::ForCurrentProcess());
}

void BackgroundContentsServiceFactory::RegisterUserPrefs(
    PrefService* user_prefs) {
  user_prefs->RegisterDictionaryPref(prefs::kRegisteredBackgroundContents,
                                     PrefService::UNSYNCABLE_PREF);
}

bool BackgroundContentsServiceFactory::ServiceHasOwnInstanceInIncognito() {
  return true;
}

bool BackgroundContentsServiceFactory::ServiceIsCreatedWithProfile() {
  return true;
}

bool BackgroundContentsServiceFactory::ServiceIsNULLWhileTesting() {
  return true;
}
