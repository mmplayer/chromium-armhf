// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_QUOTA_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_QUOTA_INTERNALS_HANDLER_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "content/browser/webui/web_ui.h"
#include "webkit/quota/quota_types.h"

namespace base {
class Value;
class ListValue;
}

namespace quota_internals {

class QuotaInternalsProxy;
class GlobalStorageInfo;
class PerHostStorageInfo;
class PerOriginStorageInfo;
typedef std::map<std::string, std::string> Statistics;

// This class handles message from WebUI page of chrome://quota-internals/.
// All methods in this class should be called on UI thread.
class QuotaInternalsHandler : public WebUIMessageHandler {
 public:
  QuotaInternalsHandler();
  virtual ~QuotaInternalsHandler();
  virtual void RegisterMessages() OVERRIDE;

  // Called by QuotaInternalsProxy to report information to WebUI page.
  void ReportAvailableSpace(int64 available_space);
  void ReportGlobalInfo(const GlobalStorageInfo& data);
  void ReportPerHostInfo(const std::vector<PerHostStorageInfo>& hosts);
  void ReportPerOriginInfo(const std::vector<PerOriginStorageInfo>& origins);
  void ReportStatistics(const Statistics& stats);

 private:
  void OnRequestInfo(const base::ListValue*);
  void SendMessage(const std::string& message, const base::Value& value);

  scoped_refptr<QuotaInternalsProxy> proxy_;

  DISALLOW_COPY_AND_ASSIGN(QuotaInternalsHandler);
};
}  // quota_internals

#endif  // CHROME_BROWSER_UI_WEBUI_QUOTA_INTERNALS_HANDLER_H_
