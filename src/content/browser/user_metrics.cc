// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/user_metrics.h"

#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "content/public/browser/notification_types.h"

void UserMetrics::RecordAction(const UserMetricsAction& action) {
  Record(action.str_);
}

void UserMetrics::RecordComputedAction(const std::string& action) {
  Record(action.c_str());
}

void UserMetrics::Record(const char *action) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableFunction(&UserMetrics::CallRecordOnUI, action));
    return;
  }

  NotificationService::current()->Notify(content::NOTIFICATION_USER_ACTION,
                                         NotificationService::AllSources(),
                                         Details<const char*>(&action));
}

void UserMetrics::CallRecordOnUI(const std::string& action) {
  Record(action.c_str());
}
