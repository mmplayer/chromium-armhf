// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"

namespace {

const char kNoServer[] = "";
const char kNoBypass[] = "";
const char kNoPac[] = "";

}  // namespace

class ProxySettingsApiTest : public ExtensionApiTest {
 protected:
  void ValidateSettings(int expected_mode,
                        const std::string& expected_server,
                        const std::string& bypass,
                        const std::string& expected_pac_url,
                        PrefService* pref_service) {
    const PrefService::Preference* pref =
        pref_service->FindPreference(prefs::kProxy);
    ASSERT_TRUE(pref != NULL);
    EXPECT_TRUE(pref->IsExtensionControlled());

    ProxyConfigDictionary dict(pref_service->GetDictionary(prefs::kProxy));

    ProxyPrefs::ProxyMode mode;
    ASSERT_TRUE(dict.GetMode(&mode));
    EXPECT_EQ(expected_mode, mode);

    std::string value;
    if (!bypass.empty()) {
       ASSERT_TRUE(dict.GetBypassList(&value));
       EXPECT_EQ(bypass, value);
     } else {
       EXPECT_FALSE(dict.GetBypassList(&value));
     }

    if (!expected_pac_url.empty()) {
       ASSERT_TRUE(dict.GetPacUrl(&value));
       EXPECT_EQ(expected_pac_url, value);
     } else {
       EXPECT_FALSE(dict.GetPacUrl(&value));
     }

    if (!expected_server.empty()) {
      ASSERT_TRUE(dict.GetProxyServer(&value));
      EXPECT_EQ(expected_server, value);
    } else {
      EXPECT_FALSE(dict.GetProxyServer(&value));
    }
  }

  void ExpectNoSettings(PrefService* pref_service) {
    const PrefService::Preference* pref =
        pref_service->FindPreference(prefs::kProxy);
    ASSERT_TRUE(pref != NULL);
    EXPECT_FALSE(pref->IsExtensionControlled());
  }
};

// Tests direct connection settings.
// Disabled due to http://crbug.com/88937.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, DISABLED_ProxyDirectSettings) {
  ASSERT_TRUE(RunExtensionTestIncognito("proxy/direct")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_DIRECT, kNoServer, kNoBypass, kNoPac,
                   pref_service);
}

// Tests auto-detect settings.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyAutoSettings) {
  ASSERT_TRUE(RunExtensionTestIncognito("proxy/auto")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_AUTO_DETECT, kNoServer, kNoBypass, kNoPac,
                   pref_service);
}

// Tests PAC proxy settings.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyPacScript) {
  ASSERT_TRUE(RunExtensionTest("proxy/pac")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_PAC_SCRIPT, kNoServer, kNoBypass,
                   "http://wpad/windows.pac", pref_service);
}

// Tests PAC proxy settings.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyPacData) {
  ASSERT_TRUE(RunExtensionTest("proxy/pacdata")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);
  const char url[] =
      "data:application/x-ns-proxy-autoconfig;base64,ZnVuY3Rpb24gRmluZFByb3h5R"
      "m9yVVJMKHVybCwgaG9zdCkgewogIGlmIChob3N0ID09ICdmb29iYXIuY29tJykKICAgIHJl"
      "dHVybiAnUFJPWFkgYmxhY2tob2xlOjgwJzsKICByZXR1cm4gJ0RJUkVDVCc7Cn0=";
  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_PAC_SCRIPT, kNoServer, kNoBypass,
                   url, pref_service);
}

// Tests setting a single proxy to cover all schemes.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyFixedSingle) {
  ASSERT_TRUE(RunExtensionTest("proxy/single")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                 "127.0.0.1:100",
                 kNoBypass,
                 kNoPac,
                 pref_service);
}

// Tests setting to use the system's proxy settings.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxySystem) {
  ASSERT_TRUE(RunExtensionTest("proxy/system")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_SYSTEM, kNoServer, kNoBypass, kNoPac,
                   pref_service);
}

// Tests setting separate proxies for each scheme.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyFixedIndividual) {
  ASSERT_TRUE(RunExtensionTestIncognito("proxy/individual")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                   "http=1.1.1.1:80;"  // http:// is pruned.
                       "https=2.2.2.2:80;"  // http:// is pruned.
                       "ftp=3.3.3.3:9000;"  // http:// is pruned.
                       "socks=socks4://4.4.4.4:9090",
                   kNoBypass,
                   kNoPac,
                   pref_service);

  // Now check the incognito preferences.
  pref_service = browser()->profile()->GetOffTheRecordProfile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                   "http=1.1.1.1:80;"
                       "https=2.2.2.2:80;"
                       "ftp=3.3.3.3:9000;"
                       "socks=socks4://4.4.4.4:9090",
                   kNoBypass,
                   kNoPac,
                   pref_service);
}

// Tests setting values only for incognito mode
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest,
                       ProxyFixedIndividualIncognitoOnly) {
  ASSERT_TRUE(RunExtensionTestIncognito("proxy/individual_incognito_only")) <<
      message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ExpectNoSettings(pref_service);

  // Now check the incognito preferences.
  pref_service = browser()->profile()->GetOffTheRecordProfile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                   "http=1.1.1.1:80;"
                       "https=socks5://2.2.2.2:1080;"
                       "ftp=3.3.3.3:9000;"
                       "socks=socks4://4.4.4.4:9090",
                   kNoBypass,
                   kNoPac,
                   pref_service);
}

// Tests setting values also for incognito mode
// Test disabled due to http://crbug.com/88972.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest,
                       DISABLED_ProxyFixedIndividualIncognitoAlso) {
  ASSERT_TRUE(RunExtensionTestIncognito("proxy/individual_incognito_also")) <<
      message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                   "http=1.1.1.1:80;"
                       "https=socks5://2.2.2.2:1080;"
                       "ftp=3.3.3.3:9000;"
                       "socks=socks4://4.4.4.4:9090",
                   kNoBypass,
                   kNoPac,
                   pref_service);

  // Now check the incognito preferences.
  pref_service = browser()->profile()->GetOffTheRecordProfile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                   "http=5.5.5.5:80;"
                       "https=socks5://6.6.6.6:1080;"
                       "ftp=7.7.7.7:9000;"
                       "socks=socks4://8.8.8.8:9090",
                   kNoBypass,
                   kNoPac,
                   pref_service);
}

// Tests setting and unsetting values
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyFixedIndividualRemove) {
  ASSERT_TRUE(RunExtensionTest("proxy/individual_remove")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ExpectNoSettings(pref_service);
}

IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest,
    ProxyBypass) {
  ASSERT_TRUE(RunExtensionTestIncognito("proxy/bypass")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                   "http=1.1.1.1:80",
                   "localhost,::1,foo.bar,<local>",
                   kNoPac,
                   pref_service);

  // Now check the incognito preferences.
  pref_service = browser()->profile()->GetOffTheRecordProfile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                   "http=1.1.1.1:80",
                   "localhost,::1,foo.bar,<local>",
                   kNoPac,
                   pref_service);
}

// Tests error events: invalid proxy
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyEventsInvalidProxy) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(
      RunExtensionSubtest("proxy/events", "invalid_proxy.html")) << message_;
}

// Tests error events: PAC script parse error.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyEventsParseError) {
  ASSERT_TRUE(
      RunExtensionSubtest("proxy/events", "parse_error.html")) << message_;
}
