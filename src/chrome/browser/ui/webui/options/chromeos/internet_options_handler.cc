// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/internet_options_handler.h"

#include <ctype.h>

#include <map>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/choose_mobile_network_dialog.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/mobile_config.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/sim_dialog_delegate.h"
#include "chrome/browser/chromeos/status/network_menu_icon.h"
#include "chrome/browser/chromeos/user_cros_settings_provider.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/time_format.h"
#include "content/common/notification_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/widget/widget.h"

static const char kOtherNetworksFakePath[] = "?";

InternetOptionsHandler::InternetOptionsHandler()
    : chromeos::CrosOptionsPageUIHandler(NULL),
      proxy_settings_(NULL) {
  registrar_.Add(this, chrome::NOTIFICATION_REQUIRE_PIN_SETTING_CHANGE_ENDED,
      NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_ENTER_PIN_ENDED,
      NotificationService::AllSources());
  cros_ = chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  if (cros_) {
    cros_->AddNetworkManagerObserver(this);
    cros_->AddCellularDataPlanObserver(this);
    MonitorNetworks();
  }
}

InternetOptionsHandler::~InternetOptionsHandler() {
  if (cros_) {
    cros_->RemoveNetworkManagerObserver(this);
    cros_->RemoveCellularDataPlanObserver(this);
    cros_->RemoveObserverForAllNetworks(this);
  }
}

void InternetOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "internetPage",
                IDS_OPTIONS_INTERNET_TAB_LABEL);

  localized_strings->SetString("wired_title",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_SECTION_TITLE_WIRED_NETWORK));
  localized_strings->SetString("wireless_title",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_SECTION_TITLE_WIRELESS_NETWORK));
  localized_strings->SetString("vpn_title",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_SECTION_TITLE_VIRTUAL_NETWORK));
  localized_strings->SetString("remembered_title",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_SECTION_TITLE_REMEMBERED_NETWORK));

  localized_strings->SetString("connect_button",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_CONNECT));
  localized_strings->SetString("disconnect_button",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_DISCONNECT));
  localized_strings->SetString("options_button",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_OPTIONS));
  localized_strings->SetString("forget_button",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_FORGET));
  localized_strings->SetString("activate_button",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_ACTIVATE));
  localized_strings->SetString("buyplan_button",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_BUY_PLAN));

  localized_strings->SetString("changeProxyButton",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CHANGE_PROXY_BUTTON));
  localized_strings->SetString("enableSharedProxiesHint",
      l10n_util::GetStringFUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_ENABLE_SHARED_PROXIES_HINT,
          l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_USE_SHARED_PROXIES)));
  localized_strings->SetString("policyManagedProxyText",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_POLICY_MANAGED_PROXY_TEXT));
  localized_strings->SetString("extensionManagedProxyText",
      l10n_util::GetStringUTF16(
        IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_EXTENSION_MANAGED_PROXY_TEXT));
  localized_strings->SetString("unmodifiableProxyText",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_UNMODIFIABLE_PROXY_TEXT));

  localized_strings->SetString("wifiNetworkTabLabel",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_TAB_WIFI));
  localized_strings->SetString("vpnTabLabel",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_TAB_VPN));
  localized_strings->SetString("cellularPlanTabLabel",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_TAB_PLAN));
  localized_strings->SetString("cellularConnTabLabel",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_TAB_CONNECTION));
  localized_strings->SetString("cellularDeviceTabLabel",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_TAB_DEVICE));
  localized_strings->SetString("networkTabLabel",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_TAB_NETWORK));
  localized_strings->SetString("securityTabLabel",
        l10n_util::GetStringUTF16(
            IDS_OPTIONS_SETTINGS_INTERNET_TAB_SECURITY));

  localized_strings->SetString("useDHCP",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_USE_DHCP));
  localized_strings->SetString("useStaticIP",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_USE_STATIC_IP));
  localized_strings->SetString("connectionState",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CONNECTION_STATE));
  localized_strings->SetString("inetAddress",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_ADDRESS));
  localized_strings->SetString("inetSubnetAddress",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SUBNETMASK));
  localized_strings->SetString("inetGateway",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_GATEWAY));
  localized_strings->SetString("inetDns",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_DNSSERVER));
  localized_strings->SetString("hardwareAddress",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_HARDWARE_ADDRESS));

  // Wifi Tab.
  localized_strings->SetString("accessLockedMsg",
      l10n_util::GetStringUTF16(
          IDS_STATUSBAR_NETWORK_LOCKED));
  localized_strings->SetString("inetSsid",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_ID));
  localized_strings->SetString("inetPassProtected",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NET_PROTECTED));
  localized_strings->SetString("inetNetworkShared",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_SHARED));
  localized_strings->SetString("inetPreferredNetwork",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PREFER_NETWORK));
  localized_strings->SetString("inetAutoConnectNetwork",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_AUTO_CONNECT));
  localized_strings->SetString("inetLogin",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_LOGIN));
  localized_strings->SetString("inetShowPass",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SHOWPASSWORD));
  localized_strings->SetString("inetPassPrompt",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PASSWORD));
  localized_strings->SetString("inetSsidPrompt",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SSID));
  localized_strings->SetString("inetStatus",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_STATUS_TITLE));
  localized_strings->SetString("inetConnect",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CONNECT_TITLE));

  // VPN Tab.
  localized_strings->SetString("inetServiceName",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_SERVICE_NAME));
  localized_strings->SetString("inetServerHostname",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_SERVER_HOSTNAME));
  localized_strings->SetString("inetProviderType",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_PROVIDER_TYPE));
  localized_strings->SetString("inetUsername",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_USERNAME));

  // Cellular Tab.
  localized_strings->SetString("serviceName",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_SERVICE_NAME));
  localized_strings->SetString("networkTechnology",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_NETWORK_TECHNOLOGY));
  localized_strings->SetString("operatorName",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_OPERATOR));
  localized_strings->SetString("operatorCode",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_OPERATOR_CODE));
  localized_strings->SetString("activationState",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_ACTIVATION_STATE));
  localized_strings->SetString("roamingState",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_ROAMING_STATE));
  localized_strings->SetString("restrictedPool",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_RESTRICTED_POOL));
  localized_strings->SetString("errorState",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_ERROR_STATE));
  localized_strings->SetString("manufacturer",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_MANUFACTURER));
  localized_strings->SetString("modelId",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_MODEL_ID));
  localized_strings->SetString("firmwareRevision",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_FIRMWARE_REVISION));
  localized_strings->SetString("hardwareRevision",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_HARDWARE_REVISION));
  localized_strings->SetString("prlVersion",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_PRL_VERSION));
  localized_strings->SetString("cellularApnLabel",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_APN));
  localized_strings->SetString("cellularApnOther",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_APN_OTHER));
  localized_strings->SetString("cellularApnUsername",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_APN_USERNAME));
  localized_strings->SetString("cellularApnPassword",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_APN_PASSWORD));
  localized_strings->SetString("cellularApnUseDefault",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_APN_CLEAR));
  localized_strings->SetString("cellularApnSet",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_APN_SET));
  localized_strings->SetString("cellularApnCancel",
      l10n_util::GetStringUTF16(
          IDS_CANCEL));

  localized_strings->SetString("accessSecurityTabLink",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_ACCESS_SECURITY_TAB));
  localized_strings->SetString("lockSimCard",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_LOCK_SIM_CARD));
  localized_strings->SetString("changePinButton",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_CHANGE_PIN_BUTTON));

  localized_strings->SetString("planName",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CELL_PLAN_NAME));
  localized_strings->SetString("planLoading",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_LOADING_PLAN));
  localized_strings->SetString("noPlansFound",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NO_PLANS_FOUND));
  localized_strings->SetString("purchaseMore",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PURCHASE_MORE));
  localized_strings->SetString("dataRemaining",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_DATA_REMAINING));
  localized_strings->SetString("planExpires",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_EXPIRES));
  localized_strings->SetString("showPlanNotifications",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SHOW_MOBILE_NOTIFICATION));
  localized_strings->SetString("autoconnectCellular",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_AUTO_CONNECT));
  localized_strings->SetString("customerSupport",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CUSTOMER_SUPPORT));

  localized_strings->SetString("enableWifi",
      l10n_util::GetStringFUTF16(
          IDS_STATUSBAR_NETWORK_DEVICE_ENABLE,
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_WIFI)));
  localized_strings->SetString("disableWifi",
      l10n_util::GetStringFUTF16(
          IDS_STATUSBAR_NETWORK_DEVICE_DISABLE,
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_WIFI)));
  localized_strings->SetString("enableCellular",
      l10n_util::GetStringFUTF16(
          IDS_STATUSBAR_NETWORK_DEVICE_ENABLE,
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_CELLULAR)));
  localized_strings->SetString("disableCellular",
      l10n_util::GetStringFUTF16(
          IDS_STATUSBAR_NETWORK_DEVICE_DISABLE,
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_CELLULAR)));
  localized_strings->SetString("useSharedProxies",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_USE_SHARED_PROXIES));
  localized_strings->SetString("enableDataRoaming",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_ENABLE_DATA_ROAMING));
  localized_strings->SetString("generalNetworkingTitle",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_CONTROL_TITLE));
  localized_strings->SetString("detailsInternetDismiss",
      l10n_util::GetStringUTF16(IDS_CLOSE));
  localized_strings->SetString("ownerOnly", l10n_util::GetStringUTF16(
      IDS_OPTIONS_ACCOUNTS_OWNER_ONLY));
  localized_strings->SetString("ownerUserId", UTF8ToUTF16(
      chromeos::UserCrosSettingsProvider::cached_owner()));

  FillNetworkInfo(localized_strings);
}

