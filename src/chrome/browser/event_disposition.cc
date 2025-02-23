// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/event_disposition.h"
#include "content/browser/disposition_utils.h"
#include "ui/base/events.h"

namespace browser {

WindowOpenDisposition DispositionFromEventFlags(int event_flags) {
  return disposition_utils::DispositionFromClick(
      (event_flags & ui::EF_MIDDLE_BUTTON_DOWN) != 0,
      (event_flags & ui::EF_ALT_DOWN) != 0,
      (event_flags & ui::EF_CONTROL_DOWN) != 0,
      (event_flags & ui::EF_COMMAND_DOWN) != 0,
      (event_flags & ui::EF_SHIFT_DOWN) != 0);
}

}  // namespace browser
