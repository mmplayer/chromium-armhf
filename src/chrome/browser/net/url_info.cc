// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/url_info.h"

#include <math.h>

#include <algorithm>
#include <string>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

namespace chrome_browser_net {

static bool detailed_logging_enabled = false;

// Use command line switch to enable detailed logging.
void EnablePredictorDetailedLog(bool enable) {
  detailed_logging_enabled = enable;
}

// static
int UrlInfo::sequence_counter = 1;

UrlInfo::UrlInfo()
    : state_(PENDING),
      old_prequeue_state_(state_),
      resolve_duration_(kNullDuration),
      queue_duration_(kNullDuration),
      sequence_number_(0),
      motivation_(NO_PREFETCH_MOTIVATION),
      was_linked_(false) {
}

UrlInfo::~UrlInfo() {}

bool UrlInfo::NeedsDnsUpdate() {
  switch (state_) {
    case PENDING:  // Just now created info.
      return true;

    case QUEUED:  // In queue.
    case ASSIGNED:  // It's being resolved.
    case ASSIGNED_BUT_MARKED:  // It's being resolved.
      return false;  // We're already working on it

    case NO_SUCH_NAME:  // Lookup failed.
    case FOUND:  // Lookup succeeded.
      return !IsStillCached();  // See if DNS cache expired.

    default:
      NOTREACHED();
      return false;
  }
}

const TimeDelta UrlInfo::kNullDuration(TimeDelta::FromMilliseconds(-1));

// Common low end TTL for sites is 5 minutes.  However, DNS servers give us
// the remaining time, not the original 5 minutes.  Hence it doesn't much matter
// whether we found something in the local cache, or an ISP cache, it will
// on average be 2.5 minutes before it expires.  We could try to model this with
// 180 seconds, but simpler is just to do the lookups all the time (wasting
// OS calls(?)), and let that OS cache decide what to do (with TTL in hand).
// We use a small time to help get some duplicate suppression, in case a page
// has a TON of copies of the same domain name, so that we don't thrash the OS
// to death.  Hopefully it is small enough that we're not hurting our cache hit
// rate (i.e., we could always ask the OS).
TimeDelta UrlInfo::cache_expiration_duration_(TimeDelta::FromSeconds(5));

const TimeDelta UrlInfo::kMaxNonNetworkDnsLookupDuration(
    TimeDelta::FromMilliseconds(15));

// Used by test ONLY.  The value is otherwise constant.
// static
void UrlInfo::set_cache_expiration(TimeDelta time) {
  cache_expiration_duration_ = time;
}

// static
TimeDelta UrlInfo::get_cache_expiration() {
  return cache_expiration_duration_;
}

void UrlInfo::SetQueuedState(ResolutionMotivation motivation) {
  DCHECK(PENDING == state_ || FOUND == state_ || NO_SUCH_NAME == state_);
  old_prequeue_state_ = state_;
  state_ = QUEUED;
  queue_duration_ = resolve_duration_ = kNullDuration;
  SetMotivation(motivation);
  GetDuration();  // Set time_
  DLogResultsStats("DNS Prefetch in queue");
}

void UrlInfo::SetAssignedState() {
  DCHECK(QUEUED == state_);
  state_ = ASSIGNED;
  queue_duration_ = GetDuration();
  DLogResultsStats("DNS Prefetch assigned");
  UMA_HISTOGRAM_TIMES("DNS.PrefetchQueue", queue_duration_);
}

void UrlInfo::RemoveFromQueue() {
  DCHECK(ASSIGNED == state_);
  state_ = old_prequeue_state_;
  DLogResultsStats("DNS Prefetch reset to prequeue");
  static const TimeDelta kBoundary = TimeDelta::FromSeconds(2);
  if (queue_duration_ > kBoundary) {
    UMA_HISTOGRAM_MEDIUM_TIMES("DNS.QueueRecycledDeltaOver2",
                               queue_duration_ - kBoundary);
    return;
  }
  // Make a custom linear histogram for the region from 0 to boundary.
  const size_t kBucketCount = 52;
  static base::Histogram* histogram(NULL);
  if (!histogram)
    histogram = base::LinearHistogram::FactoryTimeGet(
        "DNS.QueueRecycledUnder2", TimeDelta(), kBoundary, kBucketCount,
        base::Histogram::kUmaTargetedHistogramFlag);
  histogram->AddTime(queue_duration_);
}

void UrlInfo::SetPendingDeleteState() {
  DCHECK(ASSIGNED == state_  || ASSIGNED_BUT_MARKED == state_);
  state_ = ASSIGNED_BUT_MARKED;
}

void UrlInfo::SetFoundState() {
  DCHECK(ASSIGNED == state_);
  state_ = FOUND;
  resolve_duration_ = GetDuration();
  if (kMaxNonNetworkDnsLookupDuration <= resolve_duration_) {
    UMA_HISTOGRAM_CUSTOM_TIMES("DNS.PrefetchResolution", resolve_duration_,
        kMaxNonNetworkDnsLookupDuration, TimeDelta::FromMinutes(15), 100);
  }
  sequence_number_ = sequence_counter++;
  DLogResultsStats("DNS PrefetchFound");
}

void UrlInfo::SetNoSuchNameState() {
  DCHECK(ASSIGNED == state_);
  state_ = NO_SUCH_NAME;
  resolve_duration_ = GetDuration();
  if (kMaxNonNetworkDnsLookupDuration <= resolve_duration_) {
    DHISTOGRAM_TIMES("DNS.PrefetchNotFoundName", resolve_duration_);
  }
  sequence_number_ = sequence_counter++;
  DLogResultsStats("DNS PrefetchNotFound");
}

void UrlInfo::SetUrl(const GURL& url) {
  if (url_.is_empty())  // Not yet initialized.
    url_ = url;
  else
    DCHECK_EQ(url_, url);
}

// IsStillCached() guesses if the DNS cache still has IP data,
// or at least remembers results about "not finding host."
bool UrlInfo::IsStillCached() const {
  DCHECK(FOUND == state_ || NO_SUCH_NAME == state_);

  // Default MS OS does not cache failures. Hence we could return false almost
  // all the time for that case.  However, we'd never try again to prefetch
  // the value if we returned false that way.  Hence we'll just let the lookup
  // time out the same way as FOUND case.

  if (sequence_counter - sequence_number_ > kMaxGuaranteedDnsCacheSize)
    return false;

  TimeDelta time_since_resolution = TimeTicks::Now() - time_;

  return time_since_resolution < cache_expiration_duration_;
}

void UrlInfo::DLogResultsStats(const char* message) const {
  if (!detailed_logging_enabled)
    return;
  DVLOG(1) << "\t" << message << "\tq=" << queue_duration().InMilliseconds()
           << "ms,\tr=" << resolve_duration().InMilliseconds()
           << "ms,\tp=" << sequence_number_ << "\t" << url_.spec();
}

//------------------------------------------------------------------------------
// This last section supports HTML output, such as seen in about:dns.
//------------------------------------------------------------------------------

// Preclude any possibility of Java Script or markup in the text, by only
// allowing alphanumerics, '.', '-', ':', and whitespace.
static std::string RemoveJs(const std::string& text) {
  std::string output(text);
  size_t length = output.length();
  for (size_t i = 0; i < length; i++) {
    char next = output[i];
    if (isalnum(next) || isspace(next) || strchr(".-:/", next) != NULL)
      continue;
    output[i] = '?';
  }
  return output;
}

class MinMaxAverage {
 public:
  MinMaxAverage()
    : sum_(0), square_sum_(0), count_(0),
      minimum_(kint64max), maximum_(kint64min) {
  }

