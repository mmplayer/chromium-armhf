// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FULLSCREEN_EXIT_BUBBLE_TYPE_H_
#define CHROME_BROWSER_UI_FULLSCREEN_EXIT_BUBBLE_TYPE_H_
#pragma once

#include "base/string16.h"
#include "googleurl/src/gurl.h"

// Describes the contents of the fullscreen exit bubble.
// For example, if the user already agreed to fullscreen mode and the
// web page then requests mouse lock, "do you want to allow mouse lock"
// will be shown.
enum FullscreenExitBubbleType {
  FEB_TYPE_NONE = 0,

  // For tab fullscreen mode. (More comments about tab and browser fullscreen
  // mode can be found in chrome/browser/ui/browser.h.)
  FEB_TYPE_FULLSCREEN_BUTTONS,
  FEB_TYPE_FULLSCREEN_MOUSELOCK_BUTTONS,
  FEB_TYPE_MOUSELOCK_BUTTONS,
  FEB_TYPE_FULLSCREEN_EXIT_INSTRUCTION,
  FEB_TYPE_FULLSCREEN_MOUSELOCK_EXIT_INSTRUCTION,
  FEB_TYPE_MOUSELOCK_EXIT_INSTRUCTION,

  // For browser fullscreen mode.
  FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION
};

namespace fullscreen_bubble {

string16 GetLabelTextForType(
    FullscreenExitBubbleType type, const GURL& url);
string16 GetDenyButtonTextForType(FullscreenExitBubbleType type);
bool ShowButtonsForType(FullscreenExitBubbleType type);
void PermissionRequestedByType(FullscreenExitBubbleType type,
                               bool* tab_fullscreen,
                               bool* mouse_lock);

}  // namespace fullscreen_bubble

#endif  // CHROME_BROWSER_UI_FULLSCREEN_EXIT_BUBBLE_TYPE_H_
