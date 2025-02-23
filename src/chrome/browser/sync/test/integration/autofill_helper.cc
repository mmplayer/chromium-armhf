// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/autofill_helper.h"

#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "chrome/browser/webdata/autofill_table.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/thread_observer_helper.h"
#include "webkit/glue/form_field.h"

using base::WaitableEvent;
using sync_datatype_helper::test;
using testing::_;

namespace {

class GetAllAutofillEntries
    : public base::RefCountedThreadSafe<GetAllAutofillEntries> {
 public:
  explicit GetAllAutofillEntries(WebDataService* web_data_service)
      : web_data_service_(web_data_service),
        done_event_(false, false) {}

  void Init() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(
        BrowserThread::DB,
        FROM_HERE,
        NewRunnableMethod(this, &GetAllAutofillEntries::Run));
    done_event_.Wait();
  }

  const std::vector<AutofillEntry>& entries() const {
    return entries_;
  }

 private:
  friend class base::RefCountedThreadSafe<GetAllAutofillEntries>;

  void Run() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
    web_data_service_->GetDatabase()->GetAutofillTable()->GetAllAutofillEntries(
        &entries_);
    done_event_.Signal();
  }

  WebDataService* web_data_service_;
  base::WaitableEvent done_event_;
  std::vector<AutofillEntry> entries_;
};

ACTION_P(SignalEvent, event) {
  event->Signal();
}

class AutofillDBThreadObserverHelper : public DBThreadObserverHelper {
 protected:
  virtual void RegisterObservers() {
    registrar_.Add(&observer_,
                   chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED,
                   NotificationService::AllSources());
    registrar_.Add(&observer_,
                   chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED,
                   NotificationService::AllSources());
  }
};

class MockPersonalDataManagerObserver : public PersonalDataManagerObserver {
 public:
  MOCK_METHOD0(OnPersonalDataChanged, void());
};

}  // namespace

