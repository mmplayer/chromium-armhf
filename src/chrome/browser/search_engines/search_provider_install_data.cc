// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/search_provider_install_data.h"

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/task.h"
#include "chrome/browser/search_engines/search_host_to_urls_map.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/util.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"

typedef SearchHostToURLsMap::TemplateURLSet TemplateURLSet;

namespace {

// Implementation of SearchTermsData that may be used on the I/O thread.
class IOThreadSearchTermsData : public SearchTermsData {
 public:
  explicit IOThreadSearchTermsData(const std::string& google_base_url);

  // Implementation of SearchTermsData.
  virtual std::string GoogleBaseURLValue() const;
  virtual std::string GetApplicationLocale() const;
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  virtual string16 GetRlzParameterValue() const {
    // This value doesn't matter for our purposes.
    return string16();
  }
#endif

 private:
  std::string google_base_url_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadSearchTermsData);
};

IOThreadSearchTermsData::IOThreadSearchTermsData(
    const std::string& google_base_url) : google_base_url_(google_base_url) {
}

std::string IOThreadSearchTermsData::GoogleBaseURLValue() const {
  return google_base_url_;
}

std::string IOThreadSearchTermsData::GetApplicationLocale() const {
  // This value doesn't matter for our purposes.
  return "yy";
}

// Handles telling SearchProviderInstallData about changes to the google base
// url. (Ensure that this is deleted on the I/O thread so that the WeakPtr is
// deleted on the correct thread.)
class GoogleURLChangeNotifier
    : public base::RefCountedThreadSafe<GoogleURLChangeNotifier,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  explicit GoogleURLChangeNotifier(
      const base::WeakPtr<SearchProviderInstallData>& install_data);

  // Called on the I/O thread with the Google base URL whenever the value
  // changes.
  void OnChange(const std::string& google_base_url);

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class DeleteTask<GoogleURLChangeNotifier>;

  ~GoogleURLChangeNotifier() {}

  base::WeakPtr<SearchProviderInstallData> install_data_;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLChangeNotifier);
};

GoogleURLChangeNotifier::GoogleURLChangeNotifier(
    const base::WeakPtr<SearchProviderInstallData>& install_data)
    : install_data_(install_data) {
}

void GoogleURLChangeNotifier::OnChange(const std::string& google_base_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (install_data_)
    install_data_->OnGoogleURLChange(google_base_url);
}

// Notices changes in the Google base URL and sends them along
// to the SearchProviderInstallData on the I/O thread.
class GoogleURLObserver : public NotificationObserver {
 public:
  GoogleURLObserver(
      GoogleURLChangeNotifier* change_notifier,
      int ui_death_notification,
      const NotificationSource& ui_death_source);

  // Implementation of NotificationObserver.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  virtual ~GoogleURLObserver() {}

  scoped_refptr<GoogleURLChangeNotifier> change_notifier_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(GoogleURLObserver);
};

GoogleURLObserver::GoogleURLObserver(
      GoogleURLChangeNotifier* change_notifier,
      int ui_death_notification,
      const NotificationSource& ui_death_source)
    : change_notifier_(change_notifier) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
                 NotificationService::AllSources());
  registrar_.Add(this, ui_death_notification, ui_death_source);
}

void GoogleURLObserver::Observe(int type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_GOOGLE_URL_UPDATED) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(change_notifier_.get(),
                          &GoogleURLChangeNotifier::OnChange,
                          UIThreadSearchTermsData().GoogleBaseURLValue()));
  } else {
    // This must be the death notification.
    delete this;
  }
}

// Indicates if the two inputs have the same security origin.
// |requested_origin| should only be a security origin (no path, etc.).
// It is ok if |template_url| is NULL.
static bool IsSameOrigin(const GURL& requested_origin,
                         const TemplateURL* template_url,
                         const SearchTermsData& search_terms_data) {
  DCHECK(requested_origin == requested_origin.GetOrigin());
  return template_url && requested_origin ==
      TemplateURLService::GenerateSearchURLUsingTermsData(
          template_url,
          search_terms_data).GetOrigin();
}

}  // namespace

