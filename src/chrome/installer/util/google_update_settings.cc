// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_settings.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/win/registry.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/product.h"

using base::win::RegKey;
using installer::InstallationState;

namespace {

const wchar_t kGoogleUpdatePoliciesKey[] =
    L"SOFTWARE\\Policies\\Google\\Update";
const wchar_t kGoogleUpdateUpdatePolicyValue[] = L"UpdateDefault";
const wchar_t kGoogleUpdateUpdateOverrideValuePrefix[] = L"Update";
const GoogleUpdateSettings::UpdatePolicy kGoogleUpdateDefaultUpdatePolicy =
#if defined(GOOGLE_CHROME_BUILD)
    GoogleUpdateSettings::AUTOMATIC_UPDATES;
#else
    GoogleUpdateSettings::UPDATES_DISABLED;
#endif

// An list of search results in increasing order of desirability.
enum EulaSearchResult {
  NO_SETTING,
  FOUND_CLIENT_STATE,
  FOUND_OPPOSITE_SETTING,
  FOUND_SAME_SETTING
};

bool ReadGoogleUpdateStrKey(const wchar_t* const name, std::wstring* value) {
  // The registry functions below will end up going to disk.  Do this on another
  // thread to avoid slowing the IO thread.  http://crbug.com/62121
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring reg_path = dist->GetStateKey();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ);
  if (key.ReadValue(name, value) != ERROR_SUCCESS) {
    RegKey hklm_key(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ);
    return (hklm_key.ReadValue(name, value) == ERROR_SUCCESS);
  }
  return true;
}

bool WriteGoogleUpdateStrKeyInternal(BrowserDistribution* dist,
                                     const wchar_t* const name,
                                     const std::wstring& value) {
  DCHECK(dist);
  std::wstring reg_path(dist->GetStateKey());
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_SET_VALUE);
  return (key.WriteValue(name, value.c_str()) == ERROR_SUCCESS);
}

bool WriteGoogleUpdateStrKey(const wchar_t* const name,
                             const std::wstring& value) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  return WriteGoogleUpdateStrKeyInternal(dist, name, value);
}

bool WriteGoogleUpdateStrKeyMultiInstall(const wchar_t* const name,
                                         const std::wstring& value,
                                         bool system_level) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  bool result = WriteGoogleUpdateStrKeyInternal(dist, name, value);
  if (!InstallUtil::IsMultiInstall(dist, system_level))
    return result;
  // It is a multi-install distro. Must write the reg value again.
  BrowserDistribution* multi_dist =
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_BINARIES);
  return WriteGoogleUpdateStrKeyInternal(multi_dist, name, value) && result;
}

bool ClearGoogleUpdateStrKey(const wchar_t* const name) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring reg_path = dist->GetStateKey();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ | KEY_WRITE);
  std::wstring value;
  if (key.ReadValue(name, &value) != ERROR_SUCCESS)
    return false;
  return (key.WriteValue(name, L"") == ERROR_SUCCESS);
}

bool RemoveGoogleUpdateStrKey(const wchar_t* const name) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring reg_path = dist->GetStateKey();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ | KEY_WRITE);
  if (!key.ValueExists(name))
    return true;
  return (key.DeleteValue(name) == ERROR_SUCCESS);
}

EulaSearchResult HasEULASetting(HKEY root, const std::wstring& state_key,
                                bool setting) {
  RegKey key;
  DWORD previous_value = setting ? 1 : 0;
  if (key.Open(root, state_key.c_str(), KEY_QUERY_VALUE) != ERROR_SUCCESS)
    return NO_SETTING;
  if (key.ReadValueDW(google_update::kRegEULAAceptedField,
                      &previous_value) != ERROR_SUCCESS)
    return FOUND_CLIENT_STATE;

  return ((previous_value != 0) == setting) ?
      FOUND_SAME_SETTING : FOUND_OPPOSITE_SETTING;
}

