// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_id.h"

#include <string>

#include "base/stringprintf.h"

std::string DownloadId::DebugString() const {
  return base::StringPrintf("%p:%d", domain_, local_id_);
}

std::ostream& operator<<(std::ostream& out, const DownloadId& global_id) {
  return out << global_id.DebugString();
}
