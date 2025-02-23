// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/search_terms_data.h"

#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/instant/instant_field_trial.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/rlz/rlz.h"
#endif

SearchTermsData::SearchTermsData() {
}

SearchTermsData::~SearchTermsData() {
}

std::string SearchTermsData::GoogleBaseSuggestURLValue() const {
  // Start with the Google base URL.
  const GURL base_url(GoogleBaseURLValue());
  DCHECK(base_url.is_valid());

  GURL::Replacements repl;

  // Replace any existing path with "/complete/".
  static const std::string suggest_path("/complete/");
  repl.SetPathStr(suggest_path);

  // Clear the query and ref.
  repl.ClearQuery();
  repl.ClearRef();
  return base_url.ReplaceComponents(repl).spec();
}

std::string SearchTermsData::InstantFieldTrialUrlParam() const {
  return std::string();
}

// static
std::string* UIThreadSearchTermsData::google_base_url_ = NULL;

UIThreadSearchTermsData::UIThreadSearchTermsData() : profile_(NULL) {
  // GoogleURLTracker::GoogleURL() DCHECKs this also, but adding it here helps
  // us catch bad behavior at a more common place in this code.
  DCHECK(!BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
}

std::string UIThreadSearchTermsData::GoogleBaseURLValue() const {
  DCHECK(!BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  return google_base_url_ ?
    (*google_base_url_) : GoogleURLTracker::GoogleURL().spec();
}

std::string UIThreadSearchTermsData::GetApplicationLocale() const {
  DCHECK(!BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  return g_browser_process->GetApplicationLocale();
}

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
string16 UIThreadSearchTermsData::GetRlzParameterValue() const {
  DCHECK(!BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  string16 rlz_string;
  // For organic brandcodes do not use rlz at all. Empty brandcode usually
  // means a chromium install. This is ok.
  std::string brand;
  if (google_util::GetBrand(&brand) && !brand.empty() &&
      !google_util::IsOrganic(brand)) {
    // This call will return false the first time(s) it is called until the
    // value has been cached. This normally would mean that at most one omnibox
    // search might not send the RLZ data but this is not really a problem.
    RLZTracker::GetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, &rlz_string);
  }
  return rlz_string;
}
#endif

std::string UIThreadSearchTermsData::InstantFieldTrialUrlParam() const {
  DCHECK(!BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  return InstantFieldTrial::GetGroupAsUrlParam(profile_);
}

// static
void UIThreadSearchTermsData::SetGoogleBaseURL(std::string* google_base_url) {
  delete google_base_url_;
  google_base_url_ = google_base_url;
}
