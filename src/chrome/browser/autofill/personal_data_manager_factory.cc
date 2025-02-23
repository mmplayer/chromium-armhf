// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/personal_data_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

// static
PersonalDataManager* PersonalDataManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PersonalDataManager*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
PersonalDataManagerFactory* PersonalDataManagerFactory::GetInstance() {
  return Singleton<PersonalDataManagerFactory>::get();
}

PersonalDataManagerFactory::PersonalDataManagerFactory()
    : ProfileKeyedServiceFactory(ProfileDependencyManager::GetInstance()) {
  // TODO(erg): For Shutdown() order, we need to:
  //     DependsOn(WebDataServiceFactory::GetInstance());
}

PersonalDataManagerFactory::~PersonalDataManagerFactory() {
}

ProfileKeyedService* PersonalDataManagerFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  PersonalDataManager* personal_data_manager = new PersonalDataManager();
  personal_data_manager->Init(profile);
  return personal_data_manager;
}
