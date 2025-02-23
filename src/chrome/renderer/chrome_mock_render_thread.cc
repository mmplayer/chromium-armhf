// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_mock_render_thread.h"

#include <fcntl.h>

#include "base/file_util.h"
#include "base/process_util.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/print_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_sync_message.h"
#include "printing/print_job_constants.h"
#include "printing/page_range.h"
#include "testing/gtest/include/gtest/gtest.h"

ChromeMockRenderThread::ChromeMockRenderThread()
    : printer_(new MockPrinter),
      print_dialog_user_response_(true),
      print_preview_cancel_page_number_(-1),
      print_preview_pages_remaining_(0) {
}

ChromeMockRenderThread::~ChromeMockRenderThread() {
}

bool ChromeMockRenderThread::OnMessageReceived(const IPC::Message& msg) {
  if (content::MockRenderThread::OnMessageReceived(msg))
    return true;

  // Some messages we do special handling.
  bool handled = true;
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(ChromeMockRenderThread, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OpenChannelToExtension,
                        OnMsgOpenChannelToExtension)
    IPC_MESSAGE_HANDLER(PrintHostMsg_GetDefaultPrintSettings,
                        OnGetDefaultPrintSettings)
    IPC_MESSAGE_HANDLER(PrintHostMsg_ScriptedPrint, OnScriptedPrint)
    IPC_MESSAGE_HANDLER(PrintHostMsg_UpdatePrintSettings, OnUpdatePrintSettings)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidGetPrintedPagesCount,
                        OnDidGetPrintedPagesCount)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidPrintPage, OnDidPrintPage)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidGetPreviewPageCount,
                        OnDidGetPreviewPageCount)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidPreviewPage, OnDidPreviewPage)
    IPC_MESSAGE_HANDLER(PrintHostMsg_CheckForCancel, OnCheckForCancel)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DuplicateSection, OnDuplicateSection)
#endif
#if defined(OS_CHROMEOS)
    IPC_MESSAGE_HANDLER(PrintHostMsg_AllocateTempFileForPrinting,
                        OnAllocateTempFileForPrinting)
    IPC_MESSAGE_HANDLER(PrintHostMsg_TempFileForPrintingWritten,
                        OnTempFileForPrintingWritten)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void ChromeMockRenderThread::OnMsgOpenChannelToExtension(
    int routing_id, const std::string& source_extension_id,
    const std::string& target_extension_id,
    const std::string& channel_name, int* port_id) {
  *port_id = 0;
}

#if defined(OS_CHROMEOS)
void ChromeMockRenderThread::OnAllocateTempFileForPrinting(
    base::FileDescriptor* renderer_fd,
    int* browser_fd) {
  renderer_fd->fd = *browser_fd = -1;
  renderer_fd->auto_close = false;

  FilePath path;
  if (file_util::CreateTemporaryFile(&path)) {
    int fd = open(path.value().c_str(), O_WRONLY);
    DCHECK_GE(fd, 0);
    renderer_fd->fd = *browser_fd = fd;
  }
}

void ChromeMockRenderThread::OnTempFileForPrintingWritten(int browser_fd) {
  close(browser_fd);
}
#endif  // defined(OS_CHROMEOS)

void ChromeMockRenderThread::OnGetDefaultPrintSettings(
    PrintMsg_Print_Params* params) {
  if (printer_.get())
    printer_->GetDefaultPrintSettings(params);
}

void ChromeMockRenderThread::OnScriptedPrint(
    const PrintHostMsg_ScriptedPrint_Params& params,
    PrintMsg_PrintPages_Params* settings) {
  if (print_dialog_user_response_ && printer_.get()) {
    printer_->ScriptedPrint(params.cookie,
                            params.expected_pages_count,
                            params.has_selection,
                            settings);
  }
}

void ChromeMockRenderThread::OnDidGetPrintedPagesCount(
    int cookie, int number_pages) {
  if (printer_.get())
    printer_->SetPrintedPagesCount(cookie, number_pages);
}

void ChromeMockRenderThread::OnDidPrintPage(
    const PrintHostMsg_DidPrintPage_Params& params) {
  if (printer_.get())
    printer_->PrintPage(params);
}

void ChromeMockRenderThread::OnDidGetPreviewPageCount(
    const PrintHostMsg_DidGetPreviewPageCount_Params& params) {
  print_preview_pages_remaining_ = params.page_count;
}

void ChromeMockRenderThread::OnDidPreviewPage(
    const PrintHostMsg_DidPreviewPage_Params& params) {
  DCHECK(params.page_number >= printing::FIRST_PAGE_INDEX);
  print_preview_pages_remaining_--;
}

void ChromeMockRenderThread::OnCheckForCancel(
    const std::string& preview_ui_addr,
    int preview_request_id,
    bool* cancel) {
  *cancel =
      (print_preview_pages_remaining_ == print_preview_cancel_page_number_);
}

void ChromeMockRenderThread::OnUpdatePrintSettings(
    int document_cookie,
    const base::DictionaryValue& job_settings,
    PrintMsg_PrintPages_Params* params) {
  // Check and make sure the required settings are all there.
  // We don't actually care about the values.
  std::string dummy_string;
  if (!job_settings.GetBoolean(printing::kSettingLandscape, NULL) ||
      !job_settings.GetBoolean(printing::kSettingCollate, NULL) ||
      !job_settings.GetInteger(printing::kSettingColor, NULL) ||
      !job_settings.GetBoolean(printing::kSettingPrintToPDF, NULL) ||
      !job_settings.GetBoolean(printing::kIsFirstRequest, NULL) ||
      !job_settings.GetString(printing::kSettingDeviceName, &dummy_string) ||
      !job_settings.GetInteger(printing::kSettingDuplexMode, NULL) ||
      !job_settings.GetInteger(printing::kSettingCopies, NULL) ||
      !job_settings.GetString(printing::kPreviewUIAddr, &dummy_string) ||
      !job_settings.GetInteger(printing::kPreviewRequestID, NULL)) {
    return;
  }

  // Just return the default settings.
  if (printer_.get()) {
    ListValue* page_range_array;
    printing::PageRanges new_ranges;
    if (job_settings.GetList(printing::kSettingPageRange, &page_range_array)) {
      for (size_t index = 0; index < page_range_array->GetSize(); ++index) {
        base::DictionaryValue* dict;
        if (!page_range_array->GetDictionary(index, &dict))
          continue;
        printing::PageRange range;
        if (!dict->GetInteger(printing::kSettingPageRangeFrom, &range.from) ||
            !dict->GetInteger(printing::kSettingPageRangeTo, &range.to)) {
          continue;
        }
        // Page numbers are 1-based in the dictionary.
        // Page numbers are 0-based for the printing context.
        range.from--;
        range.to--;
        new_ranges.push_back(range);
      }
    }
    std::vector<int> pages(printing::PageRange::GetPages(new_ranges));
    printer_->UpdateSettings(document_cookie, params, pages);
  }
}

void ChromeMockRenderThread::set_print_dialog_user_response(bool response) {
  print_dialog_user_response_ = response;
}

void ChromeMockRenderThread::set_print_preview_cancel_page_number(int page) {
  print_preview_cancel_page_number_ = page;
}

int ChromeMockRenderThread::print_preview_pages_remaining() {
  return print_preview_pages_remaining_;
}