bool GetChromeChannelInternal(bool system_install,
                              bool add_multi_modifier,
                              std::wstring* channel) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (dist->GetChromeChannel(channel)) {
    return true;
  }

  HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  std::wstring reg_path = dist->GetStateKey();
  RegKey key(root_key, reg_path.c_str(), KEY_READ);

  installer::ChannelInfo channel_info;
  if (!channel_info.Initialize(key)) {
    channel->assign(L"unknown");
    return false;
  }

  if (!channel_info.GetChannelName(channel)) {
    channel->assign(L"unknown");
  }

  // Tag the channel name if this is a multi-install.
  if (add_multi_modifier && channel_info.IsMultiInstall()) {
    if (!channel->empty()) {
      channel->append(1, L'-');
    }
    channel->append(1, L'm');
  }

  return true;
}

// Populates |update_policy| with the UpdatePolicy enum value corresponding to a
// DWORD read from the registry and returns true if |value| is within range.
// If |value| is out of range, returns false without modifying |update_policy|.
bool GetUpdatePolicyFromDword(
    const DWORD value,
    GoogleUpdateSettings::UpdatePolicy* update_policy) {
  switch (value) {
    case GoogleUpdateSettings::UPDATES_DISABLED:
    case GoogleUpdateSettings::AUTOMATIC_UPDATES:
    case GoogleUpdateSettings::MANUAL_UPDATES_ONLY:
      *update_policy = static_cast<GoogleUpdateSettings::UpdatePolicy>(value);
      return true;
    default:
      LOG(WARNING) << "Unexpected update policy override value: " << value;
  }
  return false;
}

}  // namespace

// Older versions of Chrome unconditionally read from HKCU\...\ClientState\...
// and then HKLM\...\ClientState\....  This means that system-level Chrome
// never checked ClientStateMedium (which has priority according to Google
// Update) and gave preference to a value in HKCU (which was never checked by
// Google Update).  From now on, Chrome follows Google Update's policy.
bool GoogleUpdateSettings::GetCollectStatsConsent() {
  // Determine whether this is a system-level or a user-level install.
  bool system_install = false;
  FilePath module_dir;
  if (!PathService::Get(base::DIR_MODULE, &module_dir)) {
    LOG(WARNING)
        << "Failed to get directory of module; assuming per-user install.";
  } else {
    system_install = !InstallUtil::IsPerUserInstall(module_dir.value().c_str());
  }
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();

  // Consent applies to all products in a multi-install package.
  if (InstallUtil::IsMultiInstall(dist, system_install)) {
    dist = BrowserDistribution::GetSpecificDistribution(
        BrowserDistribution::CHROME_BINARIES);
  }

  RegKey key;
  DWORD value = 0;
  bool have_value = false;

  // For system-level installs, try ClientStateMedium first.
  have_value =
      system_install &&
      key.Open(HKEY_LOCAL_MACHINE, dist->GetStateMediumKey().c_str(),
               KEY_QUERY_VALUE) == ERROR_SUCCESS &&
      key.ReadValueDW(google_update::kRegUsageStatsField,
                      &value) == ERROR_SUCCESS;

  // Otherwise, try ClientState.
  have_value =
      !have_value &&
      key.Open(system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
               dist->GetStateKey().c_str(), KEY_QUERY_VALUE) == ERROR_SUCCESS &&
      key.ReadValueDW(google_update::kRegUsageStatsField,
                      &value) == ERROR_SUCCESS;

  // Google Update specifically checks that the value is 1, so we do the same.
  return have_value && value == 1;
}

