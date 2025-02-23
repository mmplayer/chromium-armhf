// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PROCESS_WATCHER_H_
#define CONTENT_COMMON_PROCESS_WATCHER_H_
#pragma once

#include "build/build_config.h"

#include "base/basictypes.h"
#include "base/process.h"
#include "content/common/content_export.h"

class CONTENT_EXPORT ProcessWatcher {
 public:
  // This method ensures that the specified process eventually terminates, and
  // then it closes the given process handle.
  //
  // It assumes that the process has already been signalled to exit, and it
  // begins by waiting a small amount of time for it to exit.  If the process
  // does not appear to have exited, then this function starts to become
  // aggressive about ensuring that the process terminates.
  //
  // On Linux this method does not block the calling thread.
  // On OS X this method may block for up to 2 seconds.
  //
  // NOTE: The process handle must have been opened with the PROCESS_TERMINATE
  // and SYNCHRONIZE permissions.
  //
  static void EnsureProcessTerminated(base::ProcessHandle process_handle);

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // The nicer version of EnsureProcessTerminated() that is patient and will
  // wait for |process_handle| to finish and then reap it.
  static void EnsureProcessGetsReaped(base::ProcessHandle process_handle);
#endif

 private:
  // Do not instantiate this class.
  ProcessWatcher();

  DISALLOW_COPY_AND_ASSIGN(ProcessWatcher);
};

#endif  // CONTENT_COMMON_PROCESS_WATCHER_H_