namespace autofill_helper {

AutofillProfile CreateAutofillProfile(ProfileType type) {
  AutofillProfile profile;
  switch (type) {
    case PROFILE_MARION:
      autofill_test::SetProfileInfoWithGuid(&profile,
          "C837507A-6C3B-4872-AC14-5113F157D668",
          "Marion", "Mitchell", "Morrison",
          "johnwayne@me.xyz", "Fox",
          "123 Zoo St.", "unit 5", "Hollywood", "CA",
          "91601", "US", "12345678910");
      break;
    case PROFILE_HOMER:
      autofill_test::SetProfileInfoWithGuid(&profile,
          "137DE1C3-6A30-4571-AC86-109B1ECFBE7F",
          "Homer", "J.", "Simpson",
          "homer@abc.com", "SNPP",
          "1 Main St", "PO Box 1", "Springfield", "MA",
          "94101", "US", "14155551212");
      break;
    case PROFILE_FRASIER:
      autofill_test::SetProfileInfoWithGuid(&profile,
          "9A5E6872-6198-4688-BF75-0016E781BB0A",
          "Frasier", "Winslow", "Crane",
          "", "randomness", "", "Apt. 4", "Seattle", "WA",
          "99121", "US", "0000000000");
      break;
    case PROFILE_NULL:
      autofill_test::SetProfileInfoWithGuid(&profile,
          "FE461507-7E13-4198-8E66-74C7DB6D8322",
          "", "", "", "", "", "", "", "", "", "", "", "");
      break;
  }
  return profile;
}

WebDataService* GetWebDataService(int index) {
  return test()->GetProfile(index)->GetWebDataService(Profile::EXPLICIT_ACCESS);
}

PersonalDataManager* GetPersonalDataManager(int index) {
  return PersonalDataManagerFactory::GetForProfile(test()->GetProfile(index));
}

void AddKeys(int profile, const std::set<AutofillKey>& keys) {
  std::vector<webkit_glue::FormField> form_fields;
  for (std::set<AutofillKey>::const_iterator i = keys.begin();
       i != keys.end();
       ++i) {
    webkit_glue::FormField field;
    field.name = i->name();
    field.value = i->value();
    form_fields.push_back(field);
  }

  WaitableEvent done_event(false, false);
  scoped_refptr<AutofillDBThreadObserverHelper> observer_helper(
      new AutofillDBThreadObserverHelper());
  observer_helper->Init();

  EXPECT_CALL(*observer_helper->observer(), Observe(_, _, _)).
      WillOnce(SignalEvent(&done_event));
  WebDataService* wds = GetWebDataService(profile);
  wds->AddFormFields(form_fields);
  done_event.Wait();
}

void RemoveKey(int profile, const AutofillKey& key) {
  WaitableEvent done_event(false, false);
  scoped_refptr<AutofillDBThreadObserverHelper> observer_helper(
      new AutofillDBThreadObserverHelper());
  observer_helper->Init();

  EXPECT_CALL(*observer_helper->observer(), Observe(_, _, _)).
      WillOnce(SignalEvent(&done_event));
  WebDataService* wds = GetWebDataService(profile);
  wds->RemoveFormValueForElementName(key.name(), key.value());
  done_event.Wait();
}

std::set<AutofillEntry> GetAllKeys(int profile) {
  WebDataService* wds = GetWebDataService(profile);
  scoped_refptr<GetAllAutofillEntries> get_all_entries =
      new GetAllAutofillEntries(wds);
  get_all_entries->Init();
  const std::vector<AutofillEntry>& all_entries = get_all_entries->entries();
  std::set<AutofillEntry> all_keys;
  for (std::vector<AutofillEntry>::const_iterator it = all_entries.begin();
       it != all_entries.end(); ++it) {
    all_keys.insert(*it);
  }
  return all_keys;
}

bool KeysMatch(int profile_a, int profile_b) {
  return GetAllKeys(profile_a) == GetAllKeys(profile_b);
}

void SetProfiles(int profile, std::vector<AutofillProfile>* autofill_profiles) {
  MockPersonalDataManagerObserver observer;
  EXPECT_CALL(observer, OnPersonalDataChanged()).
      WillOnce(QuitUIMessageLoop());
  PersonalDataManager* pdm = GetPersonalDataManager(profile);
  pdm->SetObserver(&observer);
  pdm->SetProfiles(autofill_profiles);
  MessageLoop::current()->Run();
  pdm->RemoveObserver(&observer);
}

void AddProfile(int profile, const AutofillProfile& autofill_profile) {
  const std::vector<AutofillProfile*>& all_profiles = GetAllProfiles(profile);
  std::vector<AutofillProfile> autofill_profiles;
  for (size_t i = 0; i < all_profiles.size(); ++i)
    autofill_profiles.push_back(*all_profiles[i]);
  autofill_profiles.push_back(autofill_profile);
  autofill_helper::SetProfiles(profile, &autofill_profiles);
}

void RemoveProfile(int profile, const std::string& guid) {
  const std::vector<AutofillProfile*>& all_profiles = GetAllProfiles(profile);
  std::vector<AutofillProfile> autofill_profiles;
  for (size_t i = 0; i < all_profiles.size(); ++i) {
    if (all_profiles[i]->guid() != guid)
      autofill_profiles.push_back(*all_profiles[i]);
  }
  autofill_helper::SetProfiles(profile, &autofill_profiles);
}

void UpdateProfile(int profile,
                   const std::string& guid,
                   const AutofillType& type,
                   const string16& value) {
  const std::vector<AutofillProfile*>& all_profiles = GetAllProfiles(profile);
  std::vector<AutofillProfile> profiles;
  for (size_t i = 0; i < all_profiles.size(); ++i) {
    profiles.push_back(*all_profiles[i]);
    if (all_profiles[i]->guid() == guid)
      profiles.back().SetInfo(type.field_type(), value);
  }
  autofill_helper::SetProfiles(profile, &profiles);
}

const std::vector<AutofillProfile*>& GetAllProfiles(
    int profile) {
  MockPersonalDataManagerObserver observer;
  EXPECT_CALL(observer, OnPersonalDataChanged()).
      WillOnce(QuitUIMessageLoop());
  PersonalDataManager* pdm = GetPersonalDataManager(profile);
  pdm->SetObserver(&observer);
  pdm->Refresh();
  MessageLoop::current()->Run();
  pdm->RemoveObserver(&observer);
  return pdm->web_profiles();
}

int GetProfileCount(int profile) {
  return GetAllProfiles(profile).size();
}

int GetKeyCount(int profile) {
  return GetAllKeys(profile).size();
}

bool ProfilesMatch(int profile_a, int profile_b) {
  const std::vector<AutofillProfile*>& autofill_profiles_a =
      GetAllProfiles(profile_a);
  std::map<std::string, AutofillProfile> autofill_profiles_a_map;
  for (size_t i = 0; i < autofill_profiles_a.size(); ++i) {
    const AutofillProfile* p = autofill_profiles_a[i];
    autofill_profiles_a_map[p->guid()] = *p;
  }

  const std::vector<AutofillProfile*>& autofill_profiles_b =
      GetAllProfiles(profile_b);
  for (size_t i = 0; i < autofill_profiles_b.size(); ++i) {
    const AutofillProfile* p = autofill_profiles_b[i];
    if (!autofill_profiles_a_map.count(p->guid())) {
      LOG(ERROR) << "GUID " << p->guid() << " not found in profile "
                 << profile_b << ".";
      return false;
    }
    AutofillProfile* expected_profile = &autofill_profiles_a_map[p->guid()];
    expected_profile->set_guid(p->guid());
    if (*expected_profile != *p) {
      LOG(ERROR) << "Mismatch in profile with GUID " << p->guid() << ".";
      return false;
    }
    autofill_profiles_a_map.erase(p->guid());
  }

  if (autofill_profiles_a_map.size()) {
    LOG(ERROR) << "Entries present in Profile " << profile_a
               << " but not in " << profile_b << ".";
    return false;
  }
  return true;
}

bool AllProfilesMatch() {
  for (int i = 1; i < test()->num_clients(); ++i) {
    if (!ProfilesMatch(0, i)) {
      LOG(ERROR) << "Profile " << i << "does not contain the same autofill "
                                       "profiles as profile 0.";
      return false;
    }
  }
  return true;
}

}  // namespace autofill_helper
