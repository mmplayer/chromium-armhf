// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_VIEW_CACHE_HELPER_H_
#define NET_URL_REQUEST_VIEW_CACHE_HELPER_H_
#pragma once

#include <string>

#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_export.h"

namespace disk_cache {
class Backend;
class Entry;
}  // namespace disk_cache

namespace net {

class URLRequestContext;

class NET_EXPORT ViewCacheHelper {
 public:
  ViewCacheHelper();
  ~ViewCacheHelper();

  // Formats the cache information for |key| as HTML. Returns a net error code.
  // If this method returns ERR_IO_PENDING, |callback| will be notified when the
  // operation completes. |out| must remain valid until this operation completes
  // or the object is destroyed.
  int GetEntryInfoHTML(const std::string& key,
                       const URLRequestContext* context,
                       std::string* out,
                       OldCompletionCallback* callback);

  // Formats the cache contents as HTML. Returns a net error code.
  // If this method returns ERR_IO_PENDING, |callback| will be notified when the
  // operation completes. |out| must remain valid until this operation completes
  // or the object is destroyed. |url_prefix| will be prepended to each entry
  // key as a link to the entry.
  int GetContentsHTML(const URLRequestContext* context,
                      const std::string& url_prefix,
                      std::string* out,
                      OldCompletionCallback* callback);

  // Lower-level helper to produce a textual representation of binary data.
  // The results are appended to |result| and can be used in HTML pages
  // provided the dump is contained within <pre></pre> tags.
  static void HexDump(const char *buf, size_t buf_len, std::string* result);

 private:
  enum State {
    STATE_NONE,
    STATE_GET_BACKEND,
    STATE_GET_BACKEND_COMPLETE,
    STATE_OPEN_NEXT_ENTRY,
    STATE_OPEN_NEXT_ENTRY_COMPLETE,
    STATE_OPEN_ENTRY,
    STATE_OPEN_ENTRY_COMPLETE,
    STATE_READ_RESPONSE,
    STATE_READ_RESPONSE_COMPLETE,
    STATE_READ_DATA,
    STATE_READ_DATA_COMPLETE
  };

  // Implements GetEntryInfoHTML and GetContentsHTML.
  int GetInfoHTML(const std::string& key,
                  const URLRequestContext* context,
                  const std::string& url_prefix,
                  std::string* out,
                  OldCompletionCallback* callback);

  // This is a helper function used to trigger a completion callback. It may
  // only be called if callback_ is non-null.
  void DoCallback(int rv);

  // This will trigger the completion callback if appropriate.
  void HandleResult(int rv);

  // Runs the state transition loop.
  int DoLoop(int result);

  // Each of these methods corresponds to a State value. If there is an
  // argument, the value corresponds to the return of the previous state or
  // corresponding callback.
  int DoGetBackend();
  int DoGetBackendComplete(int result);
  int DoOpenNextEntry();
  int DoOpenNextEntryComplete(int result);
  int DoOpenEntry();
  int DoOpenEntryComplete(int result);
  int DoReadResponse();
  int DoReadResponseComplete(int result);
  int DoReadData();
  int DoReadDataComplete(int result);

  // Called to signal completion of asynchronous IO.
  void OnIOComplete(int result);

  scoped_refptr<const URLRequestContext> context_;
  disk_cache::Backend* disk_cache_;
  disk_cache::Entry* entry_;
  void* iter_;
  scoped_refptr<IOBuffer> buf_;
  int buf_len_;
  int index_;

  std::string key_;
  std::string url_prefix_;
  std::string* data_;
  OldCompletionCallback* callback_;

  State next_state_;

  OldCompletionCallbackImpl<ViewCacheHelper> cache_callback_;
  scoped_refptr<CancelableOldCompletionCallback<ViewCacheHelper> > entry_callback_;

  DISALLOW_COPY_AND_ASSIGN(ViewCacheHelper);
};

}  // namespace net.

#endif  // NET_URL_REQUEST_VIEW_CACHE_HELPER_H_
