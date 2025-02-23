// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_script_fetcher_impl.h"

#include "base/compiler_specific.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/cert_status_flags.h"
#include "net/base/data_url.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_context.h"

// TODO(eroman):
//   - Support auth-prompts (http://crbug.com/77366)

namespace net {

namespace {

// The maximum size (in bytes) allowed for a PAC script. Responses exceeding
// this will fail with ERR_FILE_TOO_BIG.
const int kDefaultMaxResponseBytes = 1048576;  // 1 megabyte

// The maximum duration (in milliseconds) allowed for fetching the PAC script.
// Responses exceeding this will fail with ERR_TIMED_OUT.
const int kDefaultMaxDurationMs = 300000;  // 5 minutes

// Returns true if |mime_type| is one of the known PAC mime type.
bool IsPacMimeType(const std::string& mime_type) {
  static const char * const kSupportedPacMimeTypes[] = {
    "application/x-ns-proxy-autoconfig",
    "application/x-javascript-config",
  };
  for (size_t i = 0; i < arraysize(kSupportedPacMimeTypes); ++i) {
    if (LowerCaseEqualsASCII(mime_type, kSupportedPacMimeTypes[i]))
      return true;
  }
  return false;
}

// Converts |bytes| (which is encoded by |charset|) to UTF16, saving the resul
// to |*utf16|.
// If |charset| is empty, then we don't know what it was and guess.
void ConvertResponseToUTF16(const std::string& charset,
                            const std::string& bytes,
                            string16* utf16) {
  const char* codepage;

  if (charset.empty()) {
    // Assume ISO-8859-1 if no charset was specified.
    codepage = base::kCodepageLatin1;
  } else {
    // Otherwise trust the charset that was provided.
    codepage = charset.c_str();
  }

  // We will be generous in the conversion -- if any characters lie
  // outside of |charset| (i.e. invalid), then substitute them with
  // U+FFFD rather than failing.
  base::CodepageToUTF16(bytes, codepage,
                        base::OnStringConversionError::SUBSTITUTE,
                        utf16);
}

}  // namespace

ProxyScriptFetcherImpl::ProxyScriptFetcherImpl(
    URLRequestContext* url_request_context)
    : ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
      url_request_context_(url_request_context),
      buf_(new IOBuffer(kBufSize)),
      next_id_(0),
      cur_request_(NULL),
      cur_request_id_(0),
      callback_(NULL),
      result_code_(OK),
      result_text_(NULL),
      max_response_bytes_(kDefaultMaxResponseBytes),
      max_duration_(base::TimeDelta::FromMilliseconds(kDefaultMaxDurationMs)) {
  DCHECK(url_request_context);
}

ProxyScriptFetcherImpl::~ProxyScriptFetcherImpl() {
  // The URLRequest's destructor will cancel the outstanding request, and
  // ensure that the delegate (this) is not called again.
}

base::TimeDelta ProxyScriptFetcherImpl::SetTimeoutConstraint(
    base::TimeDelta timeout) {
  base::TimeDelta prev = max_duration_;
  max_duration_ = timeout;
  return prev;
}

size_t ProxyScriptFetcherImpl::SetSizeConstraint(size_t size_bytes) {
  size_t prev = max_response_bytes_;
  max_response_bytes_ = size_bytes;
  return prev;
}

void ProxyScriptFetcherImpl::OnResponseCompleted(URLRequest* request) {
  DCHECK_EQ(request, cur_request_.get());

  // Use |result_code_| as the request's error if we have already set it to
  // something specific.
  if (result_code_ == OK && !request->status().is_success())
    result_code_ = request->status().error();

  FetchCompleted();
}

int ProxyScriptFetcherImpl::Fetch(const GURL& url,
                                  string16* text,
                                  OldCompletionCallback* callback) {
  // It is invalid to call Fetch() while a request is already in progress.
  DCHECK(!cur_request_.get());

  DCHECK(callback);
  DCHECK(text);

  // Handle base-64 encoded data-urls that contain custom PAC scripts.
  if (url.SchemeIs("data")) {
    std::string mime_type;
    std::string charset;
    std::string data;
    if (!DataURL::Parse(url, &mime_type, &charset, &data))
      return ERR_FAILED;

    ConvertResponseToUTF16(charset, data, text);
    return OK;
  }

  cur_request_.reset(new URLRequest(url, this));
  cur_request_->set_context(url_request_context_);
  cur_request_->set_method("GET");

  // Make sure that the PAC script is downloaded using a direct connection,
  // to avoid circular dependencies (fetching is a part of proxy resolution).
  // Also disable the use of the disk cache. The cache is disabled so that if
  // the user switches networks we don't potentially use the cached response
  // from old network when we should in fact be re-fetching on the new network.
  // If the PAC script is hosted on an HTTPS server we bypass revocation
  // checking in order to avoid a circular dependency when attempting to fetch
  // the OCSP response or CRL. We could make the revocation check go direct but
  // the proxy might be the only way to the outside world.
  cur_request_->set_load_flags(LOAD_BYPASS_PROXY | LOAD_DISABLE_CACHE |
                               LOAD_DISABLE_CERT_REVOCATION_CHECKING);

  // Save the caller's info for notification on completion.
  callback_ = callback;
  result_text_ = text;

  bytes_read_so_far_.clear();

  // Post a task to timeout this request if it takes too long.
  cur_request_id_ = ++next_id_;
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      task_factory_.NewRunnableMethod(&ProxyScriptFetcherImpl::OnTimeout,
                                      cur_request_id_),
      static_cast<int>(max_duration_.InMilliseconds()));