bool GoogleUpdateSettings::SetCollectStatsConsent(bool consented) {
  // Google Update writes and expects 1 for true, 0 for false.
  DWORD value = consented ? 1 : 0;

  // Determine whether this is a system-level or a user-level install.
  bool system_install = false;
  FilePath module_dir;
  if (!PathService::Get(base::DIR_MODULE, &module_dir)) {
    LOG(WARNING)
        << "Failed to get directory of module; assuming per-user install.";
  } else {
    system_install = !InstallUtil::IsPerUserInstall(module_dir.value().c_str());
  }
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();

  // Consent applies to all products in a multi-install package.
  if (InstallUtil::IsMultiInstall(dist, system_install)) {
    dist = BrowserDistribution::GetSpecificDistribution(
        BrowserDistribution::CHROME_BINARIES);
  }

  // Write to ClientStateMedium for system-level; ClientState otherwise.
  HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  std::wstring reg_path =
      system_install ? dist->GetStateMediumKey() : dist->GetStateKey();
  RegKey key;
  LONG result = key.Create(root_key, reg_path.c_str(), KEY_SET_VALUE);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed opening key " << reg_path << " to set "
               << google_update::kRegUsageStatsField << "; result: " << result;
  } else {
    result = key.WriteValue(google_update::kRegUsageStatsField, value);
    LOG_IF(ERROR, result != ERROR_SUCCESS) << "Failed setting "
        << google_update::kRegUsageStatsField << " in key " << reg_path
        << "; result: " << result;
  }
  return (result == ERROR_SUCCESS);
}

bool GoogleUpdateSettings::GetMetricsId(std::wstring* metrics_id) {
  return ReadGoogleUpdateStrKey(google_update::kRegMetricsId, metrics_id);
}

bool GoogleUpdateSettings::SetMetricsId(const std::wstring& metrics_id) {
  return WriteGoogleUpdateStrKey(google_update::kRegMetricsId, metrics_id);
}

// EULA consent is only relevant for system-level installs.
bool GoogleUpdateSettings::SetEULAConsent(
    const InstallationState& machine_state,
    BrowserDistribution* dist,
    bool consented) {
  DCHECK(dist);
  const DWORD eula_accepted = consented ? 1 : 0;
  std::wstring reg_path = dist->GetStateMediumKey();
  bool succeeded = true;
  RegKey key;

  // Write the consent value into the product's ClientStateMedium key.
  if (key.Create(HKEY_LOCAL_MACHINE, reg_path.c_str(),
                 KEY_SET_VALUE) != ERROR_SUCCESS ||
      key.WriteValue(google_update::kRegEULAAceptedField,
                     eula_accepted) != ERROR_SUCCESS) {
    succeeded = false;
  }

  // If this is a multi-install, also write it into the binaries' key.
  // --mutli-install is not provided on the command-line, so deduce it from
  // the product's state.
  const installer::ProductState* product_state =
      machine_state.GetProductState(true, dist->GetType());
  if (product_state != NULL && product_state->is_multi_install()) {
    dist = BrowserDistribution::GetSpecificDistribution(
        BrowserDistribution::CHROME_BINARIES);
    reg_path = dist->GetStateMediumKey();
    if (key.Create(HKEY_LOCAL_MACHINE, reg_path.c_str(),
                   KEY_SET_VALUE) != ERROR_SUCCESS ||
        key.WriteValue(google_update::kRegEULAAceptedField,
                       eula_accepted) != ERROR_SUCCESS) {
        succeeded = false;
    }
  }

  return succeeded;
}

int GoogleUpdateSettings::GetLastRunTime() {
  std::wstring time_s;
  if (!ReadGoogleUpdateStrKey(google_update::kRegLastRunTimeField, &time_s))
    return -1;
  int64 time_i;
  if (!base::StringToInt64(time_s, &time_i))
    return -1;
  base::TimeDelta td =
      base::Time::NowFromSystemTime() - base::Time::FromInternalValue(time_i);
  return td.InDays();
}

bool GoogleUpdateSettings::SetLastRunTime() {
  int64 time = base::Time::NowFromSystemTime().ToInternalValue();
  return WriteGoogleUpdateStrKey(google_update::kRegLastRunTimeField,
                                 base::Int64ToString16(time));
}

bool GoogleUpdateSettings::RemoveLastRunTime() {
  return RemoveGoogleUpdateStrKey(google_update::kRegLastRunTimeField);
}

bool GoogleUpdateSettings::GetBrowser(std::wstring* browser) {
  return ReadGoogleUpdateStrKey(google_update::kRegBrowserField, browser);
}

