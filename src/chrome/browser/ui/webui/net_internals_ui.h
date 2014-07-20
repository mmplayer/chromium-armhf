// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_UI_H_
#pragma once

#include "chrome/browser/ui/webui/chrome_web_ui.h"

namespace base {
class Value;
}

class NetInternalsUI : public ChromeWebUI {
 public:
  explicit NetInternalsUI(TabContents* contents);

  // Returns a Value containing constants NetInternals needs to load a log file.
  // Safe to call on any thread.  Caller takes ownership of the returned Value.
  static base::Value* GetConstants();

 private:
  DISALLOW_COPY_AND_ASSIGN(NetInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_UI_H_
