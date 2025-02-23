// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SAVE_FILE_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_SAVE_FILE_RESOURCE_HANDLER_H_
#pragma once

#include <string>

#include "content/browser/renderer_host/resource_handler.h"
#include "googleurl/src/gurl.h"

class SaveFileManager;

// Forwards data to the save thread.
class SaveFileResourceHandler : public ResourceHandler {
 public:
  SaveFileResourceHandler(int render_process_host_id,
                          int render_view_id,
                          const GURL& url,
                          SaveFileManager* manager);

  // ResourceHandler Implementation:
  virtual bool OnUploadProgress(int request_id, uint64 position, uint64 size);

  // Saves the redirected URL to final_url_, we need to use the original
  // URL to match original request.
  virtual bool OnRequestRedirected(int request_id, const GURL& url,
                                   ResourceResponse* response, bool* defer);

  // Sends the download creation information to the download thread.
  virtual bool OnResponseStarted(int request_id, ResourceResponse* response);

  // Pass-through implementation.
  virtual bool OnWillStart(int request_id, const GURL& url, bool* defer);

  // Creates a new buffer, which will be handed to the download thread for file
  // writing and deletion.
  virtual bool OnWillRead(int request_id, net::IOBuffer** buf, int* buf_size,
                          int min_size);

  // Passes the buffer to the download file writer.
  virtual bool OnReadCompleted(int request_id, int* bytes_read);

  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info);

  virtual void OnRequestClosed();

  // If the content-length header is not present (or contains something other
  // than numbers), StringToInt64 returns 0, which indicates 'unknown size' and
  // is handled correctly by the SaveManager.
  void set_content_length(const std::string& content_length);

  void set_content_disposition(const std::string& content_disposition) {
    content_disposition_ = content_disposition;
  }

 private:
  virtual ~SaveFileResourceHandler();

  int save_id_;
  int render_process_id_;
  int render_view_id_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  std::string content_disposition_;
  GURL url_;
  GURL final_url_;
  int64 content_length_;
  SaveFileManager* save_manager_;

  static const int kReadBufSize = 32768;  // bytes

  DISALLOW_COPY_AND_ASSIGN(SaveFileResourceHandler);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_SAVE_FILE_RESOURCE_HANDLER_H_
