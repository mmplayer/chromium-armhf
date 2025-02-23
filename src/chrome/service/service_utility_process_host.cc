// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_utility_process_host.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_temp_dir.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_utility_messages.h"
#include "ipc/ipc_switches.h"
#include "printing/page_range.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/rect.h"

#if defined(OS_WIN)
#include "base/memory/scoped_ptr.h"
#include "base/win/scoped_handle.h"
#include "content/common/child_process_messages.h"
#include "printing/emf_win.h"
#endif

ServiceUtilityProcessHost::ServiceUtilityProcessHost(
    Client* client, base::MessageLoopProxy* client_message_loop_proxy)
        : ServiceChildProcessHost(ChildProcessInfo::UTILITY_PROCESS),
          client_(client),
          client_message_loop_proxy_(client_message_loop_proxy),
          waiting_for_reply_(false) {
  process_id_ = ChildProcessInfo::GenerateChildProcessUniqueId();
}

ServiceUtilityProcessHost::~ServiceUtilityProcessHost() {
}

bool ServiceUtilityProcessHost::StartRenderPDFPagesToMetafile(
    const FilePath& pdf_path,
    const printing::PdfRenderSettings& render_settings,
    const std::vector<printing::PageRange>& page_ranges) {
#if !defined(OS_WIN)
  // This is only implemented on Windows (because currently it is only needed
  // on Windows). Will add implementations on other platforms when needed.
  NOTIMPLEMENTED();
  return false;
#else  // !defined(OS_WIN)
  scratch_metafile_dir_.reset(new ScopedTempDir);
  if (!scratch_metafile_dir_->CreateUniqueTempDir())
    return false;
  if (!file_util::CreateTemporaryFileInDir(scratch_metafile_dir_->path(),
                                           &metafile_path_)) {
    return false;
  }

  if (!StartProcess(false, scratch_metafile_dir_->path()))
    return false;

  base::win::ScopedHandle pdf_file(
      ::CreateFile(pdf_path.value().c_str(),
                   GENERIC_READ,
                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                   NULL,
                   OPEN_EXISTING,
                   FILE_ATTRIBUTE_NORMAL,
                   NULL));
  if (pdf_file == INVALID_HANDLE_VALUE)
    return false;
  HANDLE pdf_file_in_utility_process = NULL;
  ::DuplicateHandle(::GetCurrentProcess(), pdf_file, handle(),
                    &pdf_file_in_utility_process, 0, false,
                    DUPLICATE_SAME_ACCESS);
  if (!pdf_file_in_utility_process)
    return false;
  waiting_for_reply_ = true;
  return Send(
      new ChromeUtilityMsg_RenderPDFPagesToMetafile(
          pdf_file_in_utility_process,
          metafile_path_,
          render_settings,
          page_ranges));
#endif  // !defined(OS_WIN)
}

bool ServiceUtilityProcessHost::StartGetPrinterCapsAndDefaults(
    const std::string& printer_name) {
  FilePath exposed_path;
  if (!StartProcess(true, exposed_path))
    return false;
  waiting_for_reply_ = true;
  return Send(new ChromeUtilityMsg_GetPrinterCapsAndDefaults(printer_name));
}

bool ServiceUtilityProcessHost::StartProcess(bool no_sandbox,
                                             const FilePath& exposed_dir) {
  // Name must be set or metrics_service will crash in any test which
  // launches a UtilityProcessHost.
  set_name(ASCIIToUTF16("utility process"));

  if (!CreateChannel())
    return false;

  FilePath exe_path = GetUtilityProcessCmd();
  if (exe_path.empty()) {
    NOTREACHED() << "Unable to get utility process binary name.";
    return false;
  }

  CommandLine cmd_line(exe_path);
  cmd_line.AppendSwitchASCII(switches::kProcessType, switches::kUtilityProcess);
  cmd_line.AppendSwitchASCII(switches::kProcessChannelID, channel_id());
  cmd_line.AppendSwitch(switches::kLang);

  return Launch(&cmd_line, no_sandbox, exposed_dir);
}

FilePath ServiceUtilityProcessHost::GetUtilityProcessCmd() {
#if defined(OS_LINUX)
  int flags = CHILD_ALLOW_SELF;
#else
  int flags = CHILD_NORMAL;
#endif
  return GetChildPath(flags);
}

bool ServiceUtilityProcessHost::CanShutdown() {
  return true;
}

