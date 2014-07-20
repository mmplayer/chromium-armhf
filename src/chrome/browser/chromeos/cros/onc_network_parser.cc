// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/onc_network_parser.h"

#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/native_network_constants.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "base/json/json_value_serializer.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// Local constants.
namespace {

EnumMapper<PropertyIndex>::Pair property_index_table[] = {
    { "GUID", PROPERTY_INDEX_GUID },
    { "Name", PROPERTY_INDEX_NAME },
    { "Remove", PROPERTY_INDEX_REMOVE },
    { "ProxyURL", PROPERTY_INDEX_PROXY_CONFIG },
    { "Type", PROPERTY_INDEX_TYPE },
    { "SSID", PROPERTY_INDEX_NAME },
    { "Passphrase", PROPERTY_INDEX_PASSPHRASE },
    { "AutoConnect", PROPERTY_INDEX_AUTO_CONNECT },
    { "HiddenSSID", PROPERTY_INDEX_HIDDEN_SSID },
    { "Security", PROPERTY_INDEX_SECURITY },
    { "EAP", PROPERTY_INDEX_EAP },
    { "Outer", PROPERTY_INDEX_EAP_METHOD },
    { "Inner", PROPERTY_INDEX_EAP_PHASE_2_AUTH },
    { "UseSystemCAs", PROPERTY_INDEX_EAP_USE_SYSTEM_CAS },
    { "ServerCARef", PROPERTY_INDEX_EAP_CA_CERT },
    { "ClientCARef", PROPERTY_INDEX_EAP_CLIENT_CERT },
    { "ClientCertPattern", PROPERTY_INDEX_EAP_CLIENT_CERT_PATTERN },
    { "Identity", PROPERTY_INDEX_EAP_IDENTITY },
    { "Password", PROPERTY_INDEX_EAP_PASSWORD },
    { "AnonymousIdentity", PROPERTY_INDEX_EAP_ANONYMOUS_IDENTITY },
};

// Serve the singleton mapper instance.
const EnumMapper<PropertyIndex>* get_onc_mapper() {
  static const EnumMapper<PropertyIndex> mapper(property_index_table,
                                                arraysize(property_index_table),
                                                PROPERTY_INDEX_UNKNOWN);
  return &mapper;
}

ConnectionType ParseNetworkType(const std::string& type) {
  static EnumMapper<ConnectionType>::Pair table[] = {
    { "WiFi", TYPE_WIFI },
    { "VPN", TYPE_VPN },
  };
  static EnumMapper<ConnectionType> parser(
      table, arraysize(table), TYPE_UNKNOWN);
  return parser.Get(type);
}

}  // namespace

// -------------------- OncNetworkParser --------------------

OncNetworkParser::OncNetworkParser(const std::string& onc_blob)
    : NetworkParser(get_onc_mapper()),
      network_configs_(NULL),
      certificates_(NULL) {
  JSONStringValueSerializer deserializer(onc_blob);
  deserializer.set_allow_trailing_comma(true);
  std::string error_msg;
  scoped_ptr<base::Value> root(deserializer.Deserialize(NULL, &error_msg));

  if (!root.get() || root->GetType() != base::Value::TYPE_DICTIONARY) {
    LOG(WARNING) << "OncNetworkParser received bad ONC file: " << error_msg;
  } else {
    root_dict_.reset(static_cast<DictionaryValue*>(root.release()));
    if (!root_dict_->GetList("NetworkConfigurations", &network_configs_)) {
      LOG(WARNING) << "ONC file missing NetworkConfigurations";
    }
    // Certificates is optional
    root_dict_->GetList("Certificates", &certificates_);
  }
}

OncNetworkParser::OncNetworkParser() : NetworkParser(get_onc_mapper()) {
}

OncNetworkParser::~OncNetworkParser() {
}

// static
const EnumMapper<PropertyIndex>* OncNetworkParser::property_mapper() {
  return get_onc_mapper();
}

int OncNetworkParser::GetNetworkConfigsSize() const {
  return network_configs_ ? network_configs_->GetSize() : 0;
}

int OncNetworkParser::GetCertificatesSize() const {
  return certificates_ ? certificates_->GetSize() : 0;
}

Network* OncNetworkParser::ParseNetwork(int n) {
  // TODO(chocobo): Change this to parse network into a dictionary.
  if (!network_configs_)
    return NULL;
  DictionaryValue* info = NULL;
  if (!network_configs_->GetDictionary(n, &info))
    return NULL;
  // Parse Open Network Configuration blob into a temporary Network object.
  return CreateNetworkFromInfo(std::string(), *info);
}

