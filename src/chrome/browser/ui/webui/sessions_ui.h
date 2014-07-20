// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SESSIONS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SESSIONS_UI_H_
#pragma once

#include "chrome/browser/ui/webui/chrome_web_ui.h"

class RefCountedMemory;

class SessionsUI : public ChromeWebUI {
 public:
  explicit SessionsUI(TabContents* contents);

  static RefCountedMemory* GetFaviconResourceBytes();

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SESSIONS_UI_H_
