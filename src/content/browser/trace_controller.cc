// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/trace_controller.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "content/browser/browser_message_filter.h"
#include "content/browser/trace_message_filter.h"
#include "content/common/child_process_messages.h"


TraceController::TraceController() :
    subscriber_(NULL),
    pending_end_ack_count_(0),
    pending_bpf_ack_count_(0),
    maximum_bpf_(0.0f),
    is_tracing_(false),
    is_get_categories_(false) {
  base::debug::TraceLog::GetInstance()->SetOutputCallback(
      base::Bind(&TraceController::OnTraceDataCollected,
                 base::Unretained(this)));
}

TraceController::~TraceController() {
  if (base::debug::TraceLog* trace_log = base::debug::TraceLog::GetInstance())
    trace_log->SetOutputCallback(base::debug::TraceLog::OutputCallback());
}

//static
TraceController* TraceController::GetInstance() {
  return Singleton<TraceController>::get();
}

bool TraceController::GetKnownCategoriesAsync(TraceSubscriber* subscriber) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Known categories come back from child processes with the EndTracingAck
  // message. So to get known categories, just begin and end tracing immediately
  // afterwards. This will ping all the child processes for categories.
  is_get_categories_ = true;
  bool success = BeginTracing(subscriber) && EndTracingAsync(subscriber);
  is_get_categories_ = success;
  return success;
}

bool TraceController::BeginTracing(TraceSubscriber* subscriber) {
  std::vector<std::string> include, exclude;
  // By default, exclude all categories that begin with test_
  exclude.push_back("test_*");
  return BeginTracing(subscriber, include, exclude);
}

bool TraceController::BeginTracing(
    TraceSubscriber* subscriber,
    const std::vector<std::string>& included_categories,
    const std::vector<std::string>& excluded_categories) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_begin_tracing() ||
      (subscriber_ != NULL && subscriber != subscriber_))
    return false;

  subscriber_ = subscriber;
  included_categories_ = included_categories;
  excluded_categories_ = excluded_categories;

  // Enable tracing
  is_tracing_ = true;
  base::debug::TraceLog::GetInstance()->SetEnabled(included_categories,
                                                   excluded_categories);

  // Notify all child processes.
  for (FilterMap::iterator it = filters_.begin(); it != filters_.end(); ++it) {
    it->get()->SendBeginTracing(included_categories, excluded_categories);
  }

  return true;
}

bool TraceController::EndTracingAsync(TraceSubscriber* subscriber) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_end_tracing() || subscriber != subscriber_)
    return false;

  // There could be a case where there are no child processes and filters_
  // is empty. In that case we can immediately tell the subscriber that tracing
  // has ended. To avoid recursive calls back to the subscriber, we will just
  // use the existing asynchronous OnEndTracingAck code.
  // Count myself (local trace) in pending_end_ack_count_, acked below.
  pending_end_ack_count_ = filters_.size() + 1;

  // Handle special case of zero child processes.
  if (pending_end_ack_count_ == 1) {
    // Ack asynchronously now, because we don't have any children to wait for.
    std::vector<std::string> categories;
    base::debug::TraceLog::GetInstance()->GetKnownCategories(&categories);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceController::OnEndTracingAck, base::Unretained(this),
                   categories));
  }

  // Notify all child processes.
  for (FilterMap::iterator it = filters_.begin(); it != filters_.end(); ++it) {
    it->get()->SendEndTracing();
  }

  return true;
}

bool TraceController::GetTraceBufferPercentFullAsync(
    TraceSubscriber* subscriber) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_get_buffer_percent_full() || subscriber != subscriber_)
    return false;

  maximum_bpf_ = 0.0f;
  pending_bpf_ack_count_ = filters_.size() + 1;

  // Handle special case of zero child processes.
  if (pending_bpf_ack_count_ == 1) {
    // Ack asynchronously now, because we don't have any children to wait for.
    float bpf = base::debug::TraceLog::GetInstance()->GetBufferPercentFull();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceController::OnTraceBufferPercentFullReply,
                   base::Unretained(this), bpf));
  }

  // Message all child processes.
  for (FilterMap::iterator it = filters_.begin(); it != filters_.end(); ++it) {
    it->get()->SendGetTraceBufferPercentFull();
  }

  return true;
}

