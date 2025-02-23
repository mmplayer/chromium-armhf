// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intranet_redirect_detector.h"

#include "base/command_line.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_service.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domain.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

const size_t IntranetRedirectDetector::kNumCharsInHostnames = 10;

IntranetRedirectDetector::IntranetRedirectDetector()
    : redirect_origin_(g_browser_process->local_state()->GetString(
          prefs::kLastKnownIntranetRedirectOrigin)),
      ALLOW_THIS_IN_INITIALIZER_LIST(fetcher_factory_(this)),
      in_sleep_(true) {
  // Because this function can be called during startup, when kicking off a URL
  // fetch can eat up 20 ms of time, we delay seven seconds, which is hopefully
  // long enough to be after startup, but still get results back quickly.
  // Ideally, instead of this timer, we'd do something like "check if the
  // browser is starting up, and if so, come back later", but there is currently
  // no function to do this.
  static const int kStartFetchDelayMS = 7000;
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      fetcher_factory_.NewRunnableMethod(
          &IntranetRedirectDetector::FinishSleep),
      kStartFetchDelayMS);

  net::NetworkChangeNotifier::AddIPAddressObserver(this);
}

IntranetRedirectDetector::~IntranetRedirectDetector() {
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
  STLDeleteElements(&fetchers_);
}

// static
GURL IntranetRedirectDetector::RedirectOrigin() {
  const IntranetRedirectDetector* const detector =
      g_browser_process->intranet_redirect_detector();
  return detector ? detector->redirect_origin_ : GURL();
}

// static
void IntranetRedirectDetector::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kLastKnownIntranetRedirectOrigin,
                            std::string());
}

void IntranetRedirectDetector::FinishSleep() {
  in_sleep_ = false;

  // If another fetch operation is still running, cancel it.
  STLDeleteElements(&fetchers_);
  resulting_origins_.clear();

  // The detector is not needed in Chrome Frame since we have no omnibox there.
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kDisableBackgroundNetworking) ||
      cmd_line->HasSwitch(switches::kChromeFrame))
    return;

  DCHECK(fetchers_.empty() && resulting_origins_.empty());

  // Start three fetchers on random hostnames.
  for (size_t i = 0; i < 3; ++i) {
    std::string url_string("http://");
    for (size_t j = 0; j < kNumCharsInHostnames; ++j)
      url_string += ('a' + base::RandInt(0, 'z' - 'a'));
    GURL random_url(url_string + '/');
    URLFetcher* fetcher = new URLFetcher(random_url, URLFetcher::HEAD, this);
    // We don't want these fetches to affect existing state in the profile.
    fetcher->set_load_flags(net::LOAD_DISABLE_CACHE |
                            net::LOAD_DO_NOT_SAVE_COOKIES);
    fetcher->set_request_context(g_browser_process->system_request_context());
    fetcher->Start();
    fetchers_.insert(fetcher);
  }
}

void IntranetRedirectDetector::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const net::ResponseCookies& cookies,
    const std::string& data) {
  // Delete the fetcher on this function's exit.
  Fetchers::iterator fetcher = fetchers_.find(const_cast<URLFetcher*>(source));
  DCHECK(fetcher != fetchers_.end());
  scoped_ptr<URLFetcher> clean_up_fetcher(*fetcher);
  fetchers_.erase(fetcher);

  // If any two fetches result in the same domain/host, we set the redirect
  // origin to that; otherwise we set it to nothing.
  if (!status.is_success() || (response_code != 200)) {
    if ((resulting_origins_.empty()) ||
        ((resulting_origins_.size() == 1) &&
         resulting_origins_.front().is_valid())) {
      resulting_origins_.push_back(GURL());
      return;
    }
    redirect_origin_ = GURL();
  } else {
    DCHECK(url.is_valid());
    GURL origin(url.GetOrigin());
    if (resulting_origins_.empty()) {
      resulting_origins_.push_back(origin);
      return;
    }
    if (net::RegistryControlledDomainService::SameDomainOrHost(
        resulting_origins_.front(), origin)) {
      redirect_origin_ = origin;
      if (!fetchers_.empty()) {
        // Cancel remaining fetch, we don't need it.
        DCHECK(fetchers_.size() == 1);
        delete (*fetchers_.begin());
        fetchers_.clear();
      }
    }
    if (resulting_origins_.size() == 1) {
      resulting_origins_.push_back(origin);
      return;
    }
    DCHECK(resulting_origins_.size() == 2);
    redirect_origin_ = net::RegistryControlledDomainService::SameDomainOrHost(
        resulting_origins_.back(), origin) ? origin : GURL();
  }

  g_browser_process->local_state()->SetString(
      prefs::kLastKnownIntranetRedirectOrigin, redirect_origin_.is_valid() ?
          redirect_origin_.spec() : std::string());
}

void IntranetRedirectDetector::OnIPAddressChanged() {
  // If a request is already scheduled, do not scheduled yet another one.
  if (in_sleep_)
    return;

  // Since presumably many programs open connections after network changes,
  // delay this a little bit.
  in_sleep_ = true;
  static const int kNetworkSwitchDelayMS = 1000;
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      fetcher_factory_.NewRunnableMethod(
          &IntranetRedirectDetector::FinishSleep),
      kNetworkSwitchDelayMS);
}

IntranetRedirectHostResolverProc::IntranetRedirectHostResolverProc(
    net::HostResolverProc* previous)
    : net::HostResolverProc(previous) {
}

int IntranetRedirectHostResolverProc::Resolve(
    const std::string& host,
    net::AddressFamily address_family,
    net::HostResolverFlags host_resolver_flags,
    net::AddressList* addrlist,
    int* os_error) {
  // We'd love to just ask the IntranetRedirectDetector, but we may not be on
  // the same thread.  So just use the heuristic that any all-lowercase a-z
  // hostname with the right number of characters is likely from the detector
  // (and thus should be blocked).
  return ((host.length() == IntranetRedirectDetector::kNumCharsInHostnames) &&
      (host.find_first_not_of("abcdefghijklmnopqrstuvwxyz") ==
          std::string::npos)) ?
      net::ERR_NAME_NOT_RESOLVED :
      ResolveUsingPrevious(host, address_family, host_resolver_flags, addrlist,
                           os_error);
}