SearchProviderInstallData::SearchProviderInstallData(
    WebDataService* web_service,
    int ui_death_notification,
    const NotificationSource& ui_death_source)
    : web_service_(web_service),
      load_handle_(0),
      google_base_url_(UIThreadSearchTermsData().GoogleBaseURLValue()) {
  // GoogleURLObserver is responsible for killing itself when
  // the given notification occurs.
  new GoogleURLObserver(new GoogleURLChangeNotifier(AsWeakPtr()),
                        ui_death_notification, ui_death_source);
  DetachFromThread();
}

SearchProviderInstallData::~SearchProviderInstallData() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (load_handle_) {
    DCHECK(web_service_.get());
    web_service_->CancelRequest(load_handle_);
  }
}

void SearchProviderInstallData::CallWhenLoaded(Task* task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (provider_map_.get()) {
    task->Run();
    delete task;
    return;
  }

  task_queue_.Push(task);
  if (load_handle_)
    return;

  if (web_service_.get())
    load_handle_ = web_service_->GetKeywords(this);
  else
    OnLoadFailed();
}

SearchProviderInstallData::State SearchProviderInstallData::GetInstallState(
    const GURL& requested_origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(provider_map_.get());

  // First check to see if the origin is the default search provider.
  if (requested_origin.spec() == default_search_origin_)
    return INSTALLED_AS_DEFAULT;

  // Is the url any search provider?
  const TemplateURLSet* urls = provider_map_->GetURLsForHost(
      requested_origin.host());
  if (!urls)
    return NOT_INSTALLED;

  IOThreadSearchTermsData search_terms_data(google_base_url_);
  for (TemplateURLSet::const_iterator i = urls->begin();
       i != urls->end(); ++i) {
    const TemplateURL* template_url = *i;
    if (IsSameOrigin(requested_origin, template_url, search_terms_data))
      return INSTALLED_BUT_NOT_DEFAULT;
  }
  return NOT_INSTALLED;
}

void SearchProviderInstallData::OnGoogleURLChange(
    const std::string& google_base_url) {
  google_base_url_ = google_base_url;
}

void SearchProviderInstallData::OnWebDataServiceRequestDone(
    WebDataService::Handle h,
    const WDTypedResult* result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Reset the load_handle so that we don't try and cancel the load in
  // the destructor.
  load_handle_ = 0;

  if (!result) {
    // Results are null if the database went away or (most likely) wasn't
    // loaded.
    OnLoadFailed();
    return;
  }

  const TemplateURL* default_search_provider = NULL;
  int new_resource_keyword_version = 0;
  std::vector<TemplateURL*> extracted_template_urls;
  GetSearchProvidersUsingKeywordResult(*result,
                                       NULL,
                                       NULL,
                                       &extracted_template_urls,
                                       &default_search_provider,
                                       &new_resource_keyword_version);
  template_urls_.get().insert(template_urls_.get().begin(),
                              extracted_template_urls.begin(),
                              extracted_template_urls.end());
  IOThreadSearchTermsData search_terms_data(google_base_url_);
  provider_map_.reset(new SearchHostToURLsMap());
  provider_map_->Init(template_urls_.get(), search_terms_data);
  SetDefault(default_search_provider);
  NotifyLoaded();
}

void SearchProviderInstallData::SetDefault(const TemplateURL* template_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!template_url) {
    default_search_origin_.clear();
    return;
  }

  IOThreadSearchTermsData search_terms_data(google_base_url_);
  const GURL url(TemplateURLService::GenerateSearchURLUsingTermsData(
      template_url, search_terms_data));
  if (!url.is_valid() || !url.has_host()) {
    default_search_origin_.clear();
    return;
  }
  default_search_origin_ = url.GetOrigin().spec();
}

void SearchProviderInstallData::OnLoadFailed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  provider_map_.reset(new SearchHostToURLsMap());
  IOThreadSearchTermsData search_terms_data(google_base_url_);
  provider_map_->Init(template_urls_.get(), search_terms_data);
  SetDefault(NULL);
  NotifyLoaded();
}

void SearchProviderInstallData::NotifyLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  task_queue_.Run();

  // Since we expect this request to be rare, clear out the information. This
  // also keeps the responses current as the search providers change.
  provider_map_.reset();
  SetDefault(NULL);
}
