// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_REMOTE_DEBUGGING_SERVER_H_
#define CHROME_BROWSER_DEBUGGER_REMOTE_DEBUGGING_SERVER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/browser/debugger/devtools_http_protocol_handler.h"

class DevToolsHttpProtocolHandler;
class Profile;

class RemoteDebuggingServer {
 public:
  RemoteDebuggingServer(Profile* profile,
                             const std::string& ip,
                             int port,
                             const std::string& frontend_url);

  virtual ~RemoteDebuggingServer();

 private:
  scoped_refptr<DevToolsHttpProtocolHandler> devtools_http_handler_;
  DISALLOW_COPY_AND_ASSIGN(RemoteDebuggingServer);
};

#endif  // CHROME_BROWSER_DEBUGGER_REMOTE_DEBUGGING_SERVER_H_
