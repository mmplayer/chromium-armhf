// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_request_handler.h"

#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_policy.h"
#include "webkit/appcache/appcache_url_request_job.h"

namespace appcache {

AppCacheRequestHandler::AppCacheRequestHandler(AppCacheHost* host,
                                               ResourceType::Type resource_type)
    : host_(host), resource_type_(resource_type),
      is_waiting_for_cache_selection_(false), found_cache_id_(0),
      found_network_namespace_(false), cache_entry_not_found_(false) {
  DCHECK(host_);
  host_->AddObserver(this);
}

AppCacheRequestHandler::~AppCacheRequestHandler() {
  if (host_) {
    storage()->CancelDelegateCallbacks(this);
    host_->RemoveObserver(this);
  }
}

AppCacheStorage* AppCacheRequestHandler::storage() const {
  DCHECK(host_);
  return host_->service()->storage();
}

void AppCacheRequestHandler::GetExtraResponseInfo(
    int64* cache_id, GURL* manifest_url) {
  if (job_ && job_->is_delivering_appcache_response()) {
    *cache_id = job_->cache_id();
    *manifest_url = job_->manifest_url();
  }
}

AppCacheURLRequestJob* AppCacheRequestHandler::MaybeLoadResource(
    net::URLRequest* request) {
  if (!host_ || !IsSchemeAndMethodSupported(request) || cache_entry_not_found_)
    return NULL;

  // This method can get called multiple times over the life
  // of a request. The case we detect here is having scheduled
  // delivery of a "network response" using a job setup on an
  // earlier call thru this method. To send the request thru
  // to the network involves restarting the request altogether,
  // which will call thru to our interception layer again.
  // This time thru, we return NULL so the request hits the wire.
  if (job_) {
    DCHECK(job_->is_delivering_network_response() ||
           job_->cache_entry_not_found());
    if (job_->cache_entry_not_found())
      cache_entry_not_found_ = true;
    job_ = NULL;
    storage()->CancelDelegateCallbacks(this);
    return NULL;
  }

  // Clear out our 'found' fields since we're starting a request for a
  // new resource, any values in those fields are no longer valid.
  found_entry_ = AppCacheEntry();
  found_fallback_entry_ = AppCacheEntry();
  found_cache_id_ = kNoCacheId;
  found_manifest_url_ = GURL();
  found_network_namespace_ = false;

  if (is_main_resource())
    MaybeLoadMainResource(request);
  else
    MaybeLoadSubResource(request);

  // If its been setup to deliver a network response, we can just delete
  // it now and return NULL instead to achieve that since it couldn't
  // have been started yet.
  if (job_ && job_->is_delivering_network_response()) {
    DCHECK(!job_->has_been_started());
    job_ = NULL;
  }

  return job_;
}

AppCacheURLRequestJob* AppCacheRequestHandler::MaybeLoadFallbackForRedirect(
    net::URLRequest* request, const GURL& location) {
  if (!host_ || !IsSchemeAndMethodSupported(request) || cache_entry_not_found_)
    return NULL;
  if (is_main_resource())
    return NULL;
  if (request->url().GetOrigin() == location.GetOrigin())
    return NULL;

  DCHECK(!job_);  // our jobs never generate redirects

  if (found_fallback_entry_.has_response_id()) {
    // 6.9.6, step 4: If this results in a redirect to another origin,
    // get the resource of the fallback entry.
    job_ = new AppCacheURLRequestJob(request, storage());
    DeliverAppCachedResponse(
        found_fallback_entry_, found_cache_id_, found_manifest_url_,
        true, found_fallback_url_);
  } else if (!found_network_namespace_) {
    // 6.9.6, step 6: Fail the resource load.
    job_ = new AppCacheURLRequestJob(request, storage());
    DeliverErrorResponse();
  } else {
    // 6.9.6 step 3 and 5: Fetch the resource normally.
  }

  return job_;
}

AppCacheURLRequestJob* AppCacheRequestHandler::MaybeLoadFallbackForResponse(
    net::URLRequest* request) {
  if (!host_ || !IsSchemeAndMethodSupported(request) || cache_entry_not_found_)
    return NULL;
  if (!found_fallback_entry_.has_response_id())
    return NULL;

  if (request->status().status() == net::URLRequestStatus::CANCELED ||
      request->status().status() == net::URLRequestStatus::HANDLED_EXTERNALLY) {
    // 6.9.6, step 4: But not if the user canceled the download.
    return NULL;
  }

  // We don't fallback for responses that we delivered.
  if (job_) {
    DCHECK(!job_->is_delivering_network_response());
    return NULL;
  }

  if (request->status().is_success()) {
    int code_major = request->GetResponseCode() / 100;
    if (code_major !=4 && code_major != 5)
      return NULL;

    // Servers can override the fallback behavior with a response header.
    const std::string kFallbackOverrideHeader(
        "x-chromium-appcache-fallback-override");
    const std::string kFallbackOverrideValue(
        "disallow-fallback");
    std::string header_value;
    request->GetResponseHeaderByName(kFallbackOverrideHeader, &header_value);
    if (header_value == kFallbackOverrideValue)
      return NULL;
  }

  // 6.9.6, step 4: If this results in a 4xx or 5xx status code
  // or there were network errors, get the resource of the fallback entry.
  job_ = new AppCacheURLRequestJob(request, storage());
  DeliverAppCachedResponse(
      found_fallback_entry_, found_cache_id_,  found_manifest_url_,
      true, found_fallback_url_);
  return job_;
}

void AppCacheRequestHandler::OnDestructionImminent(AppCacheHost* host) {
  storage()->CancelDelegateCallbacks(this);
  host_ = NULL;  // no need to RemoveObserver, the host is being deleted

  // Since the host is being deleted, we don't have to complete any job
  // that is current running. It's destined for the bit bucket anyway.
  if (job_) {
    job_->Kill();
    job_ = NULL;
  }
}

void AppCacheRequestHandler::DeliverAppCachedResponse(
    const AppCacheEntry& entry, int64 cache_id, const GURL& manifest_url,
    bool is_fallback, const GURL& fallback_url) {
  DCHECK(host_ && job_ && job_->is_waiting());
  DCHECK(entry.has_response_id());

  if (ResourceType::IsFrame(resource_type_) && is_fallback) {
    DCHECK(!fallback_url.is_empty());
    host_->NotifyMainResourceFallback(fallback_url);
  }

  job_->DeliverAppCachedResponse(manifest_url, cache_id, entry, is_fallback);
}

void AppCacheRequestHandler::DeliverErrorResponse() {
  DCHECK(job_ && job_->is_waiting());
  job_->DeliverErrorResponse();
}

void AppCacheRequestHandler::DeliverNetworkResponse() {
  DCHECK(job_ && job_->is_waiting());
  job_->DeliverNetworkResponse();
}

// Main-resource handling ----------------------------------------------

void AppCacheRequestHandler::MaybeLoadMainResource(net::URLRequest* request) {
  DCHECK(!job_);
  DCHECK(host_);

  const AppCacheHost* spawning_host =
      ResourceType::IsSharedWorker(resource_type_) ?
          host_ : host_->GetSpawningHost();
  GURL preferred_manifest_url = spawning_host ?
      spawning_host->preferred_manifest_url() : GURL();

  // We may have to wait for our storage query to complete, but
  // this query can also complete syncrhonously.
  job_ = new AppCacheURLRequestJob(request, storage());
  storage()->FindResponseForMainRequest(
      request->url(), preferred_manifest_url, this);
}

void AppCacheRequestHandler::OnMainResponseFound(
    const GURL& url, const AppCacheEntry& entry,
    const GURL& fallback_url, const AppCacheEntry& fallback_entry,
    int64 cache_id, const GURL& manifest_url) {
  DCHECK(job_);
  DCHECK(host_);
  DCHECK(is_main_resource());
  DCHECK(!entry.IsForeign());
  DCHECK(!fallback_entry.IsForeign());
  DCHECK(!(entry.has_response_id() && fallback_entry.has_response_id()));

  if (!job_)
    return;

  AppCachePolicy* policy = host_->service()->appcache_policy();
  bool was_blocked_by_policy = !manifest_url.is_empty() && policy &&
      !policy->CanLoadAppCache(manifest_url, host_->first_party_url());

  if (was_blocked_by_policy) {
    if (ResourceType::IsFrame(resource_type_)) {
      host_->NotifyMainResourceBlocked(manifest_url);
    } else {
      DCHECK(ResourceType::IsSharedWorker(resource_type_));
      host_->frontend()->OnContentBlocked(host_->host_id(), manifest_url);
    }
    DeliverNetworkResponse();
    return;
  }

  if (ResourceType::IsFrame(resource_type_) && cache_id != kNoCacheId) {
    // AppCacheHost loads and holds a reference to the main resource cache
    // for two reasons, firstly to preload the cache into the working set
    // in advance of subresource loads happening, secondly to prevent the
    // AppCache from falling out of the working set on frame navigations.
    host_->LoadMainResourceCache(cache_id);
    host_->set_preferred_manifest_url(manifest_url);
  }

  // 6.11.1 Navigating across documents, steps 10 and 14.

  found_entry_ = entry;
  found_fallback_url_ = fallback_url;
  found_fallback_entry_ = fallback_entry;
  found_cache_id_ = cache_id;
  found_manifest_url_ = manifest_url;
  found_network_namespace_ = false;  // not applicable to main requests

  if (found_entry_.has_response_id()) {
    DeliverAppCachedResponse(
        found_entry_, found_cache_id_, found_manifest_url_,
        false, GURL());
  } else {
    DeliverNetworkResponse();
  }
}

// Sub-resource handling ----------------------------------------------

void AppCacheRequestHandler::MaybeLoadSubResource(
    net::URLRequest* request) {
  DCHECK(!job_);

  if (host_->is_selection_pending()) {
    // We have to wait until cache selection is complete and the
    // selected cache is loaded.
    is_waiting_for_cache_selection_ = true;
    job_ = new AppCacheURLRequestJob(request, storage());
    return;
  }

  if (!host_->associated_cache() ||
      !host_->associated_cache()->is_complete()) {
    return;
  }

  job_ = new AppCacheURLRequestJob(request, storage());
  ContinueMaybeLoadSubResource();
}

void AppCacheRequestHandler::ContinueMaybeLoadSubResource() {
  // 6.9.6 Changes to the networking model
  // If the resource is not to be fetched using the HTTP GET mechanism or
  // equivalent ... then fetch the resource normally.
  DCHECK(job_);
  DCHECK(host_->associated_cache() &&
         host_->associated_cache()->is_complete());

  const GURL& url = job_->request()->url();
  AppCache* cache = host_->associated_cache();
  storage()->FindResponseForSubRequest(
      host_->associated_cache(), url,
      &found_entry_, &found_fallback_entry_, &found_network_namespace_);

  if (found_entry_.has_response_id()) {
    // Step 2: If there's an entry, get it instead.
    DCHECK(!found_network_namespace_ &&
           !found_fallback_entry_.has_response_id());
    found_cache_id_ = cache->cache_id();
    found_manifest_url_ = cache->owning_group()->manifest_url();
    DeliverAppCachedResponse(
        found_entry_, found_cache_id_, found_manifest_url_,
        false, GURL());
    return;
  }

  if (found_fallback_entry_.has_response_id()) {
    // Step 4: Fetch the resource normally, if this results
    // in certain conditions, then use the fallback.
    DCHECK(!found_network_namespace_ &&
           !found_entry_.has_response_id());
    found_cache_id_ = cache->cache_id();
    found_manifest_url_ = cache->owning_group()->manifest_url();
    DeliverNetworkResponse();
    return;
  }

  if (found_network_namespace_) {
    // Step 3 and 5: Fetch the resource normally.
    DCHECK(!found_entry_.has_response_id() &&
           !found_fallback_entry_.has_response_id());
    DeliverNetworkResponse();
    return;
  }

  // Step 6: Fail the resource load.
  DeliverErrorResponse();
}

void AppCacheRequestHandler::OnCacheSelectionComplete(AppCacheHost* host) {
  DCHECK(host == host_);
  if (is_main_resource())
    return;
  if (!is_waiting_for_cache_selection_)
    return;

  is_waiting_for_cache_selection_ = false;

  if (!host_->associated_cache() ||
      !host_->associated_cache()->is_complete()) {
    DeliverNetworkResponse();
    return;
  }

  ContinueMaybeLoadSubResource();
}

}  // namespace appcache
