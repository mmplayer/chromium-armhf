// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_group.h"

#include <algorithm>

#include "base/logging.h"
#include "base/message_loop.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_host.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/appcache/appcache_storage.h"
#include "webkit/appcache/appcache_update_job.h"

namespace appcache {

class AppCacheGroup;

// Use this helper class because we cannot make AppCacheGroup a derived class
// of AppCacheHost::Observer as it would create a circular dependency between
// AppCacheHost and AppCacheGroup.
class AppCacheGroup::HostObserver : public AppCacheHost::Observer {
 public:
  explicit HostObserver(AppCacheGroup* group) : group_(group) {}

  // Methods for AppCacheHost::Observer.
  void OnCacheSelectionComplete(AppCacheHost* host) {}  // N/A
  void OnDestructionImminent(AppCacheHost* host) {
    group_->HostDestructionImminent(host);
  }
 private:
  AppCacheGroup* group_;
};

AppCacheGroup::AppCacheGroup(AppCacheService* service,
                             const GURL& manifest_url,
                             int64 group_id)
    : group_id_(group_id),
      manifest_url_(manifest_url),
      update_status_(IDLE),
      is_obsolete_(false),
      is_being_deleted_(false),
      newest_complete_cache_(NULL),
      update_job_(NULL),
      service_(service),
      restart_update_task_(NULL),
      is_in_dtor_(false) {
  service_->storage()->working_set()->AddGroup(this);
  host_observer_.reset(new HostObserver(this));
}

AppCacheGroup::~AppCacheGroup() {
  DCHECK(old_caches_.empty());
  DCHECK(!newest_complete_cache_);
  DCHECK(!restart_update_task_);
  DCHECK(queued_updates_.empty());

  is_in_dtor_ = true;

  if (update_job_)
    delete update_job_;
  DCHECK_EQ(IDLE, update_status_);

  service_->storage()->working_set()->RemoveGroup(this);
  service_->storage()->DeleteResponses(
      manifest_url_, newly_deletable_response_ids_);
}

void AppCacheGroup::AddUpdateObserver(UpdateObserver* observer) {
  // If observer being added is a host that has been queued for later update,
  // add observer to a different observer list.
  AppCacheHost* host = static_cast<AppCacheHost*>(observer);
  if (queued_updates_.find(host) != queued_updates_.end())
    queued_observers_.AddObserver(observer);
  else
    observers_.AddObserver(observer);
}

void AppCacheGroup::RemoveUpdateObserver(UpdateObserver* observer) {
  observers_.RemoveObserver(observer);
  queued_observers_.RemoveObserver(observer);
}

void AppCacheGroup::AddCache(AppCache* complete_cache) {
  DCHECK(complete_cache->is_complete());
  complete_cache->set_owning_group(this);

  if (!newest_complete_cache_) {
    newest_complete_cache_ = complete_cache;
    return;
  }

  if (complete_cache->IsNewerThan(newest_complete_cache_)) {
    old_caches_.push_back(newest_complete_cache_);
    newest_complete_cache_ = complete_cache;

    // Update hosts of older caches to add a reference to the newest cache.
    for (Caches::iterator it = old_caches_.begin();
         it != old_caches_.end(); ++it) {
      AppCache::AppCacheHosts& hosts = (*it)->associated_hosts();
      for (AppCache::AppCacheHosts::iterator host_it = hosts.begin();
           host_it != hosts.end(); ++host_it) {
        (*host_it)->SetSwappableCache(this);
      }
    }
  } else {
    old_caches_.push_back(complete_cache);
  }
}

void AppCacheGroup::RemoveCache(AppCache* cache) {
  DCHECK(cache->associated_hosts().empty());
  if (cache == newest_complete_cache_) {
    AppCache* tmp_cache = newest_complete_cache_;
    newest_complete_cache_ = NULL;
    tmp_cache->set_owning_group(NULL);  // may cause this group to be deleted
  } else {
    scoped_refptr<AppCacheGroup> protect(this);

    Caches::iterator it =
        std::find(old_caches_.begin(), old_caches_.end(), cache);
    if (it != old_caches_.end()) {
      AppCache* tmp_cache = *it;
      old_caches_.erase(it);
      tmp_cache->set_owning_group(NULL);  // may cause group to be released
    }

    if (!is_obsolete() && old_caches_.empty() &&
        !newly_deletable_response_ids_.empty()) {
      service_->storage()->DeleteResponses(
          manifest_url_, newly_deletable_response_ids_);
      newly_deletable_response_ids_.clear();
    }
  }
}

void AppCacheGroup::AddNewlyDeletableResponseIds(
    std::vector<int64>* response_ids) {
  if (is_being_deleted() || (!is_obsolete() && old_caches_.empty())) {
    service_->storage()->DeleteResponses(manifest_url_, *response_ids);
    response_ids->clear();
    return;
  }

  if (newly_deletable_response_ids_.empty()) {
    newly_deletable_response_ids_.swap(*response_ids);
    return;
  }
  newly_deletable_response_ids_.insert(
      newly_deletable_response_ids_.end(),
      response_ids->begin(), response_ids->end());
  response_ids->clear();
}

void AppCacheGroup::StartUpdateWithNewMasterEntry(
    AppCacheHost* host, const GURL& new_master_resource) {
  DCHECK(!is_obsolete() && !is_being_deleted());
  if (is_in_dtor_)
    return;

  if (!update_job_)
    update_job_ = new AppCacheUpdateJob(service_, this);

  update_job_->StartUpdate(host, new_master_resource);

  // Run queued update immediately as an update has been started manually.
  if (restart_update_task_) {
    restart_update_task_->Cancel();
    restart_update_task_ = NULL;
    RunQueuedUpdates();
  }
}

void AppCacheGroup::CancelUpdate() {
  if (update_job_) {
    delete update_job_;
    DCHECK(!update_job_);
    DCHECK_EQ(IDLE, update_status_);
  }
}

void AppCacheGroup::QueueUpdate(AppCacheHost* host,
                                const GURL& new_master_resource) {
  DCHECK(update_job_ && host && !new_master_resource.is_empty());
  queued_updates_.insert(QueuedUpdates::value_type(host, new_master_resource));

  // Need to know when host is destroyed.
  host->AddObserver(host_observer_.get());

  // If host is already observing for updates, move host to queued observers
  // list so that host is not notified when the current update completes.
  if (FindObserver(host, observers_)) {
    observers_.RemoveObserver(host);
    queued_observers_.AddObserver(host);
  }
}

void AppCacheGroup::RunQueuedUpdates() {
  if (restart_update_task_)
    restart_update_task_ = NULL;

  if (queued_updates_.empty())
    return;

  QueuedUpdates updates_to_run;
  queued_updates_.swap(updates_to_run);
  DCHECK(queued_updates_.empty());

  for (QueuedUpdates::iterator it = updates_to_run.begin();
       it != updates_to_run.end(); ++it) {
    AppCacheHost* host = it->first;
    host->RemoveObserver(host_observer_.get());
    if (FindObserver(host, queued_observers_)) {
      queued_observers_.RemoveObserver(host);
      observers_.AddObserver(host);
    }

    if (!is_obsolete() && !is_being_deleted())
      StartUpdateWithNewMasterEntry(host, it->second);
  }
}

bool AppCacheGroup::FindObserver(UpdateObserver* find_me,
    const ObserverList<UpdateObserver>& observer_list) {
  return observer_list.HasObserver(find_me);
}

void AppCacheGroup::ScheduleUpdateRestart(int delay_ms) {
  DCHECK(!restart_update_task_);
  restart_update_task_ =
      NewRunnableMethod(this, &AppCacheGroup::RunQueuedUpdates);
  MessageLoop::current()->PostDelayedTask(FROM_HERE, restart_update_task_,
                                          delay_ms);
}

void AppCacheGroup::HostDestructionImminent(AppCacheHost* host) {
  queued_updates_.erase(host);
  if (queued_updates_.empty() && restart_update_task_) {
    restart_update_task_->Cancel();
    restart_update_task_ = NULL;
  }
}

void AppCacheGroup::SetUpdateStatus(UpdateStatus status) {
  if (status == update_status_)
    return;

  update_status_ = status;

  if (status != IDLE) {
    DCHECK(update_job_);
  } else {
    update_job_ = NULL;

    // Observers may release us in these callbacks, so we protect against
    // deletion by adding an extra ref in this scope (but only if we're not
    // in our destructor).
    scoped_refptr<AppCacheGroup> protect(is_in_dtor_ ? NULL : this);
    FOR_EACH_OBSERVER(UpdateObserver, observers_, OnUpdateComplete(this));
    if (!queued_updates_.empty())
      ScheduleUpdateRestart(kUpdateRestartDelayMs);
  }
}

}  // namespace appcache
