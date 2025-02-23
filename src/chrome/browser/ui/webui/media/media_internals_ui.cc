// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media/media_internals_ui.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/media/media_internals_handler.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

ChromeWebUIDataSource* CreateMediaInternalsHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIMediaInternalsHost);

  source->set_json_path("strings.js");
  source->add_resource_path("media_internals.js", IDR_MEDIA_INTERNALS_JS);
  source->set_default_resource(IDR_MEDIA_INTERNALS_HTML);
  return source;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// MediaInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

MediaInternalsUI::MediaInternalsUI(TabContents* contents)
    : ChromeWebUI(contents) {
  AddMessageHandler((new MediaInternalsMessageHandler())->Attach(this));

  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(
      CreateMediaInternalsHTMLSource());
}