void TraceController::CancelSubscriber(TraceSubscriber* subscriber) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (subscriber == subscriber_) {
    subscriber_ = NULL;
    // End tracing if necessary.
    if (is_tracing_ && pending_end_ack_count_ == 0)
      EndTracingAsync(NULL);
  }
}

void TraceController::AddFilter(TraceMessageFilter* filter) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceController::AddFilter, base::Unretained(this),
                   make_scoped_refptr(filter)));
    return;
  }

  filters_.insert(filter);
  if (is_tracing_enabled()) {
    filter->SendBeginTracing(included_categories_, excluded_categories_);
  }
}

void TraceController::RemoveFilter(TraceMessageFilter* filter) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceController::RemoveFilter, base::Unretained(this),
                   make_scoped_refptr(filter)));
    return;
  }

  filters_.erase(filter);
}

void TraceController::OnEndTracingAck(
    const std::vector<std::string>& known_categories) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceController::OnEndTracingAck, base::Unretained(this),
                   known_categories));
    return;
  }

  // Merge known_categories with known_categories_
  known_categories_.insert(known_categories.begin(), known_categories.end());

  if (pending_end_ack_count_ == 0)
    return;

  if (--pending_end_ack_count_ == 0) {
    // All acks have been received.
    is_tracing_ = false;

    // Disable local trace. During this call, our OnTraceDataCollected will be
    // called with the last of the local trace data. Since we are on the UI
    // thread, the call to OnTraceDataCollected will be synchronous, so we can
    // immediately call OnEndTracingComplete below.
    base::debug::TraceLog::GetInstance()->SetEnabled(false);

    // Trigger callback if one is set.
    if (subscriber_) {
      if (is_get_categories_)
        subscriber_->OnKnownCategoriesCollected(known_categories_);
      else
        subscriber_->OnEndTracingComplete();
      // Clear subscriber so that others can use TraceController.
      subscriber_ = NULL;
    }

    is_get_categories_ = false;
  }

  if (pending_end_ack_count_ == 1) {
    // The last ack represents local trace, so we need to ack it now. Note that
    // this code only executes if there were child processes.
    std::vector<std::string> categories;
    base::debug::TraceLog::GetInstance()->GetKnownCategories(&categories);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceController::OnEndTracingAck, base::Unretained(this),
                   categories));
  }
}

void TraceController::OnTraceDataCollected(
    const scoped_refptr<base::debug::TraceLog::RefCountedString>&
        json_events_str_ptr) {
  // OnTraceDataCollected may be called from any browser thread, either by the
  // local event trace system or from child processes via TraceMessageFilter.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceController::OnTraceDataCollected,
                   base::Unretained(this), json_events_str_ptr));
    return;
  }

  // Drop trace events if we are just getting categories.
  if (subscriber_ && !is_get_categories_)
    subscriber_->OnTraceDataCollected(json_events_str_ptr->data);
}

void TraceController::OnTraceBufferFull() {
  // OnTraceBufferFull may be called from any browser thread, either by the
  // local event trace system or from child processes via TraceMessageFilter.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceController::OnTraceBufferFull,
                   base::Unretained(this)));
    return;
  }

  // EndTracingAsync may return false if tracing is already in the process of
  // being ended. That is ok.
  EndTracingAsync(subscriber_);
}

void TraceController::OnTraceBufferPercentFullReply(float percent_full) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceController::OnTraceBufferPercentFullReply,
                   base::Unretained(this), percent_full));
    return;
  }

  if (pending_bpf_ack_count_ == 0)
    return;

  maximum_bpf_ = (maximum_bpf_ > percent_full)? maximum_bpf_ : percent_full;

  if (--pending_bpf_ack_count_ == 0) {
    // Trigger callback if one is set.
    if (subscriber_)
      subscriber_->OnTraceBufferPercentFullReply(maximum_bpf_);
  }

  if (pending_bpf_ack_count_ == 1) {
    // The last ack represents local trace, so we need to ack it now. Note that
    // this code only executes if there were child processes.
    float bpf = base::debug::TraceLog::GetInstance()->GetBufferPercentFull();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TraceController::OnTraceBufferPercentFullReply,
                   base::Unretained(this), bpf));
  }
}

