// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_SYSTEM_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_SYSTEM_OPTIONS_HANDLER_H_
#pragma once

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/options/chromeos/cros_options_page_ui_handler.h"

namespace base {
class DictionaryValue;
}

// ChromeOS system options page UI handler.
class SystemOptionsHandler
  : public chromeos::CrosOptionsPageUIHandler,
    public base::SupportsWeakPtr<SystemOptionsHandler> {
 public:
  SystemOptionsHandler();
  virtual ~SystemOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(base::DictionaryValue* localized_strings);
  virtual void Initialize();

  virtual void RegisterMessages();

  // Called when the accessibility checkbox value is changed.
  // |args| will contain the checkbox checked state as a string
  // ("true" or "false").
  void AccessibilityChangeCallback(const base::ListValue* args);

  // Called when the System configuration screen is used to adjust
  // the screen brightness.
  // |args| will be an empty list.
  void DecreaseScreenBrightnessCallback(const base::ListValue* args);
  void IncreaseScreenBrightnessCallback(const base::ListValue* args);

  // Called when the 'Enable bluetooth' checkbox value is changed.
  // |args| will contain the checkbox checked state as a string
  // ("true" or "false").
  void BluetoothEnableChangeCallback(const base::ListValue* args);

  // Called when the 'Find Devices' button is pressed from the
  // Bluetooth settings.
  // |args| will contain the list of devices currently connected
  // devices according to the System options page.
  void FindBluetoothDevicesCallback(const base::ListValue* args);

  // Sends a notification to the Web UI of the status of a bluetooth device.
  // |device| is the decription of the device.  The supported dictionary keys
  // for device are "deviceName", "deviceId", "deviceType" and "deviceStatus".
  void BluetoothDeviceNotification(const base::DictionaryValue& device);

 private:
  // Simulates extracting a list of available bluetooth devices.
  // Called when emulating ChromeOS from a desktop environment.
  void GenerateFakeDeviceList();

  // Callback for TouchpadHelper.
  void TouchpadExists(bool* exists);

  DISALLOW_COPY_AND_ASSIGN(SystemOptionsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_SYSTEM_OPTIONS_HANDLER_H_