  // Return values for use in printf formatted as "%d"
  int sample(int64 value) {
    sum_ += value;
    square_sum_ += value * value;
    count_++;
    minimum_ = std::min(minimum_, value);
    maximum_ = std::max(maximum_, value);
    return static_cast<int>(value);
  }
  int minimum() const { return static_cast<int>(minimum_);    }
  int maximum() const { return static_cast<int>(maximum_);    }
  int average() const { return static_cast<int>(sum_/count_); }
  int     sum() const { return static_cast<int>(sum_);        }

  int standard_deviation() const {
    double average = static_cast<float>(sum_) / count_;
    double variance = static_cast<float>(square_sum_)/count_
                      - average * average;
    return static_cast<int>(floor(sqrt(variance) + .5));
  }

 private:
  int64 sum_;
  int64 square_sum_;
  int count_;
  int64 minimum_;
  int64 maximum_;

  // DISALLOW_COPY_AND_ASSIGN(MinMaxAverage);
};

static std::string HoursMinutesSeconds(int seconds) {
  std::string result;
  int print_seconds = seconds % 60;
  int minutes = seconds / 60;
  int print_minutes = minutes % 60;
  int print_hours = minutes/60;
  if (print_hours)
    base::StringAppendF(&result, "%.2d:",  print_hours);
  if (print_hours || print_minutes)
    base::StringAppendF(&result, "%2.2d:",  print_minutes);
  base::StringAppendF(&result, "%2.2d",  print_seconds);
  return result;
}

// static
void UrlInfo::GetHtmlTable(const UrlInfoTable& host_infos,
                           const char* description,
                           bool brief,
                           std::string* output) {
  if (0 == host_infos.size())
    return;
  output->append(description);
  base::StringAppendF(output, "%" PRIuS " %s", host_infos.size(),
                      (1 == host_infos.size()) ? "hostname" : "hostnames");

  if (brief) {
    output->append("<br><br>");
    return;
  }

  output->append("<br><table border=1>"
      "<tr><th>Host name</th>"
      "<th>How long ago<br>(HH:MM:SS)</th>"
      "<th>Motivation</th>"
      "</tr>");

  const char* row_format = "<tr align=right><td>%s</td>"  // Host name.
                           "<td>%s</td>"                  // How long ago.
                           "<td>%s</td>"                  // Motivation.
                           "</tr>";

  // Print bulk of table, and gather stats at same time.
  MinMaxAverage queue, when;
  TimeTicks current_time = TimeTicks::Now();
  for (UrlInfoTable::const_iterator it(host_infos.begin());
       it != host_infos.end(); it++) {
    queue.sample((it->queue_duration_.InMilliseconds()));
    base::StringAppendF(
        output,
        row_format,
        RemoveJs(it->url_.spec()).c_str(),
                 HoursMinutesSeconds(when.sample(
                     (current_time - it->time_).InSeconds())).c_str(),
        it->GetAsciiMotivation().c_str());
  }
  output->append("</table>");

#ifndef NDEBUG
  base::StringAppendF(
      output,
      "Prefetch Queue Durations: min=%d, avg=%d, max=%d<br><br>",
      queue.minimum(), queue.average(), queue.maximum());
#endif

  output->append("<br>");
}

void UrlInfo::SetMotivation(ResolutionMotivation motivation) {
  motivation_ = motivation;
  if (motivation < LINKED_MAX_MOTIVATED)
    was_linked_ = true;
}

std::string UrlInfo::GetAsciiMotivation() const {
  switch (motivation_) {
    case MOUSE_OVER_MOTIVATED:
      return "[mouse-over]";

    case PAGE_SCAN_MOTIVATED:
      return "[page scan]";

    case OMNIBOX_MOTIVATED:
      return "[omnibox]";

    case STARTUP_LIST_MOTIVATED:
      return "[startup list]";

    case NO_PREFETCH_MOTIVATION:
      return "n/a";

    case STATIC_REFERAL_MOTIVATED:
      return RemoveJs(referring_url_.spec()) + "*";

    case LEARNED_REFERAL_MOTIVATED:
      return RemoveJs(referring_url_.spec());

    default:
      return "";
  }
}

}  // namespace chrome_browser_net