  // Start the request.
  cur_request_->Start();
  return ERR_IO_PENDING;
}

void ProxyScriptFetcherImpl::Cancel() {
  // ResetCurRequestState will free the URLRequest, which will cause
  // cancellation.
  ResetCurRequestState();
}

URLRequestContext* ProxyScriptFetcherImpl::GetRequestContext() const {
  return url_request_context_;
}

void ProxyScriptFetcherImpl::OnAuthRequired(URLRequest* request,
                                            AuthChallengeInfo* auth_info) {
  DCHECK_EQ(request, cur_request_.get());
  // TODO(eroman): http://crbug.com/77366
  LOG(WARNING) << "Auth required to fetch PAC script, aborting.";
  result_code_ = ERR_NOT_IMPLEMENTED;
  request->CancelAuth();
}

void ProxyScriptFetcherImpl::OnSSLCertificateError(URLRequest* request,
                                                   const SSLInfo& ssl_info,
                                                   bool is_hsts_host) {
  DCHECK_EQ(request, cur_request_.get());
  // Revocation check failures are not fatal.
  if (IsCertStatusMinorError(ssl_info.cert_status)) {
    request->ContinueDespiteLastError();
    return;
  }
  LOG(WARNING) << "SSL certificate error when fetching PAC script, aborting.";
  // Certificate errors are in same space as net errors.
  result_code_ = MapCertStatusToNetError(ssl_info.cert_status);
  request->Cancel();
}

void ProxyScriptFetcherImpl::OnResponseStarted(URLRequest* request) {
  DCHECK_EQ(request, cur_request_.get());

  if (!request->status().is_success()) {
    OnResponseCompleted(request);
    return;
  }

  // Require HTTP responses to have a success status code.
  if (request->url().SchemeIs("http") || request->url().SchemeIs("https")) {
    // NOTE about status codes: We are like Firefox 3 in this respect.
    // {IE 7, Safari 3, Opera 9.5} do not care about the status code.
    if (request->GetResponseCode() != 200) {
      VLOG(1) << "Fetched PAC script had (bad) status line: "
              << request->response_headers()->GetStatusLine();
      result_code_ = ERR_PAC_STATUS_NOT_OK;
      request->Cancel();
      return;
    }

    // NOTE about mime types: We do not enforce mime types on PAC files.
    // This is for compatibility with {IE 7, Firefox 3, Opera 9.5}. We will
    // however log mismatches to help with debugging.
    std::string mime_type;
    cur_request_->GetMimeType(&mime_type);
    if (!IsPacMimeType(mime_type)) {
      VLOG(1) << "Fetched PAC script does not have a proper mime type: "
              << mime_type;
    }
  }

  ReadBody(request);
}

void ProxyScriptFetcherImpl::OnReadCompleted(URLRequest* request,
                                             int num_bytes) {
  DCHECK_EQ(request, cur_request_.get());
  if (ConsumeBytesRead(request, num_bytes)) {
    // Keep reading.
    ReadBody(request);
  }
}

void ProxyScriptFetcherImpl::ReadBody(URLRequest* request) {
  // Read as many bytes as are available synchronously.
  while (true) {
    int num_bytes;
    if (!request->Read(buf_, kBufSize, &num_bytes)) {
      // Check whether the read failed synchronously.
      if (!request->status().is_io_pending())
        OnResponseCompleted(request);
      return;
    }
    if (!ConsumeBytesRead(request, num_bytes))
      return;
  }
}

bool ProxyScriptFetcherImpl::ConsumeBytesRead(URLRequest* request,
                                              int num_bytes) {
  if (num_bytes <= 0) {
    // Error while reading, or EOF.
    OnResponseCompleted(request);
    return false;
  }

  // Enforce maximum size bound.
  if (num_bytes + bytes_read_so_far_.size() >
      static_cast<size_t>(max_response_bytes_)) {
    result_code_ = ERR_FILE_TOO_BIG;
    request->Cancel();
    return false;
  }

  bytes_read_so_far_.append(buf_->data(), num_bytes);
  return true;
}

void ProxyScriptFetcherImpl::FetchCompleted() {
  if (result_code_ == OK) {
    // The caller expects the response to be encoded as UTF16.
    std::string charset;
    cur_request_->GetCharset(&charset);
    ConvertResponseToUTF16(charset, bytes_read_so_far_, result_text_);
  } else {
    // On error, the caller expects empty string for bytes.
    result_text_->clear();
  }

  int result_code = result_code_;
  OldCompletionCallback* callback = callback_;

  // Hold a reference to the URLRequestContext to prevent re-entrancy from
  // ~URLRequestContext.
  scoped_refptr<const URLRequestContext> context(cur_request_->context());
  ResetCurRequestState();

  callback->Run(result_code);
}

void ProxyScriptFetcherImpl::ResetCurRequestState() {
  cur_request_.reset();
  cur_request_id_ = 0;
  callback_ = NULL;
  result_code_ = OK;
  result_text_ = NULL;
}

void ProxyScriptFetcherImpl::OnTimeout(int id) {
  // Timeout tasks may outlive the URLRequest they reference. Make sure it
  // is still applicable.
  if (cur_request_id_ != id)
    return;

  DCHECK(cur_request_.get());
  result_code_ = ERR_TIMED_OUT;
  cur_request_->Cancel();
}

}  // namespace net