bool GoogleUpdateSettings::GetLanguage(std::wstring* language) {
  return ReadGoogleUpdateStrKey(google_update::kRegLangField, language);
}

bool GoogleUpdateSettings::GetBrand(std::wstring* brand) {
  return ReadGoogleUpdateStrKey(google_update::kRegRLZBrandField, brand);
}

bool GoogleUpdateSettings::GetReactivationBrand(std::wstring* brand) {
  return ReadGoogleUpdateStrKey(google_update::kRegRLZReactivationBrandField,
                                brand);
}

bool GoogleUpdateSettings::GetClient(std::wstring* client) {
  return ReadGoogleUpdateStrKey(google_update::kRegClientField, client);
}

bool GoogleUpdateSettings::SetClient(const std::wstring& client) {
  return WriteGoogleUpdateStrKey(google_update::kRegClientField, client);
}

bool GoogleUpdateSettings::GetReferral(std::wstring* referral) {
  return ReadGoogleUpdateStrKey(google_update::kRegReferralField, referral);
}

bool GoogleUpdateSettings::ClearReferral() {
  return ClearGoogleUpdateStrKey(google_update::kRegReferralField);
}

bool GoogleUpdateSettings::UpdateDidRunState(bool did_run,
                                             bool system_level) {
  return WriteGoogleUpdateStrKeyMultiInstall(google_update::kRegDidRunField,
                                             did_run ? L"1" : L"0",
                                             system_level);
}

std::wstring GoogleUpdateSettings::GetChromeChannel(bool system_install) {
  std::wstring channel;
  GetChromeChannelInternal(system_install, false, &channel);
  return channel;
}

bool GoogleUpdateSettings::GetChromeChannelAndModifiers(bool system_install,
                                                        std::wstring* channel) {
  return GetChromeChannelInternal(system_install, true, channel);
}

void GoogleUpdateSettings::UpdateInstallStatus(bool system_install,
    installer::ArchiveType archive_type, int install_return_code,
    const std::wstring& product_guid) {
  DCHECK(archive_type != installer::UNKNOWN_ARCHIVE_TYPE ||
         install_return_code != 0);
  HKEY reg_root = (system_install) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  RegKey key;
  installer::ChannelInfo channel_info;
  std::wstring reg_key(google_update::kRegPathClientState);
  reg_key.append(L"\\");
  reg_key.append(product_guid);
  LONG result = key.Open(reg_root, reg_key.c_str(),
                         KEY_QUERY_VALUE | KEY_SET_VALUE);
  if (result == ERROR_SUCCESS)
    channel_info.Initialize(key);
  else if (result != ERROR_FILE_NOT_FOUND)
    LOG(ERROR) << "Failed to open " << reg_key << "; Error: " << result;

  if (UpdateGoogleUpdateApKey(archive_type, install_return_code,
                              &channel_info)) {
    // We have a modified channel_info value to write.
    // Create the app's ClientState key if it doesn't already exist.
    if (!key.Valid()) {
      result = key.Open(reg_root, google_update::kRegPathClientState,
                        KEY_CREATE_SUB_KEY);
      if (result == ERROR_SUCCESS)
        result = key.CreateKey(product_guid.c_str(), KEY_SET_VALUE);

      if (result != ERROR_SUCCESS) {
        LOG(ERROR) << "Failed to create " << reg_key << "; Error: " << result;
        return;
      }
    }
    if (!channel_info.Write(&key)) {
      LOG(ERROR) << "Failed to write to application's ClientState key "
                 << google_update::kRegApField << " = " << channel_info.value();
    }
  }
}