Network* OncNetworkParser::CreateNetworkFromInfo(
    const std::string& service_path,
    const DictionaryValue& info) {
  ConnectionType type = ParseTypeFromDictionary(info);
  if (type == TYPE_UNKNOWN)  // Return NULL if cannot parse network type.
    return NULL;
  scoped_ptr<Network> network(CreateNewNetwork(type, service_path));

  // Get the child dictionary with properties for the network.
  // And copy all the values from this network type dictionary to parent.
  DictionaryValue* dict;
  if (!info.GetDictionary(GetTypeFromDictionary(info), &dict))
    return NULL;

  UpdateNetworkFromInfo(*dict, network.get());
  VLOG(2) << "Created Network '" << network->name()
          << "' from info. Path:" << service_path
          << " Type:" << ConnectionTypeToString(type);
  return network.release();
}

Network* OncNetworkParser::CreateNewNetwork(
    ConnectionType type, const std::string& service_path) {
  Network* network = NetworkParser::CreateNewNetwork(type, service_path);
  if (network) {
    if (type == TYPE_WIFI)
      network->SetNetworkParser(new OncWifiNetworkParser());
    else if (type == TYPE_VPN)
      network->SetNetworkParser(new OncVirtualNetworkParser());
  }
  return network;
}

ConnectionType OncNetworkParser::ParseType(const std::string& type) {
  return ParseNetworkType(type);
}

ConnectionType OncNetworkParser::ParseTypeFromDictionary(
    const DictionaryValue& info) {
  return ParseType(GetTypeFromDictionary(info));
}

std::string OncNetworkParser::GetTypeFromDictionary(
    const base::DictionaryValue& info) {
  std::string type_string;
  info.GetString("Type", &type_string);
  return type_string;
}


// -------------------- OncWirelessNetworkParser --------------------

OncWirelessNetworkParser::OncWirelessNetworkParser() {}
OncWirelessNetworkParser::~OncWirelessNetworkParser() {}

bool OncWirelessNetworkParser::ParseValue(PropertyIndex index,
                                          const base::Value& value,
                                          Network* network) {
  DCHECK_NE(TYPE_ETHERNET, network->type());
  DCHECK_NE(TYPE_VPN, network->type());
  return OncNetworkParser::ParseValue(index, value, network);
}

// -------------------- OncWifiNetworkParser --------------------

OncWifiNetworkParser::OncWifiNetworkParser() {}
OncWifiNetworkParser::~OncWifiNetworkParser() {}

bool OncWifiNetworkParser::ParseValue(PropertyIndex index,
                                      const base::Value& value,
                                      Network* network) {
  DCHECK_EQ(TYPE_WIFI, network->type());
  WifiNetwork* wifi_network = static_cast<WifiNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_NAME: {
      return OncWirelessNetworkParser::ParseValue(index, value, network);
    }
    case PROPERTY_INDEX_GUID: {
      std::string unique_id;
      if (!value.GetAsString(&unique_id))
        break;
      wifi_network->set_unique_id(unique_id);
      return true;
    }
    case PROPERTY_INDEX_SECURITY: {
      std::string security_string;
      if (!value.GetAsString(&security_string))
        break;
      wifi_network->set_encryption(ParseSecurity(security_string));
      return true;
    }
    case PROPERTY_INDEX_PASSPHRASE: {
      std::string passphrase;
      if (!value.GetAsString(&passphrase))
        break;
      wifi_network->set_passphrase(passphrase);
      return true;
    }
    case PROPERTY_INDEX_IDENTITY: {
      std::string identity;
      if (!value.GetAsString(&identity))
        break;
      wifi_network->set_identity(identity);
      return true;
    }
    case PROPERTY_INDEX_EAP: {
      DCHECK_EQ(value.GetType(), Value::TYPE_DICTIONARY);
      const DictionaryValue& dict = static_cast<const DictionaryValue&>(value);
      for (DictionaryValue::key_iterator iter = dict.begin_keys();
           iter != dict.end_keys(); ++iter) {
        const std::string& key = *iter;
        base::Value* eap_value;
        bool res = dict.GetWithoutPathExpansion(key, &eap_value);
        DCHECK(res);
        if (res) {
          PropertyIndex index = mapper().Get(key);
          if (!ParseEAPValue(index, *eap_value, wifi_network))
            VLOG(1) << network->name() << ": EAP unhandled key: " << key
                    << " Type: " << eap_value->GetType();
        }
      }
      return true;
    }
    default:
      return OncWirelessNetworkParser::ParseValue(index, value, network);
  }
  return false;
}