void ServiceUtilityProcessHost::OnChildDied() {
  if (waiting_for_reply_) {
    // If we are yet to receive a reply then notify the client that the
    // child died.
    client_message_loop_proxy_->PostTask(
        FROM_HERE,
        NewRunnableMethod(client_.get(), &Client::OnChildDied));
  }
  // The base class implementation will delete |this|.
  ServiceChildProcessHost::OnChildDied();
}

bool ServiceUtilityProcessHost::OnMessageReceived(const IPC::Message& message) {
  bool msg_is_ok = false;
  IPC_BEGIN_MESSAGE_MAP_EX(ServiceUtilityProcessHost, message, msg_is_ok)
#if defined(OS_WIN)  // This hack is Windows-specific.
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_PreCacheFont, OnPreCacheFont)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_ReleaseCachedFonts,
                        OnReleaseCachedFonts)
#endif
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_RenderPDFPagesToMetafile_Succeeded,
        OnRenderPDFPagesToMetafileSucceeded)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_RenderPDFPagesToMetafile_Failed,
                        OnRenderPDFPagesToMetafileFailed)
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_GetPrinterCapsAndDefaults_Succeeded,
        OnGetPrinterCapsAndDefaultsSucceeded)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_GetPrinterCapsAndDefaults_Failed,
                        OnGetPrinterCapsAndDefaultsFailed)
    IPC_MESSAGE_UNHANDLED(msg_is_ok__ = false)
  IPC_END_MESSAGE_MAP_EX()
  return true;
}

#if defined(OS_WIN)  // This hack is Windows-specific.
void ServiceUtilityProcessHost::OnPreCacheFont(const LOGFONT& font) {
  PreCacheFont(font, process_id_);
}

void ServiceUtilityProcessHost::OnReleaseCachedFonts() {
  ReleaseCachedFonts(process_id_);
}
#endif  // OS_WIN

void ServiceUtilityProcessHost::OnRenderPDFPagesToMetafileSucceeded(
    int highest_rendered_page_number) {
  DCHECK(waiting_for_reply_);
  waiting_for_reply_ = false;
  // If the metafile was successfully created, we need to take our hands off the
  // scratch metafile directory. The client will delete it when it is done with
  // metafile.
  scratch_metafile_dir_->Take();
  client_message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(client_.get(),
                        &Client::MetafileAvailable,
                        metafile_path_,
                        highest_rendered_page_number));
}

void ServiceUtilityProcessHost::OnRenderPDFPagesToMetafileFailed() {
  DCHECK(waiting_for_reply_);
  waiting_for_reply_ = false;
  client_message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(client_.get(),
                        &Client::OnRenderPDFPagesToMetafileFailed));
}

void ServiceUtilityProcessHost::OnGetPrinterCapsAndDefaultsSucceeded(
    const std::string& printer_name,
    const printing::PrinterCapsAndDefaults& caps_and_defaults) {
  DCHECK(waiting_for_reply_);
  waiting_for_reply_ = false;
  client_message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(client_.get(),
                        &Client::OnGetPrinterCapsAndDefaultsSucceeded,
                        printer_name,
                        caps_and_defaults));
}

void ServiceUtilityProcessHost::OnGetPrinterCapsAndDefaultsFailed(
    const std::string& printer_name) {
  DCHECK(waiting_for_reply_);
  waiting_for_reply_ = false;
  client_message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(client_.get(),
                        &Client::OnGetPrinterCapsAndDefaultsFailed,
                        printer_name));
}

void ServiceUtilityProcessHost::Client::MetafileAvailable(
    const FilePath& metafile_path,
    int highest_rendered_page_number) {
  // The metafile was created in a temp folder which needs to get deleted after
  // we have processed it.
  ScopedTempDir scratch_metafile_dir;
  if (!scratch_metafile_dir.Set(metafile_path.DirName()))
    LOG(WARNING) << "Unable to set scratch metafile directory";
#if defined(OS_WIN)
  // It's important that metafile is declared after scratch_metafile_dir so
  // that the metafile destructor closes the file before the ScopedTempDir
  // destructor tries to remove the directory.
  printing::Emf metafile;
  if (!metafile.InitFromFile(metafile_path)) {
    OnRenderPDFPagesToMetafileFailed();
  } else {
    OnRenderPDFPagesToMetafileSucceeded(metafile,
                                        highest_rendered_page_number);
  }
#endif  // defined(OS_WIN)
}

