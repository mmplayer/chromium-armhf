// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/utility/utility_thread.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"

namespace content {

// Keep the global UtilityThread in a TLS slot so it is impossible to access
// incorrectly from the wrong thread.
static base::LazyInstance<base::ThreadLocalPointer<UtilityThread> > lazy_tls(
    base::LINKER_INITIALIZED);

UtilityThread* UtilityThread::Get() {
  return lazy_tls.Pointer()->Get();
}

UtilityThread::UtilityThread() {
  lazy_tls.Pointer()->Set(this);
}

UtilityThread::~UtilityThread() {
  lazy_tls.Pointer()->Set(NULL);
}

}  // namespace content

