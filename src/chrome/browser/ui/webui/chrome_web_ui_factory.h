// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_FACTORY_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "content/browser/webui/web_ui.h"
#include "content/browser/webui/web_ui_factory.h"
#include "chrome/browser/favicon/favicon_service.h"

class WebUI;
class GURL;
class Profile;
class RefCountedMemory;
class TabContents;

class ChromeWebUIFactory : public content::WebUIFactory {
 public:
  virtual WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                     const GURL& url) const OVERRIDE;
  virtual bool UseWebUIForURL(content::BrowserContext* browser_context,
                              const GURL& url) const OVERRIDE;
  virtual bool HasWebUIScheme(const GURL& url) const OVERRIDE;
  virtual bool IsURLAcceptableForWebUI(content::BrowserContext* browser_context,
                                       const GURL& url) const OVERRIDE;
  virtual WebUI* CreateWebUIForURL(TabContents* tab_contents,
                                   const GURL& url) const OVERRIDE;

  // Get the favicon for |page_url| and forward the result to the |request|
  // when loaded.
  void GetFaviconForURL(Profile* profile,
                        FaviconService::GetFaviconRequest* request,
                        const GURL& page_url) const;

  static ChromeWebUIFactory* GetInstance();

 protected:
  ChromeWebUIFactory();
  virtual ~ChromeWebUIFactory();

 private:

  friend struct DefaultSingletonTraits<ChromeWebUIFactory>;

  // Gets the data for the favicon for a WebUI page. Returns NULL if the WebUI
  // does not have a favicon.
  RefCountedMemory* GetFaviconResourceBytes(const GURL& page_url) const;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebUIFactory);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_FACTORY_H_
