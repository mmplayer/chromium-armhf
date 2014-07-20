// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"

// Disabled, http://crbug.com/91058.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_WebSocket) {
  FilePath websocket_root_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &websocket_root_dir);
  websocket_root_dir = websocket_root_dir.AppendASCII("layout_tests")
      .AppendASCII("LayoutTests");
  ui_test_utils::TestWebSocketServer server;
  ASSERT_TRUE(server.Start(websocket_root_dir));
  ASSERT_TRUE(RunExtensionTest("websocket")) << message_;
}
