// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/backoff_entry.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/rand_util.h"

namespace net {

BackoffEntry::BackoffEntry(const BackoffEntry::Policy* const policy)
    : policy_(policy) {
  DCHECK(policy_);
  Reset();
}

BackoffEntry::~BackoffEntry() {
  // TODO(joi): Remove this once our clients (e.g. URLRequestThrottlerManager)
  // always destroy from the I/O thread.
  DetachFromThread();
}

void BackoffEntry::InformOfRequest(bool succeeded) {
  if (!succeeded) {
    ++failure_count_;
    exponential_backoff_release_time_ = CalculateReleaseTime();
  } else {
    // We slowly decay the number of times delayed instead of resetting it to 0
    // in order to stay stable if we receive successes interleaved between lots
    // of failures.
    //
    // TODO(joi): Revisit this; it might be most correct to go to zero
    // but have a way to go back to "old error count +1" if there is
    // another error soon after.
    if (failure_count_ > 0)
      --failure_count_;

    // The reason why we are not just cutting the release time to
    // ImplGetTimeNow() is on the one hand, it would unset a release
    // time set by SetCustomReleaseTime and on the other we would like
    // to push every request up to our "horizon" when dealing with
    // multiple in-flight requests. Ex: If we send three requests and
    // we receive 2 failures and 1 success. The success that follows
    // those failures will not reset the release time, further
    // requests will then need to wait the delay caused by the 2
    // failures.
    exponential_backoff_release_time_ = std::max(
        ImplGetTimeNow(), exponential_backoff_release_time_);
  }
}

bool BackoffEntry::ShouldRejectRequest() const {
  return exponential_backoff_release_time_ > ImplGetTimeNow();
}

base::TimeTicks BackoffEntry::GetReleaseTime() const {
  return exponential_backoff_release_time_;
}

void BackoffEntry::SetCustomReleaseTime(const base::TimeTicks& release_time) {
  exponential_backoff_release_time_ = release_time;
}

bool BackoffEntry::CanDiscard() const {
  if (policy_->entry_lifetime_ms == -1)
    return false;

  base::TimeTicks now = ImplGetTimeNow();

  int64 unused_since_ms =
      (now - exponential_backoff_release_time_).InMilliseconds();

  // Release time is further than now, we are managing it.
  if (unused_since_ms < 0)
    return false;

  if (failure_count_ > 0) {
    // Need to keep track of failures until maximum back-off period
    // has passed (since further failures can add to back-off).
    return unused_since_ms >= std::max(policy_->maximum_backoff_ms,
                                       policy_->entry_lifetime_ms);
  }

  // Otherwise, consider the entry is outdated if it hasn't been used for the
  // specified lifetime period.
  return unused_since_ms >= policy_->entry_lifetime_ms;
}

void BackoffEntry::Reset() {
  failure_count_ = 0;

  // We leave exponential_backoff_release_time_ unset, meaning 0. We could
  // initialize to ImplGetTimeNow() but because it's a virtual method it's
  // not safe to call in the constructor (and the constructor calls Reset()).
  // The effects are the same, i.e. ShouldRejectRequest() will return false
  // right after Reset().
  exponential_backoff_release_time_ = base::TimeTicks();
}

base::TimeTicks BackoffEntry::ImplGetTimeNow() const {
  return base::TimeTicks::Now();
}

base::TimeTicks BackoffEntry::CalculateReleaseTime() const {
  int effective_failure_count =
      std::max(0, failure_count_ - policy_->num_errors_to_ignore);
  if (effective_failure_count == 0) {
    // Never reduce previously set release horizon, e.g. due to Retry-After
    // header.
    return std::max(ImplGetTimeNow(), exponential_backoff_release_time_);
  }

  // The delay is calculated with this formula:
  // delay = initial_backoff * multiply_factor^(
  //     effective_failure_count - 1) * Uniform(1 - jitter_factor, 1]
  double delay = policy_->initial_backoff_ms;
  delay *= pow(policy_->multiply_factor, effective_failure_count - 1);
  delay -= base::RandDouble() * policy_->jitter_factor * delay;

  // Ensure that we do not exceed maximum delay.
  int64 delay_int = static_cast<int64>(delay + 0.5);
  delay_int = std::min(delay_int,
                       static_cast<int64>(policy_->maximum_backoff_ms));

  // Never reduce previously set release horizon, e.g. due to Retry-After
  // header.
  return std::max(
      ImplGetTimeNow() + base::TimeDelta::FromMilliseconds(delay_int),
      exponential_backoff_release_time_);
}

}  // namespace net
