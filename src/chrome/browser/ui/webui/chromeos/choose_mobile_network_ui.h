// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CHOOSE_MOBILE_NETWORK_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CHOOSE_MOBILE_NETWORK_UI_H_
#pragma once

#include "chrome/browser/ui/webui/chrome_web_ui.h"

namespace chromeos {

// A custom WebUI that defines datasource for choosing cellular network dialog.
class ChooseMobileNetworkUI : public ChromeWebUI {
 public:
  explicit ChooseMobileNetworkUI(TabContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(ChooseMobileNetworkUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CHOOSE_MOBILE_NETWORK_UI_H_
