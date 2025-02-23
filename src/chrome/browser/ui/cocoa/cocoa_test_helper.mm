// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"

#include "base/mac/foundation_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_constants.h"

CocoaTest::CocoaTest() {
  BootstrapCocoa();

  Init();
}

void CocoaTest::BootstrapCocoa() {
  // Look in the framework bundle for resources.
  FilePath path;
  PathService::Get(base::DIR_EXE, &path);
  path = path.Append(chrome::kFrameworkName);
  base::mac::SetOverrideAppBundlePath(path);
}
