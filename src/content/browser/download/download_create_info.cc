// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_create_info.h"

#include <string>

#include "base/format_macros.h"
#include "base/stringprintf.h"

DownloadCreateInfo::DownloadCreateInfo(const FilePath& path,
                                       const GURL& url,
                                       const base::Time& start_time,
                                       int64 received_bytes,
                                       int64 total_bytes,
                                       int32 state,
                                       const DownloadId& download_id,
                                       bool has_user_gesture,
                                       content::PageTransition transition_type)
    : path(path),
      url_chain(1, url),
      path_uniquifier(0),
      start_time(start_time),
      received_bytes(received_bytes),
      total_bytes(total_bytes),
      state(state),
      download_id(download_id),
      has_user_gesture(has_user_gesture),
      transition_type(transition_type),
      db_handle(0),
      prompt_user_for_save_location(false) {
}

DownloadCreateInfo::DownloadCreateInfo()
    : path_uniquifier(0),
      received_bytes(0),
      total_bytes(0),
      state(-1),
      download_id(DownloadId::Invalid()),
      has_user_gesture(false),
      transition_type(content::PAGE_TRANSITION_LINK),
      db_handle(0),
      prompt_user_for_save_location(false) {
}

DownloadCreateInfo::~DownloadCreateInfo() {
}

std::string DownloadCreateInfo::DebugString() const {
  return base::StringPrintf("{"
                            " download_id = %s"
                            " url = \"%s\""
                            " path = \"%" PRFilePath "\""
                            " received_bytes = %" PRId64
                            " total_bytes = %" PRId64
                            " request_handle = %s"
                            " prompt_user_for_save_location = %c"
                            " }",
                            download_id.DebugString().c_str(),
                            url().spec().c_str(),
                            path.value().c_str(),
                            received_bytes,
                            total_bytes,
                            request_handle.DebugString().c_str(),
                            prompt_user_for_save_location ? 'T' : 'F');
}

const GURL& DownloadCreateInfo::url() const {
  return url_chain.empty() ? GURL::EmptyGURL() : url_chain.back();
}
