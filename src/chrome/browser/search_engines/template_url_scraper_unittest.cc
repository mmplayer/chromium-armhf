// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_source.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_util.h"

namespace {
class TemplateURLScraperTest : public InProcessBrowserTest {
 public:
  TemplateURLScraperTest() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TemplateURLScraperTest);
};

class TemplateURLServiceLoader : public NotificationObserver {
 public:
  explicit TemplateURLServiceLoader(TemplateURLService* model) : model_(model) {
    registrar_.Add(this, chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED,
                   Source<TemplateURLService>(model));
    model_->Load();
    ui_test_utils::RunMessageLoop();
  }

  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED &&
        Source<TemplateURLService>(source).ptr() == model_) {
      MessageLoop::current()->Quit();
    }
  }

 private:
  NotificationRegistrar registrar_;

  TemplateURLService* model_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLServiceLoader);
};

}  // namespace

/*
IN_PROC_BROWSER_TEST_F(TemplateURLScraperTest, ScrapeWithOnSubmit) {
  host_resolver()->AddRule("*.foo.com", "localhost");

  TemplateURLService* template_urls =
      TemplateURLServiceFactory::GetInstance(browser()->profile());
  TemplateURLServiceLoader loader(template_urls);

  std::vector<const TemplateURL*> all_urls = template_urls->GetTemplateURLs();

  // We need to substract the default pre-populated engines that the profile is
  // set up with.
  size_t default_index = 0;
  std::vector<TemplateURL*> prepopulate_urls;
  TemplateURLPrepopulateData::GetPrepopulatedEngines(
      browser()->profile()->GetPrefs(),
      &prepopulate_urls,
      &default_index);

  EXPECT_EQ(prepopulate_urls.size(), all_urls.size());

  scoped_refptr<HTTPTestServer> server(
      HTTPTestServer::CreateServerWithFileRootURL(
          L"chrome/test/data/template_url_scraper/submit_handler", L"/",
          g_browser_process->io_thread()->message_loop()));
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("http://www.foo.com:1337/"), 2);

  all_urls = template_urls->GetTemplateURLs();
  EXPECT_EQ(1, all_urls.size() - prepopulate_urls.size());
}
*/