bool GoogleUpdateSettings::UpdateGoogleUpdateApKey(
    installer::ArchiveType archive_type, int install_return_code,
    installer::ChannelInfo* value) {
  DCHECK(archive_type != installer::UNKNOWN_ARCHIVE_TYPE ||
         install_return_code != 0);
  bool modified = false;

  if (archive_type == installer::FULL_ARCHIVE_TYPE || !install_return_code) {
    if (value->SetFullSuffix(false)) {
      VLOG(1) << "Removed incremental installer failure key; "
                 "switching to channel: "
              << value->value();
      modified = true;
    }
  } else if (archive_type == installer::INCREMENTAL_ARCHIVE_TYPE) {
    if (value->SetFullSuffix(true)) {
      VLOG(1) << "Incremental installer failed; switching to channel: "
              << value->value();
      modified = true;
    } else {
      VLOG(1) << "Incremental installer failure; already on channel: "
              << value->value();
    }
  } else {
    // It's okay if we don't know the archive type.  In this case, leave the
    // "-full" suffix as we found it.
    DCHECK_EQ(installer::UNKNOWN_ARCHIVE_TYPE, archive_type);
  }

  if (value->SetMultiFailSuffix(false)) {
    VLOG(1) << "Removed multi-install failure key; switching to channel: "
            << value->value();
    modified = true;
  }

  return modified;
}

int GoogleUpdateSettings::DuplicateGoogleUpdateSystemClientKey() {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring reg_path = dist->GetStateKey();

  // Minimum access needed is to be able to write to this key.
  RegKey reg_key(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_SET_VALUE);
  if (!reg_key.Valid())
    return 0;

  HANDLE target_handle = 0;
  if (!DuplicateHandle(GetCurrentProcess(), reg_key.Handle(),
                       GetCurrentProcess(), &target_handle, KEY_SET_VALUE,
                       TRUE, DUPLICATE_SAME_ACCESS)) {
    return 0;
  }
  return reinterpret_cast<int>(target_handle);
}

bool GoogleUpdateSettings::WriteGoogleUpdateSystemClientKey(
    int handle, const std::wstring& key, const std::wstring& value) {
  HKEY reg_key = reinterpret_cast<HKEY>(reinterpret_cast<void*>(handle));
  DWORD size = static_cast<DWORD>(value.size()) * sizeof(wchar_t);
  LSTATUS status = RegSetValueEx(reg_key, key.c_str(), 0, REG_SZ,
      reinterpret_cast<const BYTE*>(value.c_str()), size);
  return status == ERROR_SUCCESS;
}

GoogleUpdateSettings::UpdatePolicy GoogleUpdateSettings::GetAppUpdatePolicy(
    const std::wstring& app_guid,
    bool* is_overridden) {
  bool found_override = false;
  UpdatePolicy update_policy = kGoogleUpdateDefaultUpdatePolicy;

#if defined(GOOGLE_CHROME_BUILD)
  DCHECK(!app_guid.empty());
  RegKey policy_key;

  // Google Update Group Policy settings are always in HKLM.
  if (policy_key.Open(HKEY_LOCAL_MACHINE, kGoogleUpdatePoliciesKey,
                      KEY_QUERY_VALUE) == ERROR_SUCCESS) {
    static const size_t kPrefixLen =
        arraysize(kGoogleUpdateUpdateOverrideValuePrefix) - 1;
    DWORD value;
    std::wstring app_update_override;
    app_update_override.reserve(kPrefixLen + app_guid.size());
    app_update_override.append(kGoogleUpdateUpdateOverrideValuePrefix,
                               kPrefixLen);
    app_update_override.append(app_guid);
    // First try to read and comprehend the app-specific override.
    found_override = (policy_key.ReadValueDW(app_update_override.c_str(),
                                             &value) == ERROR_SUCCESS &&
                      GetUpdatePolicyFromDword(value, &update_policy));

    // Failing that, try to read and comprehend the default override.
    if (!found_override &&
        policy_key.ReadValueDW(kGoogleUpdateUpdatePolicyValue,
                               &value) == ERROR_SUCCESS) {
      GetUpdatePolicyFromDword(value, &update_policy);
    }
  }
#endif  // defined(GOOGLE_CHROME_BUILD)

  if (is_overridden != NULL)
    *is_overridden = found_override;

  return update_policy;
}
