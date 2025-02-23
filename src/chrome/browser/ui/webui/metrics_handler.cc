// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/metrics_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/metrics/metric_event_duration_details.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/user_metrics.h"
#include "content/common/notification_service.h"

using base::ListValue;

MetricsHandler::MetricsHandler() {}
MetricsHandler::~MetricsHandler() {}

void MetricsHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(
      "metricsHandler:recordAction",
      base::Bind(&MetricsHandler::HandleRecordAction, base::Unretained(this)));
  web_ui_->RegisterMessageCallback(
      "metricsHandler:recordInHistogram",
      base::Bind(&MetricsHandler::HandleRecordInHistogram,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback(
      "metricsHandler:logEventTime",
      base::Bind(&MetricsHandler::HandleLogEventTime, base::Unretained(this)));
}

void MetricsHandler::HandleRecordAction(const ListValue* args) {
  std::string string_action = UTF16ToUTF8(ExtractStringValue(args));
  UserMetrics::RecordComputedAction(string_action);
}

void MetricsHandler::HandleRecordInHistogram(const ListValue* args) {
  std::string histogram_name;
  double value;
  double boundary_value;
  if (!args->GetString(0, &histogram_name) ||
      !args->GetDouble(1, &value) ||
      !args->GetDouble(2, &boundary_value)) {
    NOTREACHED();
    return;
  }

  int int_value = static_cast<int>(value);
  int int_boundary_value = static_cast<int>(boundary_value);
  if (int_boundary_value >= 4000 ||
      int_value > int_boundary_value ||
      int_value < 0) {
    NOTREACHED();
    return;
  }

  int bucket_count = int_boundary_value;
  while (bucket_count >= 100) {
    bucket_count /= 10;
  }

  // As |histogram_name| may change between calls, the UMA_HISTOGRAM_ENUMERATION
  // macro cannot be used here.
  base::Histogram* counter =
      base::LinearHistogram::FactoryGet(
          histogram_name, 1, int_boundary_value, bucket_count + 1,
          base::Histogram::kUmaTargetedHistogramFlag);
  counter->Add(int_value);
}

void MetricsHandler::HandleLogEventTime(const ListValue* args) {
  std::string event_name = UTF16ToUTF8(ExtractStringValue(args));
  TabContents* tab = web_ui_->tab_contents();

  // Not all new tab pages get timed. In those cases, we don't have a
  // new_tab_start_time_.
  if (tab->new_tab_start_time().is_null())
    return;

  base::TimeDelta duration = base::TimeTicks::Now() - tab->new_tab_start_time();
  MetricEventDurationDetails details(event_name,
      static_cast<int>(duration.InMilliseconds()));

  if (event_name == "Tab.NewTabScriptStart") {
    UMA_HISTOGRAM_TIMES("Tab.NewTabScriptStart", duration);
  } else if (event_name == "Tab.NewTabDOMContentLoaded") {
    UMA_HISTOGRAM_TIMES("Tab.NewTabDOMContentLoaded", duration);
  } else if (event_name == "Tab.NewTabOnload") {
    UMA_HISTOGRAM_TIMES("Tab.NewTabOnload", duration);
    // The new tab page has finished loading; reset it.
    tab->set_new_tab_start_time(base::TimeTicks());
  } else {
    NOTREACHED();
  }
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_METRIC_EVENT_DURATION,
      Source<TabContents>(tab),
      Details<MetricEventDurationDetails>(&details));
}
