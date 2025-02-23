// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/job_status_updater.h"

#include "base/json/json_reader.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/net/http_return.h"
#include "chrome/service/cloud_print/cloud_print_consts.h"
#include "chrome/service/cloud_print/cloud_print_helpers.h"
#include "googleurl/src/gurl.h"

JobStatusUpdater::JobStatusUpdater(const std::string& printer_name,
                           const std::string& job_id,
                           cloud_print::PlatformJobId& local_job_id,
                           const GURL& cloud_print_server_url,
                           cloud_print::PrintSystem* print_system,
                           Delegate* delegate)
    : printer_name_(printer_name), job_id_(job_id),
      local_job_id_(local_job_id),
      cloud_print_server_url_(cloud_print_server_url),
      print_system_(print_system), delegate_(delegate), stopped_(false) {
  DCHECK(delegate_);
}

JobStatusUpdater::~JobStatusUpdater() {}

// Start checking the status of the local print job.
void JobStatusUpdater::UpdateStatus() {
  // It does not matter if we had already sent out an update and are waiting for
  // a response. This is a new update and we will simply cancel the old request
  // and send a new one.
  if (!stopped_) {
    bool need_update = false;
    // If the job has already been completed, we just need to update the server
    // with that status. The *only* reason we would come back here in that case
    // is if our last server update attempt failed.
    if (last_job_details_.status == cloud_print::PRINT_JOB_STATUS_COMPLETED) {
      need_update = true;
    } else {
      cloud_print::PrintJobDetails details;
      if (print_system_->GetJobDetails(printer_name_, local_job_id_,
              &details)) {
        if (details != last_job_details_) {
          last_job_details_ = details;
          need_update = true;
        }
      } else {
        // If GetJobDetails failed, the most likely case is that the job no
        // longer exists in the OS queue. We are going to assume it is done in
        // this case.
        last_job_details_.Clear();
        last_job_details_.status = cloud_print::PRINT_JOB_STATUS_COMPLETED;
        need_update = true;
      }
    }
    if (need_update) {
      request_ = new CloudPrintURLFetcher;
      request_->StartGetRequest(
          CloudPrintHelpers::GetUrlForJobStatusUpdate(
              cloud_print_server_url_, job_id_, last_job_details_),
          this,
          kCloudPrintAPIMaxRetryCount,
          std::string());
    }
  }
}

void JobStatusUpdater::Stop() {
  request_ = NULL;
  DCHECK(delegate_);
  stopped_ = true;
  delegate_->OnJobCompleted(this);
}

// CloudPrintURLFetcher::Delegate implementation.
CloudPrintURLFetcher::ResponseAction JobStatusUpdater::HandleJSONData(
      const URLFetcher* source,
      const GURL& url,
      DictionaryValue* json_data,
      bool succeeded) {
  if (last_job_details_.status == cloud_print::PRINT_JOB_STATUS_COMPLETED) {
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &JobStatusUpdater::Stop));
  }
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

void JobStatusUpdater::OnRequestAuthError() {
  if (delegate_)
    delegate_->OnAuthError();
}
