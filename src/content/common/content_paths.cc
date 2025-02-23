// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_paths.h"

#include "base/file_util.h"
#include "base/path_service.h"

namespace content {

bool PathProvider(int key, FilePath* result) {
  switch (key) {
    case CHILD_PROCESS_EXE:
      return PathService::Get(base::FILE_EXE, result);
    case DIR_TEST_DATA: {
      FilePath cur;
      if (!PathService::Get(base::DIR_SOURCE_ROOT, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("content"));
      cur = cur.Append(FILE_PATH_LITERAL("test"));
      cur = cur.Append(FILE_PATH_LITERAL("data"));
      if (!file_util::PathExists(cur))  // we don't want to create this
        return false;

      *result = cur;
      return true;
      break;
    }
    default:
      return false;
  }

  return false;
}

// This cannot be done as a static initializer sadly since Visual Studio will
// eliminate this object file if there is no direct entry point into it.
void RegisterPathProvider() {
  PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace content
