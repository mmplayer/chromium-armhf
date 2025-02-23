// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/quota_internals_ui.h"

#include <string>

#include "base/json/json_value_serializer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/quota_internals_handler.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/quota_internals_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"


namespace {

ChromeWebUIDataSource* CreateQuotaInternalsHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIQuotaInternalsHost);

  source->set_json_path("strings.js");
  source->add_resource_path(
      "event_handler.js", IDR_QUOTA_INTERNALS_EVENT_HANDLER_JS);
  source->add_resource_path(
      "message_dispatcher.js", IDR_QUOTA_INTERNALS_MESSAGE_DISPATCHER_JS);
  source->set_default_resource(IDR_QUOTA_INTERNALS_MAIN_HTML);
  return source;
}

}  // namespace

QuotaInternalsUI::QuotaInternalsUI(TabContents* contents)
    : ChromeWebUI(contents) {
  WebUIMessageHandler* handler = new quota_internals::QuotaInternalsHandler;
  AddMessageHandler(handler->Attach(this));
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(
      CreateQuotaInternalsHTMLSource());
}