bool OncWifiNetworkParser::ParseEAPValue(PropertyIndex index,
                                         const base::Value& value,
                                         WifiNetwork* wifi_network) {
  switch (index) {
    case PROPERTY_INDEX_EAP_IDENTITY: {
      std::string eap_identity;
      if (!value.GetAsString(&eap_identity))
        break;
      wifi_network->set_eap_identity(eap_identity);
      return true;
    }
    case PROPERTY_INDEX_EAP_METHOD: {
      std::string eap_method;
      if (!value.GetAsString(&eap_method))
        break;
      wifi_network->set_eap_method(ParseEAPMethod(eap_method));
      return true;
    }
    case PROPERTY_INDEX_EAP_PHASE_2_AUTH: {
      std::string eap_phase_2_auth;
      if (!value.GetAsString(&eap_phase_2_auth))
        break;
      wifi_network->set_eap_phase_2_auth(ParseEAPPhase2Auth(eap_phase_2_auth));
      return true;
    }
    case PROPERTY_INDEX_EAP_ANONYMOUS_IDENTITY: {
      std::string eap_anonymous_identity;
      if (!value.GetAsString(&eap_anonymous_identity))
        break;
      wifi_network->set_eap_anonymous_identity(eap_anonymous_identity);
      return true;
    }
    case PROPERTY_INDEX_EAP_CERT_ID: {
      std::string eap_client_cert_pkcs11_id;
      if (!value.GetAsString(&eap_client_cert_pkcs11_id))
        break;
      wifi_network->set_eap_client_cert_pkcs11_id(eap_client_cert_pkcs11_id);
      return true;
    }
    case PROPERTY_INDEX_EAP_CA_CERT_NSS: {
      std::string eap_server_ca_cert_nss_nickname;
      if (!value.GetAsString(&eap_server_ca_cert_nss_nickname))
        break;
      wifi_network->set_eap_server_ca_cert_nss_nickname(
          eap_server_ca_cert_nss_nickname);
      return true;
    }
    case PROPERTY_INDEX_EAP_USE_SYSTEM_CAS: {
      bool eap_use_system_cas;
      if (!value.GetAsBoolean(&eap_use_system_cas))
        break;
      wifi_network->set_eap_use_system_cas(eap_use_system_cas);
      return true;
    }
    case PROPERTY_INDEX_EAP_PASSWORD: {
      std::string eap_passphrase;
      if (!value.GetAsString(&eap_passphrase))
        break;
      wifi_network->set_eap_passphrase(eap_passphrase);
      return true;
    }
    default:
      break;
  }
  return false;
}

ConnectionSecurity OncWifiNetworkParser::ParseSecurity(
    const std::string& security) {
  static EnumMapper<ConnectionSecurity>::Pair table[] = {
    { "None", SECURITY_NONE },
    { "WEP", SECURITY_WEP },
    { "WPA", SECURITY_WPA },
    { "WPA2", SECURITY_8021X },
  };
  static EnumMapper<ConnectionSecurity> parser(
      table, arraysize(table), SECURITY_UNKNOWN);
  return parser.Get(security);
}

EAPMethod OncWifiNetworkParser::ParseEAPMethod(const std::string& method) {
  static EnumMapper<EAPMethod>::Pair table[] = {
    { "PEAP", EAP_METHOD_PEAP },
    { "EAP-TLS", EAP_METHOD_TLS },
    { "EAP-TTLS", EAP_METHOD_TTLS },
    { "LEAP", EAP_METHOD_LEAP },
  };
  static EnumMapper<EAPMethod> parser(
      table, arraysize(table), EAP_METHOD_UNKNOWN);
  return parser.Get(method);
}

EAPPhase2Auth OncWifiNetworkParser::ParseEAPPhase2Auth(
    const std::string& auth) {
  static EnumMapper<EAPPhase2Auth>::Pair table[] = {
    { "MD5", EAP_PHASE_2_AUTH_MD5 },
    { "MSCHAPV2", EAP_PHASE_2_AUTH_MSCHAPV2 },
    { "MSCHAP", EAP_PHASE_2_AUTH_MSCHAP },
    { "PAP", EAP_PHASE_2_AUTH_PAP },
  };
  static EnumMapper<EAPPhase2Auth> parser(
      table, arraysize(table), EAP_PHASE_2_AUTH_AUTO);
  return parser.Get(auth);
}

// -------------------- OncVirtualNetworkParser --------------------


OncVirtualNetworkParser::OncVirtualNetworkParser() {}
OncVirtualNetworkParser::~OncVirtualNetworkParser() {}

