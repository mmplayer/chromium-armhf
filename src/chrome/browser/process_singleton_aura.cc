// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_singleton.h"

// Look for a Chrome instance that uses the same profile directory.
ProcessSingleton::ProcessSingleton(const FilePath& user_data_dir)
    :
#if defined(OS_WIN) && !defined(USE_AURA)
    window_(NULL),
#endif
    locked_(false), foreground_window_(NULL) {
}

ProcessSingleton::~ProcessSingleton() {
}

ProcessSingleton::NotifyResult ProcessSingleton::NotifyOtherProcess() {
  // TODO(beng):
  NOTIMPLEMENTED();
  return PROCESS_NONE;
}

ProcessSingleton::NotifyResult ProcessSingleton::NotifyOtherProcessOrCreate() {
  // TODO(beng):
  NOTIMPLEMENTED();
  return PROCESS_NONE;
}

bool ProcessSingleton::Create() {
  // TODO(beng):
  NOTIMPLEMENTED();
  return true;
}

void ProcessSingleton::Cleanup() {
  // TODO(beng):
  NOTIMPLEMENTED();
}
