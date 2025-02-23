// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_install_helper.h"

#include <string>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/common/chrome_utility_messages.h"
#include "content/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace {

const char kImageDecodeError[] = "Image decode failed";

}  // namespace

WebstoreInstallHelper::WebstoreInstallHelper(
    Delegate* delegate,
    const std::string& id,
    const std::string& manifest,
    const std::string& icon_data,
    const GURL& icon_url,
    net::URLRequestContextGetter* context_getter)
    : delegate_(delegate),
      id_(id),
      manifest_(manifest),
      icon_base64_data_(icon_data),
      icon_url_(icon_url),
      context_getter_(context_getter),
      utility_host_(NULL),
      icon_decode_complete_(false),
      manifest_parse_complete_(false),
      parse_error_(Delegate::UNKNOWN_ERROR) {}

WebstoreInstallHelper::~WebstoreInstallHelper() {}

void WebstoreInstallHelper::Start() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(icon_base64_data_.empty() || icon_url_.is_empty());

  if (icon_base64_data_.empty() && icon_url_.is_empty())
    icon_decode_complete_ = true;

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&WebstoreInstallHelper::StartWorkOnIOThread, this));

  if (!icon_url_.is_empty()) {
    CHECK(context_getter_);
    url_fetcher_.reset(new URLFetcher(icon_url_, URLFetcher::GET, this));
    url_fetcher_->set_request_context(context_getter_);

    url_fetcher_->Start();
    // We'll get called back in OnURLFetchComplete.
  }
}

void WebstoreInstallHelper::StartWorkOnIOThread() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  utility_host_ = new UtilityProcessHost(this, BrowserThread::IO);
  utility_host_->StartBatchMode();

  if (!icon_base64_data_.empty())
    utility_host_->Send(
        new ChromeUtilityMsg_DecodeImageBase64(icon_base64_data_));

  utility_host_->Send(new ChromeUtilityMsg_ParseJSON(manifest_));
}

void WebstoreInstallHelper::OnURLFetchComplete(const URLFetcher* source) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(source == url_fetcher_.get());
  if (source->status().status() != net::URLRequestStatus::SUCCESS ||
      source->response_code() != 200) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&WebstoreInstallHelper::OnDecodeImageFailed, this));
  } else {
    std::string response_data;
    source->GetResponseAsString(&response_data);
    fetched_icon_data_.insert(fetched_icon_data_.begin(),
                              response_data.begin(),
                              response_data.end());
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&WebstoreInstallHelper::StartFetchedImageDecode, this));
  }
  url_fetcher_.reset();
}

void WebstoreInstallHelper::StartFetchedImageDecode() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  CHECK(utility_host_);
  utility_host_->Send(new ChromeUtilityMsg_DecodeImage(fetched_icon_data_));
}


bool WebstoreInstallHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebstoreInstallHelper, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_DecodeImage_Succeeded,
                        OnDecodeImageSucceeded)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_DecodeImage_Failed,
                        OnDecodeImageFailed)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParseJSON_Succeeded,
                        OnJSONParseSucceeded)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParseJSON_Failed,
                        OnJSONParseFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}


void WebstoreInstallHelper::OnDecodeImageSucceeded(
    const SkBitmap& decoded_image) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  icon_ = decoded_image;
  icon_decode_complete_ = true;
  ReportResultsIfComplete();
}

void WebstoreInstallHelper::OnDecodeImageFailed() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  icon_decode_complete_ = true;
  error_ = kImageDecodeError;
  parse_error_ = Delegate::ICON_ERROR;
  ReportResultsIfComplete();
}

void WebstoreInstallHelper::OnJSONParseSucceeded(const ListValue& wrapper) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  manifest_parse_complete_ = true;
  Value* value = NULL;
  CHECK(wrapper.Get(0, &value));
  if (value->IsType(Value::TYPE_DICTIONARY)) {
    parsed_manifest_.reset(
        static_cast<DictionaryValue*>(value)->DeepCopy());
  } else {
    parse_error_ = Delegate::MANIFEST_ERROR;
  }
  ReportResultsIfComplete();
}

void WebstoreInstallHelper::OnJSONParseFailed(
    const std::string& error_message) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  manifest_parse_complete_ = true;
  error_ = error_message;
  parse_error_ = Delegate::MANIFEST_ERROR;
  ReportResultsIfComplete();
}

void WebstoreInstallHelper::ReportResultsIfComplete() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!icon_decode_complete_ || !manifest_parse_complete_)
    return;

  // The utility_host_ will take care of deleting itself after this call.
  utility_host_->EndBatchMode();
  utility_host_ = NULL;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WebstoreInstallHelper::ReportResultFromUIThread, this));
}

void WebstoreInstallHelper::ReportResultFromUIThread() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (error_.empty() && parsed_manifest_.get())
    delegate_->OnWebstoreParseSuccess(id_, icon_, parsed_manifest_.release());
  else
    delegate_->OnWebstoreParseFailure(id_, parse_error_, error_);
}