void InternetOptionsHandler::Initialize() {
  cros_->RequestNetworkScan();
}

void InternetOptionsHandler::RegisterMessages() {
  // Setup handlers specific to this panel.
  DCHECK(web_ui_);
  web_ui_->RegisterMessageCallback("buttonClickCallback",
      base::Bind(&InternetOptionsHandler::ButtonClickCallback,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("refreshCellularPlan",
      base::Bind(&InternetOptionsHandler::RefreshCellularPlanCallback,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("setPreferNetwork",
      base::Bind(&InternetOptionsHandler::SetPreferNetworkCallback,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("setAutoConnect",
      base::Bind(&InternetOptionsHandler::SetAutoConnectCallback,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("setIPConfig",
      base::Bind(&InternetOptionsHandler::SetIPConfigCallback,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("enableWifi",
      base::Bind(&InternetOptionsHandler::EnableWifiCallback,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("disableWifi",
      base::Bind(&InternetOptionsHandler::DisableWifiCallback,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("enableCellular",
      base::Bind(&InternetOptionsHandler::EnableCellularCallback,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("disableCellular",
      base::Bind(&InternetOptionsHandler::DisableCellularCallback,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("buyDataPlan",
      base::Bind(&InternetOptionsHandler::BuyDataPlanCallback,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("showMorePlanInfo",
      base::Bind(&InternetOptionsHandler::BuyDataPlanCallback,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("setApn",
      base::Bind(&InternetOptionsHandler::SetApnCallback,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("setSimCardLock",
      base::Bind(&InternetOptionsHandler::SetSimCardLockCallback,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("changePin",
      base::Bind(&InternetOptionsHandler::ChangePinCallback,
                 base::Unretained(this)));
}

void InternetOptionsHandler::EnableWifiCallback(const ListValue* args) {
  cros_->EnableWifiNetworkDevice(true);
}

void InternetOptionsHandler::DisableWifiCallback(const ListValue* args) {
  cros_->EnableWifiNetworkDevice(false);
}

void InternetOptionsHandler::EnableCellularCallback(const ListValue* args) {
  const chromeos::NetworkDevice* cellular = cros_->FindCellularDevice();
  if (!cellular) {
    LOG(ERROR) << "Didn't find cellular device, it should have been available.";
    cros_->EnableCellularNetworkDevice(true);
  } else if (cellular->sim_lock_state() == chromeos::SIM_UNLOCKED ||
             cellular->sim_lock_state() == chromeos::SIM_UNKNOWN) {
      cros_->EnableCellularNetworkDevice(true);
  } else {
    chromeos::SimDialogDelegate::ShowDialog(GetNativeWindow(),
        chromeos::SimDialogDelegate::SIM_DIALOG_UNLOCK);
  }
}

void InternetOptionsHandler::DisableCellularCallback(const ListValue* args) {
  cros_->EnableCellularNetworkDevice(false);
}

void InternetOptionsHandler::BuyDataPlanCallback(const ListValue* args) {
  if (!web_ui_)
    return;
  Browser* browser = BrowserList::FindBrowserWithFeature(
      Profile::FromWebUI(web_ui_), Browser::FEATURE_TABSTRIP);
  if (browser)
    browser->OpenMobilePlanTabAndActivate();
}

void InternetOptionsHandler::SetApnCallback(const ListValue* args) {
  std::string service_path;
  std::string apn;
  std::string username;
  std::string password;
  if (args->GetSize() != 4 ||
      !args->GetString(0, &service_path) ||
      !args->GetString(1, &apn) ||
      !args->GetString(2, &username) ||
      !args->GetString(3, &password)) {
    NOTREACHED();
    return;
  }

  chromeos::CellularNetwork* network =
        cros_->FindCellularNetworkByPath(service_path);
  if (network) {
    network->SetApn(chromeos::CellularApn(
        apn, network->apn().network_id, username, password));
  }
}

void InternetOptionsHandler::SetSimCardLockCallback(const ListValue* args) {
  bool require_pin_new_value;
  if (!args->GetBoolean(0, &require_pin_new_value)) {
    NOTREACHED();
    return;
  }
  // 1. Bring up SIM unlock dialog, pass new RequirePin setting in URL.
  // 2. Dialog will ask for current PIN in any case.
  // 3. If card is locked it will first call PIN unlock operation
  // 4. Then it will call Set RequirePin, passing the same PIN.
  // 5. We'll get notified by REQUIRE_PIN_SETTING_CHANGE_ENDED notification.
  chromeos::SimDialogDelegate::SimDialogMode mode;
  if (require_pin_new_value)
    mode = chromeos::SimDialogDelegate::SIM_DIALOG_SET_LOCK_ON;
  else
    mode = chromeos::SimDialogDelegate::SIM_DIALOG_SET_LOCK_OFF;
  chromeos::SimDialogDelegate::ShowDialog(GetNativeWindow(), mode);
}

void InternetOptionsHandler::ChangePinCallback(const ListValue* args) {
  chromeos::SimDialogDelegate::ShowDialog(GetNativeWindow(),
      chromeos::SimDialogDelegate::SIM_DIALOG_CHANGE_PIN);
}

void InternetOptionsHandler::RefreshNetworkData() {
  DictionaryValue dictionary;
  FillNetworkInfo(&dictionary);
  web_ui_->CallJavascriptFunction(
      "options.InternetOptions.refreshNetworkData", dictionary);
}

void InternetOptionsHandler::OnNetworkManagerChanged(
    chromeos::NetworkLibrary* cros) {
  if (!web_ui_)
    return;
  MonitorNetworks();
  RefreshNetworkData();
}

void InternetOptionsHandler::OnNetworkChanged(
    chromeos::NetworkLibrary* cros,
    const chromeos::Network* network) {
  if (web_ui_)
    RefreshNetworkData();
}

// Monitor wireless networks for changes. It is only necessary
// to set up individual observers for the cellular networks
// (if any) and for the connected Wi-Fi network (if any). The
// only change we are interested in for Wi-Fi networks is signal
// strength. For non-connected Wi-Fi networks, all information is
// reported via scan results, which trigger network manager
// updates. Only the connected Wi-Fi network has changes reported
// via service property updates.
void InternetOptionsHandler::MonitorNetworks() {
  cros_->RemoveObserverForAllNetworks(this);
  const chromeos::WifiNetwork* wifi_network = cros_->wifi_network();
  if (wifi_network)
    cros_->AddNetworkObserver(wifi_network->service_path(), this);
  // Always monitor the cellular networks, if any, so that changes
  // in network technology, roaming status, and signal strength
  // will be shown.
  const chromeos::CellularNetworkVector& cell_networks =
      cros_->cellular_networks();
  for (size_t i = 0; i < cell_networks.size(); ++i) {
    chromeos::CellularNetwork* cell_network = cell_networks[i];
    cros_->AddNetworkObserver(cell_network->service_path(), this);
  }
  const chromeos::VirtualNetwork* virtual_network = cros_->virtual_network();
  if (virtual_network)
    cros_->AddNetworkObserver(virtual_network->service_path(), this);
}

void InternetOptionsHandler::OnCellularDataPlanChanged(
    chromeos::NetworkLibrary* cros) {
  if (!web_ui_)
    return;
  const chromeos::CellularNetwork* cellular = cros_->cellular_network();
  if (!cellular)
    return;
  const chromeos::CellularDataPlanVector* plans =
      cros_->GetDataPlans(cellular->service_path());
  DictionaryValue connection_plans;
  ListValue* plan_list = new ListValue();
  if (plans) {
    for (chromeos::CellularDataPlanVector::const_iterator iter = plans->begin();
         iter != plans->end(); ++iter) {
      plan_list->Append(CellularDataPlanToDictionary(*iter));
    }
  }
  connection_plans.SetString("servicePath", cellular->service_path());
  connection_plans.SetBoolean("needsPlan", cellular->needs_new_plan());
  connection_plans.SetBoolean("activated",
      cellular->activation_state() == chromeos::ACTIVATION_STATE_ACTIVATED);
  connection_plans.Set("plans", plan_list);
  SetActivationButtonVisibility(cellular, &connection_plans);
  web_ui_->CallJavascriptFunction(
      "options.InternetOptions.updateCellularPlans", connection_plans);
}


void InternetOptionsHandler::Observe(int type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  chromeos::CrosOptionsPageUIHandler::Observe(type, source, details);
  if (type == chrome::NOTIFICATION_REQUIRE_PIN_SETTING_CHANGE_ENDED) {
    base::FundamentalValue require_pin(*Details<bool>(details).ptr());
    web_ui_->CallJavascriptFunction(
        "options.InternetOptions.updateSecurityTab", require_pin);
  } else if (type == chrome::NOTIFICATION_ENTER_PIN_ENDED) {
    // We make an assumption (which is valid for now) that the SIM
    // unlock dialog is put up only when the user is trying to enable
    // mobile data.
    bool cancelled = *Details<bool>(details).ptr();
    if (cancelled) {
      base::DictionaryValue dictionary;
      FillNetworkInfo(&dictionary);
      web_ui_->CallJavascriptFunction(
          "options.InternetOptions.setupAttributes", dictionary);
    }
    // The case in which the correct PIN was entered and the SIM is
    // now unlocked is handled in NetworkMenuButton.
  }
}

DictionaryValue* InternetOptionsHandler::CellularDataPlanToDictionary(
    const chromeos::CellularDataPlan* plan) {
  DictionaryValue* plan_dict = new DictionaryValue();
  plan_dict->SetInteger("planType", plan->plan_type);
  plan_dict->SetString("name", plan->plan_name);
  plan_dict->SetString("planSummary", plan->GetPlanDesciption());
  plan_dict->SetString("dataRemaining", plan->GetDataRemainingDesciption());
  plan_dict->SetString("planExpires", plan->GetPlanExpiration());
  plan_dict->SetString("warning", plan->GetRemainingWarning());
  return plan_dict;
}

void InternetOptionsHandler::SetPreferNetworkCallback(const ListValue* args) {
  std::string service_path;
  std::string prefer_network_str;

  if (args->GetSize() < 2 ||
      !args->GetString(0, &service_path) ||
      !args->GetString(1, &prefer_network_str)) {
    NOTREACHED();
    return;
  }

  chromeos::Network* network = cros_->FindNetworkByPath(service_path);
  if (!network)
    return;

  bool prefer_network = prefer_network_str == "true";
  if (prefer_network != network->preferred())
    network->SetPreferred(prefer_network);
}

void InternetOptionsHandler::SetAutoConnectCallback(const ListValue* args) {
  std::string service_path;
  std::string auto_connect_str;

  if (args->GetSize() < 2 ||
      !args->GetString(0, &service_path) ||
      !args->GetString(1, &auto_connect_str)) {
    NOTREACHED();
    return;
  }

  chromeos::Network* network = cros_->FindNetworkByPath(service_path);
  if (!network)
    return;

  bool auto_connect = auto_connect_str == "true";
  if (auto_connect != network->auto_connect())
    network->SetAutoConnect(auto_connect);
}

void InternetOptionsHandler::SetIPConfigCallback(const ListValue* args) {
  std::string service_path;
  std::string dhcp_str;
  std::string address;
  std::string netmask;
  std::string gateway;
  std::string name_servers;

  if (args->GetSize() < 6 ||
      !args->GetString(0, &service_path) ||
      !args->GetString(1, &dhcp_str) ||
      !args->GetString(2, &address) ||
      !args->GetString(3, &netmask) ||
      !args->GetString(4, &gateway) ||
      !args->GetString(5, &name_servers)) {
    NOTREACHED();
    return;
  }

  chromeos::Network* network = cros_->FindNetworkByPath(service_path);
  if (!network)
    return;

  cros_->SetIPConfig(chromeos::NetworkIPConfig(network->device_path(),
      dhcp_str == "true" ? chromeos::IPCONFIG_TYPE_DHCP :
                           chromeos::IPCONFIG_TYPE_IPV4,
      address, netmask, gateway, name_servers));
}

void InternetOptionsHandler::PopulateDictionaryDetails(
    const chromeos::Network* network) {
  DCHECK(network);
  bool use_shared_proxies = false;
  if (proxy_settings()) {
    proxy_settings()->SetCurrentNetwork(network->service_path());
    use_shared_proxies = proxy_settings()->IsUsingSharedProxies();
  }
  DictionaryValue dictionary;
  std::string hardware_address;
  chromeos::NetworkIPConfigVector ipconfigs = cros_->GetIPConfigs(
      network->device_path(), &hardware_address,
      chromeos::NetworkLibrary::FORMAT_COLON_SEPARATED_HEX);
  if (!hardware_address.empty())
    dictionary.SetString("hardwareAddress", hardware_address);
  for (chromeos::NetworkIPConfigVector::const_iterator it = ipconfigs.begin();
       it != ipconfigs.end(); ++it) {
    const chromeos::NetworkIPConfig& ipconfig = *it;
    scoped_ptr<DictionaryValue> ipconfig_dict(new DictionaryValue());
    ipconfig_dict->SetString("address", ipconfig.address);
    ipconfig_dict->SetString("subnetAddress", ipconfig.netmask);
    ipconfig_dict->SetString("gateway", ipconfig.gateway);
    ipconfig_dict->SetString("dns", ipconfig.name_servers);
    if (ipconfig.type == chromeos::IPCONFIG_TYPE_DHCP)
      dictionary.Set("ipconfigDHCP", ipconfig_dict.release());
    else if (ipconfig.type == chromeos::IPCONFIG_TYPE_IPV4)
      dictionary.Set("ipconfigStatic", ipconfig_dict.release());
  }

  chromeos::ConnectionType type = network->type();
  dictionary.SetInteger("type", type);
  dictionary.SetString("servicePath", network->service_path());
  dictionary.SetBoolean("connecting", network->connecting());
  dictionary.SetBoolean("connected", network->connected());
  dictionary.SetString("connectionState", network->GetStateString());

  // Determine if proxy is configurable.
  // First check proxy prefs.
  bool proxy_configurable = true;
  std::string change_proxy_text;
  if (web_ui_) {
    const PrefService::Preference* proxy_pref =
      Profile::FromWebUI(web_ui_)->GetPrefs()->FindPreference(prefs::kProxy);
    if (proxy_pref && (!proxy_pref->IsUserModifiable() ||
                       proxy_pref->HasUserSetting())) {
      proxy_configurable = false;
      // Provide reason that proxy is managed by admin or extension.
      if (proxy_pref->IsManaged())
        change_proxy_text = "policyManagedProxyText";
      else if (proxy_pref->IsExtensionControlled())
        change_proxy_text = "extensionManagedProxyText";
      else
        change_proxy_text = "unmodifiableProxyText";
    }
  }
  // Next check network type and use-shared-proxies.
  chromeos::NetworkProfileType profile = network->profile_type();
  bool shared_network = type == chromeos::TYPE_ETHERNET ||
                        profile == chromeos::PROFILE_SHARED;
  if (proxy_configurable) {  // Only check more if proxy  is still configurable.
    proxy_configurable = profile == chromeos::PROFILE_USER ||
        (shared_network && use_shared_proxies);
  }
  // If no reason has been set yet, provide hint to configure shared proxy.
  if (change_proxy_text.empty() && shared_network && !use_shared_proxies)
    change_proxy_text = "enableSharedProxiesHint";
  // Lastly, store proxy-configurable flag and, if available, text to display.
  dictionary.SetBoolean("proxyConfigurable", proxy_configurable);
  if (!change_proxy_text.empty())
    dictionary.SetString("changeProxyText", change_proxy_text);

  // Hide the dhcp/static radio if not ethernet or wifi (or if not enabled)
  bool staticIPConfig = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableStaticIPConfig);
  dictionary.SetBoolean("showStaticIPConfig", staticIPConfig &&
      (type == chromeos::TYPE_WIFI || type == chromeos::TYPE_ETHERNET));

  if (network->profile_type() == chromeos::PROFILE_USER) {
    dictionary.SetBoolean("showPreferred", true);
    dictionary.SetBoolean("preferred", network->preferred());
  } else {
    dictionary.SetBoolean("showPreferred", false);
    dictionary.SetBoolean("preferred", false);
  }
  dictionary.SetBoolean("autoConnect", network->auto_connect());

  if (type == chromeos::TYPE_WIFI) {
    dictionary.SetBoolean("deviceConnected", cros_->wifi_connected());
    const chromeos::WifiNetwork* wifi =
        cros_->FindWifiNetworkByPath(network->service_path());
    if (!wifi) {
      LOG(WARNING) << "Cannot find network " << network->service_path();
    } else {
      PopulateWifiDetails(wifi, &dictionary);
    }
  } else if (type == chromeos::TYPE_CELLULAR) {
    dictionary.SetBoolean("deviceConnected", cros_->cellular_connected());
    const chromeos::CellularNetwork* cellular =
        cros_->FindCellularNetworkByPath(network->service_path());
    if (!cellular) {
      LOG(WARNING) << "Cannot find network " << network->service_path();
    } else {
      PopulateCellularDetails(cellular, &dictionary);
    }
  } else if (type == chromeos::TYPE_VPN) {
    dictionary.SetBoolean("deviceConnected",
                          cros_->virtual_network_connected());
    const chromeos::VirtualNetwork* vpn =
        cros_->FindVirtualNetworkByPath(network->service_path());
    if (!vpn) {
      LOG(WARNING) << "Cannot find network " << network->service_path();
    } else {
      PopulateVPNDetails(vpn, &dictionary);
    }
  } else if (type == chromeos::TYPE_ETHERNET) {
    dictionary.SetBoolean("deviceConnected", cros_->ethernet_connected());
  }

  web_ui_->CallJavascriptFunction(
      "options.InternetOptions.showDetailedInfo", dictionary);
}

chromeos::ProxyCrosSettingsProvider* InternetOptionsHandler::proxy_settings() {
  if (!proxy_settings_) {
    proxy_settings_ = static_cast<chromeos::ProxyCrosSettingsProvider*>(
      chromeos::CrosSettings::Get()->GetProvider("cros.session.proxy"));
    if (!proxy_settings_)
      NOTREACHED() << "Error getting access to proxy cros settings provider";
  }
  return proxy_settings_;
}

void InternetOptionsHandler::PopulateWifiDetails(
    const chromeos::WifiNetwork* wifi,
    DictionaryValue* dictionary) {
  dictionary->SetString("ssid", wifi->name());
  bool remembered = (wifi->profile_type() != chromeos::PROFILE_NONE);
  dictionary->SetBoolean("remembered", remembered);
  dictionary->SetBoolean("encrypted", wifi->encrypted());
  bool shared = wifi->profile_type() == chromeos::PROFILE_SHARED;
  dictionary->SetBoolean("shared", shared);
}

DictionaryValue* InternetOptionsHandler::CreateDictionaryFromCellularApn(
    const chromeos::CellularApn& apn) {
  DictionaryValue* dictionary = new DictionaryValue();
  dictionary->SetString("apn", apn.apn);
  dictionary->SetString("networkId", apn.network_id);
  dictionary->SetString("username", apn.username);
  dictionary->SetString("password", apn.password);
  dictionary->SetString("name", apn.name);
  dictionary->SetString("localizedName", apn.localized_name);
  dictionary->SetString("language", apn.language);
  return dictionary;
}

void InternetOptionsHandler::PopulateCellularDetails(
    const chromeos::CellularNetwork* cellular,
    DictionaryValue* dictionary) {
  // Cellular network / connection settings.
  dictionary->SetString("serviceName", cellular->name());
  dictionary->SetString("networkTechnology",
                        cellular->GetNetworkTechnologyString());
  dictionary->SetString("operatorName", cellular->operator_name());
  dictionary->SetString("operatorCode", cellular->operator_code());
  dictionary->SetString("activationState",
                        cellular->GetActivationStateString());
  dictionary->SetString("roamingState",
                        cellular->GetRoamingStateString());
  dictionary->SetString("restrictedPool",
                        cellular->restricted_pool() ?
                        l10n_util::GetStringUTF8(
                            IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL) :
                        l10n_util::GetStringUTF8(
                            IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL));
  dictionary->SetString("errorState", cellular->GetErrorString());
  dictionary->SetString("supportUrl", cellular->payment_url());
  dictionary->SetBoolean("needsPlan", cellular->needs_new_plan());

  dictionary->Set("apn", CreateDictionaryFromCellularApn(cellular->apn()));
  dictionary->Set("lastGoodApn",
                  CreateDictionaryFromCellularApn(cellular->last_good_apn()));

  // Device settings.
  const chromeos::NetworkDevice* device =
      cros_->FindNetworkDeviceByPath(cellular->device_path());
  if (device) {
    dictionary->SetString("manufacturer", device->manufacturer());
    dictionary->SetString("modelId", device->model_id());
    dictionary->SetString("firmwareRevision", device->firmware_revision());
    dictionary->SetString("hardwareRevision", device->hardware_revision());
    dictionary->SetString("prlVersion",
                          base::StringPrintf("%u", device->prl_version()));
    dictionary->SetString("meid", device->meid());
    dictionary->SetString("imei", device->imei());
    dictionary->SetString("mdn", device->mdn());
    dictionary->SetString("imsi", device->imsi());
    dictionary->SetString("esn", device->esn());
    dictionary->SetString("min", device->min());
    dictionary->SetBoolean("gsm",
        device->technology_family() == chromeos::TECHNOLOGY_FAMILY_GSM);
    dictionary->SetBoolean("simCardLockEnabled",
        device->sim_pin_required() == chromeos::SIM_PIN_REQUIRED);

    chromeos::MobileConfig* config = chromeos::MobileConfig::GetInstance();
    if (config->IsReady()) {
      std::string carrier_id = cros_->GetCellularHomeCarrierId();
      const chromeos::MobileConfig::Carrier* carrier =
          config->GetCarrier(carrier_id);
      if (carrier && !carrier->top_up_url().empty())
        dictionary->SetString("carrierUrl", carrier->top_up_url());
    }

    const chromeos::CellularApnList& apn_list = device->provider_apn_list();
    ListValue* apn_list_value = new ListValue();
    for (chromeos::CellularApnList::const_iterator it = apn_list.begin();
         it != apn_list.end(); ++it) {
      apn_list_value->Append(CreateDictionaryFromCellularApn(*it));
    }
    dictionary->Set("providerApnList", apn_list_value);
  }

  SetActivationButtonVisibility(cellular, dictionary);
}

void InternetOptionsHandler::PopulateVPNDetails(
    const chromeos::VirtualNetwork* vpn,
    DictionaryValue* dictionary) {
  dictionary->SetString("service_name", vpn->name());
  bool remembered = (vpn->profile_type() != chromeos::PROFILE_NONE);
  dictionary->SetBoolean("remembered", remembered);
  dictionary->SetString("server_hostname", vpn->server_hostname());
  dictionary->SetString("provider_type", vpn->GetProviderTypeString());
  dictionary->SetString("username", vpn->username());
}

void InternetOptionsHandler::SetActivationButtonVisibility(
    const chromeos::CellularNetwork* cellular,
    DictionaryValue* dictionary) {
  if (cellular->needs_new_plan()) {
    dictionary->SetBoolean("showBuyButton", true);
  } else if (cellular->activation_state() !=
                 chromeos::ACTIVATION_STATE_ACTIVATING &&
             cellular->activation_state() !=
                 chromeos::ACTIVATION_STATE_ACTIVATED) {
    dictionary->SetBoolean("showActivateButton", true);
  }
}

void InternetOptionsHandler::CreateModalPopup(views::WidgetDelegate* view) {
  views::Widget* window = browser::CreateViewsWindow(GetNativeWindow(), view);
  window->SetAlwaysOnTop(true);
  window->Show();
}

gfx::NativeWindow InternetOptionsHandler::GetNativeWindow() const {
  // TODO(beng): This is an improper direct dependency on Browser. Route this
  // through some sort of delegate.
  Browser* browser =
      BrowserList::FindBrowserWithProfile(Profile::FromWebUI(web_ui_));
  return browser->window()->GetNativeHandle();
}

void InternetOptionsHandler::ButtonClickCallback(const ListValue* args) {
  std::string str_type;
  std::string service_path;
  std::string command;
  if (args->GetSize() != 3 ||
      !args->GetString(0, &str_type) ||
      !args->GetString(1, &service_path) ||
      !args->GetString(2, &command)) {
    NOTREACHED();
    return;
  }

  int type = atoi(str_type.c_str());
  if (type == chromeos::TYPE_ETHERNET) {
    const chromeos::EthernetNetwork* ether = cros_->ethernet_network();
    if (ether)
      PopulateDictionaryDetails(ether);
  } else if (type == chromeos::TYPE_WIFI) {
    HandleWifiButtonClick(service_path, command);
  } else if (type == chromeos::TYPE_CELLULAR) {
    HandleCellularButtonClick(service_path, command);
  } else if (type == chromeos::TYPE_VPN) {
    HandleVPNButtonClick(service_path, command);
  } else {
    NOTREACHED();
  }
}

void InternetOptionsHandler::HandleWifiButtonClick(
    const std::string& service_path,
    const std::string& command) {
  chromeos::WifiNetwork* wifi = NULL;
  if (command == "forget") {
    cros_->ForgetNetwork(service_path);
  } else if (service_path == kOtherNetworksFakePath) {
    // Other wifi networks.
    CreateModalPopup(new chromeos::NetworkConfigView(chromeos::TYPE_WIFI));
  } else if ((wifi = cros_->FindWifiNetworkByPath(service_path))) {
    if (command == "connect") {
      // Connect to wifi here. Open password page if appropriate.
      if (wifi->IsPassphraseRequired()) {
        CreateModalPopup(new chromeos::NetworkConfigView(wifi));
      } else {
        cros_->ConnectToWifiNetwork(wifi);
      }
    } else if (command == "disconnect") {
      cros_->DisconnectFromNetwork(wifi);
    } else if (command == "options") {
      PopulateDictionaryDetails(wifi);
    }
  }
}

void InternetOptionsHandler::HandleCellularButtonClick(
    const std::string& service_path,
    const std::string& command) {
  chromeos::CellularNetwork* cellular = NULL;
  if (service_path == kOtherNetworksFakePath) {
    chromeos::ChooseMobileNetworkDialog::ShowDialog(GetNativeWindow());
  } else if ((cellular = cros_->FindCellularNetworkByPath(service_path))) {
    if (command == "connect") {
      cros_->ConnectToCellularNetwork(cellular);
    } else if (command == "disconnect") {
      cros_->DisconnectFromNetwork(cellular);
    } else if (command == "activate") {
      Browser* browser = BrowserList::GetLastActive();
      if (browser)
        browser->OpenMobilePlanTabAndActivate();
    } else if (command == "options") {
      PopulateDictionaryDetails(cellular);
    }
  }
}

void InternetOptionsHandler::HandleVPNButtonClick(
    const std::string& service_path,
    const std::string& command) {
  chromeos::VirtualNetwork* network = NULL;
  if (command == "forget") {
    cros_->ForgetNetwork(service_path);
  } else if (service_path == kOtherNetworksFakePath) {
    // TODO(altimofeev): verify if service_path in condition is correct.
    // Other VPN networks.
    CreateModalPopup(new chromeos::NetworkConfigView(chromeos::TYPE_VPN));
  } else if ((network = cros_->FindVirtualNetworkByPath(service_path))) {
    if (command == "connect") {
      // Connect to VPN here. Open password page if appropriate.
      if (network->NeedMoreInfoToConnect()) {
        CreateModalPopup(new chromeos::NetworkConfigView(network));
      } else {
        cros_->ConnectToVirtualNetwork(network);
      }
    } else if (command == "disconnect") {
      cros_->DisconnectFromNetwork(network);
    } else if (command == "options") {
      PopulateDictionaryDetails(network);
    }
  }
}

void InternetOptionsHandler::RefreshCellularPlanCallback(
    const ListValue* args) {
  std::string service_path;
  if (args->GetSize() != 1 ||
      !args->GetString(0, &service_path)) {
    NOTREACHED();
    return;
  }
  const chromeos::CellularNetwork* cellular =
      cros_->FindCellularNetworkByPath(service_path);
  if (cellular)
    cellular->RefreshDataPlansIfNeeded();
}

ListValue* InternetOptionsHandler::GetNetwork(
    const std::string& service_path,
    const SkBitmap& icon,
    const std::string& name,
    bool connecting,
    bool connected,
    bool connectable,
    chromeos::ConnectionType connection_type,
    bool remembered,
    bool shared,
    chromeos::ActivationState activation_state,
    bool needs_new_plan) {
  ListValue* network = new ListValue();

  std::string status;

  if (remembered) {
    if (shared)
      status = l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_SHARED_NETWORK);
  } else {
    // 802.1X networks can be connected but not have saved credentials, and
    // hence be "not configured".  Give preference to the "connected" and
    // "connecting" states.  http://crosbug.com/14459
    int connection_state = IDS_STATUSBAR_NETWORK_DEVICE_DISCONNECTED;
    if (connected)
      connection_state = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTED;
    else if (connecting)
      connection_state = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING;
    else if (!connectable)
      connection_state = IDS_STATUSBAR_NETWORK_DEVICE_NOT_CONFIGURED;
    status = l10n_util::GetStringUTF8(connection_state);
    if (connection_type == chromeos::TYPE_CELLULAR) {
      if (needs_new_plan) {
        status = l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_NO_PLAN_LABEL);
      } else if (activation_state != chromeos::ACTIVATION_STATE_ACTIVATED) {
        status.append(" / ");
        status.append(chromeos::CellularNetwork::ActivationStateToString(
            activation_state));
      }
    }
  }

  // To keep the consistency with JS implementation, do not change the order
  // locally.
  // TODO(kochi): Use dictionaly for future maintainability.
  // 0) service path
  network->Append(Value::CreateStringValue(service_path));
  // 1) name
  network->Append(Value::CreateStringValue(name));
  // 2) status
  network->Append(Value::CreateStringValue(status));
  // 3) type
  network->Append(Value::CreateIntegerValue(static_cast<int>(connection_type)));
  // 4) connected
  network->Append(Value::CreateBooleanValue(connected));
  // 5) connecting
  network->Append(Value::CreateBooleanValue(connecting));
  // 6) icon data url
  network->Append(Value::CreateStringValue(icon.isNull() ? "" :
      web_ui_util::GetImageDataUrl(icon)));
  // 7) remembered
  network->Append(Value::CreateBooleanValue(remembered));
  // 8) activation state
  network->Append(Value::CreateIntegerValue(
                    static_cast<int>(activation_state)));
  // 9) needs new plan
  network->Append(Value::CreateBooleanValue(needs_new_plan));
  // 10) connectable
  network->Append(Value::CreateBooleanValue(connectable));
  return network;
}

ListValue* InternetOptionsHandler::GetWiredList() {
  ListValue* list = new ListValue();

  // If ethernet is not enabled, then don't add anything.
  if (cros_->ethernet_enabled()) {
    const chromeos::EthernetNetwork* ethernet_network =
        cros_->ethernet_network();
    if (ethernet_network) {
      const SkBitmap icon =
          chromeos::NetworkMenuIcon::GetBitmap(ethernet_network);
      list->Append(GetNetwork(
          ethernet_network->service_path(),
          icon,
          l10n_util::GetStringUTF8(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET),
          ethernet_network->connecting(),
          ethernet_network->connected(),
          ethernet_network->connectable(),
          chromeos::TYPE_ETHERNET,
          false,
          false,
          chromeos::ACTIVATION_STATE_UNKNOWN,
          false));
    }
  }
  return list;
}

ListValue* InternetOptionsHandler::GetWirelessList() {
  ListValue* list = new ListValue();

  const chromeos::WifiNetworkVector& wifi_networks = cros_->wifi_networks();
  for (chromeos::WifiNetworkVector::const_iterator it =
      wifi_networks.begin(); it != wifi_networks.end(); ++it) {
    const SkBitmap icon = chromeos::NetworkMenuIcon::GetBitmap(*it);
    list->Append(GetNetwork(
        (*it)->service_path(),
        icon,
        (*it)->name(),
        (*it)->connecting(),
        (*it)->connected(),
        cros_->CanConnectToNetwork(*it),
        chromeos::TYPE_WIFI,
        false,
        false,
        chromeos::ACTIVATION_STATE_UNKNOWN,
        false));
  }

  // Add "Other WiFi network..." if wifi is enabled.
  if (cros_->wifi_enabled()) {
    list->Append(GetNetwork(
        kOtherNetworksFakePath,
        chromeos::NetworkMenuIcon::GetConnectedBitmap(
            chromeos::NetworkMenuIcon::ARCS),
        l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_OTHER_WIFI_NETWORKS),
        false,
        false,
        true,
        chromeos::TYPE_WIFI,
        false,
        false,
        chromeos::ACTIVATION_STATE_UNKNOWN,
        false));
  }

  const chromeos::CellularNetworkVector cellular_networks =
      cros_->cellular_networks();
  for (chromeos::CellularNetworkVector::const_iterator it =
      cellular_networks.begin(); it != cellular_networks.end(); ++it) {
    const SkBitmap icon = chromeos::NetworkMenuIcon::GetBitmap(*it);
    list->Append(GetNetwork(
        (*it)->service_path(),
        icon,
        (*it)->name(),
        (*it)->connecting(),
        (*it)->connected(),
        cros_->CanConnectToNetwork(*it),
        chromeos::TYPE_CELLULAR,
        false,
        false,
        (*it)->activation_state(),
        (*it)->SupportsDataPlan() && (*it)->restricted_pool()));
  }

  const chromeos::NetworkDevice* cellular_device = cros_->FindCellularDevice();
  if (cellular_device && cellular_device->support_network_scan() &&
      cros_->cellular_enabled()) {
    list->Append(GetNetwork(
        kOtherNetworksFakePath,
        chromeos::NetworkMenuIcon::GetDisconnectedBitmap(
            chromeos::NetworkMenuIcon::BARS),
        l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_OTHER_CELLULAR_NETWORKS),
        false,
        false,
        true,
        chromeos::TYPE_CELLULAR,
        false,
        false,
        chromeos::ACTIVATION_STATE_ACTIVATED,
        false));
  }

  return list;
}

ListValue* InternetOptionsHandler::GetVPNList() {
  ListValue* list = new ListValue();

  const chromeos::VirtualNetworkVector& virtual_networks =
      cros_->virtual_networks();
  for (chromeos::VirtualNetworkVector::const_iterator it =
      virtual_networks.begin(); it != virtual_networks.end(); ++it) {
    const SkBitmap icon = chromeos::NetworkMenuIcon::GetBitmap(*it);
    list->Append(GetNetwork(
        (*it)->service_path(),
        icon,
        (*it)->name(),
        (*it)->connecting(),
        (*it)->connected(),
        cros_->CanConnectToNetwork(*it),
        chromeos::TYPE_VPN,
        false,
        false,
        chromeos::ACTIVATION_STATE_UNKNOWN,
        false));
  }

  return list;
}

ListValue* InternetOptionsHandler::GetRememberedList() {
  ListValue* list = new ListValue();

  for (chromeos::WifiNetworkVector::const_iterator rit =
           cros_->remembered_wifi_networks().begin();
       rit != cros_->remembered_wifi_networks().end(); ++rit) {
    chromeos::WifiNetwork* remembered = *rit;
    chromeos::WifiNetwork* wifi = static_cast<chromeos::WifiNetwork*>(
        cros_->FindNetworkByUniqueId(remembered->unique_id()));

    // Set in_active_profile.
    bool shared =
        remembered->profile_type() == chromeos::PROFILE_SHARED;
    const SkBitmap icon =
        chromeos::NetworkMenuIcon::GetBitmap(wifi ? wifi : remembered);
    list->Append(GetNetwork(
        remembered->service_path(),
        icon,
        remembered->name(),
        wifi ? wifi->connecting() : false,
        wifi ? wifi->connected() : false,
        true,
        chromeos::TYPE_WIFI,
        true,
        shared,
        chromeos::ACTIVATION_STATE_UNKNOWN,
        false));
  }

  for (chromeos::VirtualNetworkVector::const_iterator rit =
           cros_->remembered_virtual_networks().begin();
       rit != cros_->remembered_virtual_networks().end(); ++rit) {
    chromeos::VirtualNetwork* remembered = *rit;
    chromeos::VirtualNetwork* vpn = static_cast<chromeos::VirtualNetwork*>(
        cros_->FindNetworkByUniqueId(remembered->unique_id()));

    // Set in_active_profile.
    bool shared =
        remembered->profile_type() == chromeos::PROFILE_SHARED;
    const SkBitmap icon =
        chromeos::NetworkMenuIcon::GetBitmap(vpn ? vpn : remembered);
    list->Append(GetNetwork(
        remembered->service_path(),
        icon,
        remembered->name(),
        vpn ? vpn->connecting() : false,
        vpn ? vpn->connected() : false,
        true,
        chromeos::TYPE_WIFI,
        true,
        shared,
        chromeos::ACTIVATION_STATE_UNKNOWN,
        false));
  }

  return list;
}

void InternetOptionsHandler::FillNetworkInfo(DictionaryValue* dictionary) {
  dictionary->SetBoolean("accessLocked", cros_->IsLocked());
  dictionary->Set("wiredList", GetWiredList());
  dictionary->Set("wirelessList", GetWirelessList());
  dictionary->Set("vpnList", GetVPNList());
  dictionary->Set("rememberedList", GetRememberedList());
  dictionary->SetBoolean("wifiAvailable", cros_->wifi_available());
  dictionary->SetBoolean("wifiBusy", cros_->wifi_busy());
  dictionary->SetBoolean("wifiEnabled", cros_->wifi_enabled());
  dictionary->SetBoolean("cellularAvailable", cros_->cellular_available());
  dictionary->SetBoolean("cellularBusy", cros_->cellular_busy());
  dictionary->SetBoolean("cellularEnabled", cros_->cellular_enabled());
}