bool OncVirtualNetworkParser::UpdateNetworkFromInfo(
    const DictionaryValue& info,
    Network* network) {
  DCHECK_EQ(TYPE_VPN, network->type());
  VirtualNetwork* virtual_network = static_cast<VirtualNetwork*>(network);
  if (!OncNetworkParser::UpdateNetworkFromInfo(info, network))
    return false;
  VLOG(1) << "Updating VPN '" << virtual_network->name()
          << "': Server: " << virtual_network->server_hostname()
          << " Type: "
          << ProviderTypeToString(virtual_network->provider_type());
  if (virtual_network->provider_type() == PROVIDER_TYPE_L2TP_IPSEC_PSK) {
    if (!virtual_network->client_cert_id().empty())
      virtual_network->set_provider_type(PROVIDER_TYPE_L2TP_IPSEC_USER_CERT);
  }
  return true;
}

bool OncVirtualNetworkParser::ParseValue(PropertyIndex index,
                                         const base::Value& value,
                                         Network* network) {
  DCHECK_EQ(TYPE_VPN, network->type());
  VirtualNetwork* virtual_network = static_cast<VirtualNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_PROVIDER: {
      DCHECK_EQ(value.GetType(), Value::TYPE_DICTIONARY);
      const DictionaryValue& dict = static_cast<const DictionaryValue&>(value);
      for (DictionaryValue::key_iterator iter = dict.begin_keys();
           iter != dict.end_keys(); ++iter) {
        const std::string& key = *iter;
        base::Value* provider_value;
        bool res = dict.GetWithoutPathExpansion(key, &provider_value);
        DCHECK(res);
        if (res) {
          PropertyIndex index = mapper().Get(key);
          if (!ParseProviderValue(index, *provider_value, virtual_network))
            VLOG(1) << network->name() << ": Provider unhandled key: " << key
                    << " Type: " << provider_value->GetType();
        }
      }
      return true;
    }
    default:
      return OncNetworkParser::ParseValue(index, value, network);
      break;
  }
  return false;
}

bool OncVirtualNetworkParser::ParseProviderValue(PropertyIndex index,
                                                 const base::Value& value,
                                                 VirtualNetwork* network) {
  switch (index) {
    case PROPERTY_INDEX_HOST: {
      std::string server_hostname;
      if (!value.GetAsString(&server_hostname))
        break;
      network->set_server_hostname(server_hostname);
      return true;
    }
    case PROPERTY_INDEX_NAME: {
      std::string name;
      if (!value.GetAsString(&name))
        break;
      network->set_name(name);
      return true;
    }
    case PROPERTY_INDEX_TYPE: {
      std::string provider_type_string;
      if (!value.GetAsString(&provider_type_string))
        break;
      network->set_provider_type(ParseProviderType(provider_type_string));
      return true;
    }
    case PROPERTY_INDEX_L2TPIPSEC_CA_CERT_NSS: {
      std::string ca_cert_nss;
      if (!value.GetAsString(&ca_cert_nss))
        break;
      network->set_ca_cert_nss(ca_cert_nss);
      return true;
    }
    case PROPERTY_INDEX_L2TPIPSEC_PSK: {
      std::string psk_passphrase;
      if (!value.GetAsString(&psk_passphrase))
        break;
      network->set_psk_passphrase(psk_passphrase);
      return true;
    }
    case PROPERTY_INDEX_L2TPIPSEC_CLIENT_CERT_ID: {
      std::string client_cert_id;
      if (!value.GetAsString(&client_cert_id))
        break;
      network->set_client_cert_id(client_cert_id);
      return true;
    }
    case PROPERTY_INDEX_L2TPIPSEC_USER: {
      std::string username;
      if (!value.GetAsString(&username))
        break;
      network->set_username(username);
      return true;
    }
    case PROPERTY_INDEX_L2TPIPSEC_PASSWORD: {
      std::string user_passphrase;
      if (!value.GetAsString(&user_passphrase))
        break;
      network->set_user_passphrase(user_passphrase);
      return true;
    }
    case PROPERTY_INDEX_L2TPIPSEC_GROUP_NAME: {
      std::string group_name;
      if (!value.GetAsString(&group_name))
        break;
      network->set_group_name(group_name);
      return true;
    }
    default:
      break;
  }
  return false;
}

ProviderType OncVirtualNetworkParser::ParseProviderType(
    const std::string& type) {
  static EnumMapper<ProviderType>::Pair table[] = {
    { flimflam::kProviderL2tpIpsec, PROVIDER_TYPE_L2TP_IPSEC_PSK },
    { flimflam::kProviderOpenVpn, PROVIDER_TYPE_OPEN_VPN },
  };
  static EnumMapper<ProviderType> parser(
      table, arraysize(table), PROVIDER_TYPE_MAX);
  return parser.Get(type);
}

}  // namespace chromeos
