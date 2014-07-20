// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/webdata/web_data_service.h"

// Internal object representing all data associated with a single query.
struct WebIntentsRegistry::IntentsQuery {
  // Unique query identifier.
  QueryID query_id_;

  // Underlying data query.
  WebDataService::Handle pending_query_;

  // The consumer for this particular query.
  Consumer* consumer_;

  // The particular action to filter for while searching through extensions.
  // If |action_| is empty, return all extension-provided intents.
  string16 action_;

  // TODO(groby): Additional filter data will go here - filtering is handled
  // per query.
};

WebIntentsRegistry::WebIntentsRegistry() : next_query_id_(0) {}

WebIntentsRegistry::~WebIntentsRegistry() {
  // Cancel all pending queries, since we can't handle them any more.
  for (QueryMap::iterator it(queries_.begin()); it != queries_.end(); ++it) {
    wds_->CancelRequest(it->first);
    delete it->second;
  }
}

void WebIntentsRegistry::Initialize(
    scoped_refptr<WebDataService> wds,
    ExtensionServiceInterface* extension_service) {
  wds_ = wds;
  extension_service_ = extension_service;
}

void WebIntentsRegistry::OnWebDataServiceRequestDone(
    WebDataService::Handle h,
    const WDTypedResult* result) {
  DCHECK(result);
  DCHECK(result->GetType() == WEB_INTENTS_RESULT);

  QueryMap::iterator it = queries_.find(h);
  DCHECK(it != queries_.end());

  IntentsQuery* query(it->second);
  DCHECK(query);
  queries_.erase(it);

  // TODO(groby): Filtering goes here.
  IntentList matching_intents = static_cast<
      const WDResult<IntentList>*>(result)->GetValue();

  // Loop over all intents in all extensions, collect ones matching the query.
  if (extension_service_) {
    const ExtensionList* extensions = extension_service_->extensions();
    if (extensions) {
      for (ExtensionList::const_iterator i(extensions->begin());
           i != extensions->end(); ++i) {
        const IntentList& intents((*i)->intents());
        for (IntentList::const_iterator j(intents.begin());
             j != intents.end(); ++j) {
          if (query->action_.empty() || query->action_ == j->action)
            matching_intents.push_back(*j);
        }
      }
    }
  }

  query->consumer_->OnIntentsQueryDone(query->query_id_, matching_intents);
  delete query;
}

WebIntentsRegistry::QueryID WebIntentsRegistry::GetIntentProviders(
    const string16& action,
    Consumer* consumer) {
  DCHECK(consumer);
  DCHECK(wds_.get());

  IntentsQuery* query = new IntentsQuery;
  query->query_id_ = next_query_id_++;
  query->consumer_ = consumer;
  query->action_ = action;
  query->pending_query_ = wds_->GetWebIntents(action, this);
  queries_[query->pending_query_] = query;

  return query->query_id_;
}

WebIntentsRegistry::QueryID WebIntentsRegistry::GetAllIntentProviders(
    Consumer* consumer) {
  DCHECK(consumer);
  DCHECK(wds_.get());

  IntentsQuery* query = new IntentsQuery;
  query->query_id_ = next_query_id_++;
  query->consumer_ = consumer;
  query->pending_query_ = wds_->GetAllWebIntents(this);
  queries_[query->pending_query_] = query;

  return query->query_id_;
}

void WebIntentsRegistry::RegisterIntentProvider(
    const WebIntentServiceData& service) {
  DCHECK(wds_.get());
  wds_->AddWebIntent(service);
}

void WebIntentsRegistry::UnregisterIntentProvider(
    const WebIntentServiceData& service) {
  DCHECK(wds_.get());
  wds_->RemoveWebIntent(service);
}
