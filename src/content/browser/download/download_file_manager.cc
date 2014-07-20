// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_file_manager.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "content/browser/browser_thread.h"
#include "content/browser/download/download_file.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/download/download_manager_delegate.h"
#include "content/browser/download/interrupt_reasons.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"

namespace {

// Throttle updates to the UI thread so that a fast moving download doesn't
// cause it to become unresponsive (in milliseconds).
const int kUpdatePeriodMs = 500;

}  // namespace

DownloadFileManager::DownloadFileManager(ResourceDispatcherHost* rdh)
    : resource_dispatcher_host_(rdh) {
}

DownloadFileManager::~DownloadFileManager() {
  DCHECK(downloads_.empty());
}

void DownloadFileManager::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &DownloadFileManager::OnShutdown));
}

void DownloadFileManager::OnShutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  StopUpdateTimer();
  STLDeleteValues(&downloads_);
}

void DownloadFileManager::CreateDownloadFile(DownloadCreateInfo* info,
                                             DownloadManager* download_manager,
                                             bool get_hash) {
  DCHECK(info);
  VLOG(20) << __FUNCTION__ << "()" << " info = " << info->DebugString();
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Life of |info| ends here. No more references to it after this method.
  scoped_ptr<DownloadCreateInfo> infop(info);

  scoped_ptr<DownloadFile>
      download_file(new DownloadFile(info, download_manager));
  if (net::OK != download_file->Initialize(get_hash)) {
    info->request_handle.CancelRequest();
    return;
  }

  DCHECK(GetDownloadFile(info->download_id) == NULL);
  downloads_[info->download_id] = download_file.release();

  // The file is now ready, we can un-pause the request and start saving data.
  info->request_handle.ResumeRequest();

  StartUpdateTimer();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(download_manager,
                        &DownloadManager::StartDownload,
                        info->download_id.local()));
}

DownloadFile* DownloadFileManager::GetDownloadFile(DownloadId global_id) {
  DownloadFileMap::iterator it = downloads_.find(global_id);
  return it == downloads_.end() ? NULL : it->second;
}

void DownloadFileManager::StartUpdateTimer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!update_timer_.IsRunning()) {
    update_timer_.Start(FROM_HERE,
                        base::TimeDelta::FromMilliseconds(kUpdatePeriodMs),
                        this, &DownloadFileManager::UpdateInProgressDownloads);
  }
}

void DownloadFileManager::StopUpdateTimer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  update_timer_.Stop();
}

void DownloadFileManager::UpdateInProgressDownloads() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  for (DownloadFileMap::iterator i = downloads_.begin();
       i != downloads_.end(); ++i) {
    DownloadId global_id = i->first;
    DownloadFile* download_file = i->second;
    DownloadManager* manager = download_file->GetDownloadManager();
    if (manager) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(manager, &DownloadManager::UpdateDownload,
                            global_id.local(), download_file->bytes_so_far()));
    }
  }
}

void DownloadFileManager::StartDownload(DownloadCreateInfo* info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(info);

  DownloadManager* manager = info->request_handle.GetDownloadManager();
  if (!manager) {
    info->request_handle.CancelRequest();
    delete info;
    return;
  }

  // TODO(phajdan.jr): fix the duplication of path info below.
  info->path = info->save_info.file_path;

  manager->CreateDownloadItem(info);
  bool hash_needed = manager->delegate()->GenerateFileHash();

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &DownloadFileManager::CreateDownloadFile,
                        info, make_scoped_refptr(manager), hash_needed));
}

// We don't forward an update to the UI thread here, since we want to throttle
// the UI update rate via a periodic timer. If the user has cancelled the
// download (in the UI thread), we may receive a few more updates before the IO
// thread gets the cancel message: we just delete the data since the
// DownloadFile has been deleted.
void DownloadFileManager::UpdateDownload(
    DownloadId global_id, DownloadBuffer* buffer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::vector<DownloadBuffer::Contents> contents;
  {
    base::AutoLock auto_lock(buffer->lock);
    contents.swap(buffer->contents);
  }

  DownloadFile* download_file = GetDownloadFile(global_id);
  bool had_error = false;
  for (size_t i = 0; i < contents.size(); ++i) {
    net::IOBuffer* data = contents[i].first;
    const int data_len = contents[i].second;
    if (!had_error && download_file) {
      net::Error write_result =
          download_file->AppendDataToFile(data->data(), data_len);
      if (write_result != net::OK) {
        // Write failed: interrupt the download.
        DownloadManager* download_manager = download_file->GetDownloadManager();
        had_error = true;

        int64 bytes_downloaded = download_file->bytes_so_far();
        // Calling this here in case we get more data, to avoid
        // processing data after an error.  That could lead to
        // files that are corrupted if the later processing succeeded.
        CancelDownload(global_id);
        download_file = NULL;  // Was deleted in |CancelDownload|.

        if (download_manager) {
          BrowserThread::PostTask(
              BrowserThread::UI,
              FROM_HERE,
              NewRunnableMethod(
                  download_manager,
                  &DownloadManager::OnDownloadInterrupted,
                  global_id.local(),
                  bytes_downloaded,
                  ConvertNetErrorToInterruptReason(
                      write_result,
                      DOWNLOAD_INTERRUPT_FROM_DISK)));
        }
      }
    }
    data->Release();
  }
}

void DownloadFileManager::OnResponseCompleted(
    DownloadId global_id,
    DownloadBuffer* buffer,
    net::Error net_error,
    const std::string& security_info) {
  VLOG(20) << __FUNCTION__ << "()" << " id = " << global_id
           << " net_error = " << net_error
           << " security_info = \"" << security_info << "\"";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  delete buffer;
  DownloadFile* download_file = GetDownloadFile(global_id);
  if (!download_file)
    return;

  download_file->Finish();

  DownloadManager* download_manager = download_file->GetDownloadManager();
  if (!download_manager) {
    CancelDownload(global_id);
    return;
  }

  std::string hash;
  if (!download_file->GetSha256Hash(&hash))
    hash.clear();

  // ERR_CONNECTION_CLOSED is allowed since a number of servers in the wild
  // advertise a larger Content-Length than the amount of bytes in the message
  // body, and then close the connection. Other browsers - IE8, Firefox 4.0.1,
  // and Safari 5.0.4 - treat the download as complete in this case, so we
  // follow their lead.
  if (net_error == net::OK || net_error == net::ERR_CONNECTION_CLOSED) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        NewRunnableMethod(
            download_manager,
            &DownloadManager::OnResponseCompleted,
            global_id.local(),
            download_file->bytes_so_far(),
            hash));
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        NewRunnableMethod(
            download_manager,
            &DownloadManager::OnDownloadInterrupted,
            global_id.local(),
            download_file->bytes_so_far(),
            ConvertNetErrorToInterruptReason(
                net_error,
                DOWNLOAD_INTERRUPT_FROM_NETWORK)));
  }
  // We need to keep the download around until the UI thread has finalized
  // the name.
}

// This method will be sent via a user action, or shutdown on the UI thread, and
// run on the download thread. Since this message has been sent from the UI
// thread, the download may have already completed and won't exist in our map.
void DownloadFileManager::CancelDownload(DownloadId global_id) {
  VLOG(20) << __FUNCTION__ << "()" << " id = " << global_id;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DownloadFileMap::iterator it = downloads_.find(global_id);
  if (it == downloads_.end())
    return;

  DownloadFile* download_file = it->second;
  VLOG(20) << __FUNCTION__ << "()"
           << " download_file = " << download_file->DebugString();
  download_file->Cancel();

  EraseDownload(global_id);
}

void DownloadFileManager::CompleteDownload(DownloadId global_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!ContainsKey(downloads_, global_id))
    return;

  DownloadFile* download_file = downloads_[global_id];

  VLOG(20) << " " << __FUNCTION__ << "()"
           << " id = " << global_id
           << " download_file = " << download_file->DebugString();

  download_file->Detach();

  EraseDownload(global_id);
}

void DownloadFileManager::OnDownloadManagerShutdown(DownloadManager* manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(manager);

  std::set<DownloadFile*> to_remove;

  for (DownloadFileMap::iterator i = downloads_.begin();
       i != downloads_.end(); ++i) {
    DownloadFile* download_file = i->second;
    if (download_file->GetDownloadManager() == manager) {
      download_file->CancelDownloadRequest();
      to_remove.insert(download_file);
    }
  }

  for (std::set<DownloadFile*>::iterator i = to_remove.begin();
       i != to_remove.end(); ++i) {
    downloads_.erase((*i)->global_id());
    delete *i;
  }
}

// Actions from the UI thread and run on the download thread

// The DownloadManager in the UI thread has provided an intermediate .crdownload
// name for the download specified by 'id'. Rename the in progress download.
//
// There are 2 possible rename cases where this method can be called:
// 1. tmp -> foo.crdownload (not final, safe)
// 2. tmp-> Unconfirmed.xxx.crdownload (not final, dangerous)
void DownloadFileManager::RenameInProgressDownloadFile(
    DownloadId global_id, const FilePath& full_path) {
  VLOG(20) << __FUNCTION__ << "()" << " id = " << global_id
           << " full_path = \"" << full_path.value() << "\"";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  DownloadFile* download_file = GetDownloadFile(global_id);
  if (!download_file)
    return;

  VLOG(20) << __FUNCTION__ << "()"
           << " download_file = " << download_file->DebugString();

  net::Error rename_error = download_file->Rename(full_path);
  if (net::OK != rename_error) {
    // Error. Between the time the UI thread generated 'full_path' to the time
    // this code runs, something happened that prevents us from renaming.
    CancelDownloadOnRename(global_id, rename_error);
  }
}

// The DownloadManager in the UI thread has provided a final name for the
// download specified by 'id'. Rename the download that's in the process
// of completing.
//
// There are 2 possible rename cases where this method can be called:
// 1. foo.crdownload -> foo (final, safe)
// 2. Unconfirmed.xxx.crdownload -> xxx (final, validated)
void DownloadFileManager::RenameCompletingDownloadFile(
    DownloadId global_id,
    const FilePath& full_path,
    bool overwrite_existing_file) {
  VLOG(20) << __FUNCTION__ << "()" << " id = " << global_id
           << " overwrite_existing_file = " << overwrite_existing_file
           << " full_path = \"" << full_path.value() << "\"";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  DownloadFile* download_file = GetDownloadFile(global_id);
  if (!download_file)
    return;

  DCHECK(download_file->GetDownloadManager());
  DownloadManager* download_manager = download_file->GetDownloadManager();

  VLOG(20) << __FUNCTION__ << "()"
           << " download_file = " << download_file->DebugString();

  int uniquifier = 0;
  FilePath new_path = full_path;
  if (!overwrite_existing_file) {
    // Make our name unique at this point, as if a dangerous file is
    // downloading and a 2nd download is started for a file with the same
    // name, they would have the same path.  This is because we uniquify
    // the name on download start, and at that time the first file does
    // not exists yet, so the second file gets the same name.
    // This should not happen in the SAFE case, and we check for that in the UI
    // thread.
    uniquifier = DownloadFile::GetUniquePathNumber(new_path);
    if (uniquifier > 0) {
      DownloadFile::AppendNumberToPath(&new_path, uniquifier);
    }
  }

  // Rename the file, overwriting if necessary.
  net::Error rename_error = download_file->Rename(new_path);
  if (net::OK != rename_error) {
    // Error. Between the time the UI thread generated 'full_path' to the time
    // this code runs, something happened that prevents us from renaming.
    CancelDownloadOnRename(global_id, rename_error);
    return;
  }

#if defined(OS_MACOSX)
  // Done here because we only want to do this once; see
  // http://crbug.com/13120 for details.
  download_file->AnnotateWithSourceInformation();
#endif

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          download_manager, &DownloadManager::OnDownloadRenamedToFinalName,
          global_id.local(), new_path, uniquifier));
}

// Called only from RenameInProgressDownloadFile and
// RenameCompletingDownloadFile on the FILE thread.
void DownloadFileManager::CancelDownloadOnRename(
    DownloadId global_id, net::Error rename_error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  DownloadFile* download_file = GetDownloadFile(global_id);
  if (!download_file)
    return;

  DownloadManager* download_manager = download_file->GetDownloadManager();
  if (!download_manager) {
    // Without a download manager, we can't cancel the request normally, so we
    // need to do it here.  The normal path will also update the download
    // history before canceling the request.
    download_file->CancelDownloadRequest();
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(download_manager,
                        &DownloadManager::OnDownloadInterrupted,
                        global_id.local(),
                        download_file->bytes_so_far(),
                        ConvertNetErrorToInterruptReason(
                            rename_error,
                            DOWNLOAD_INTERRUPT_FROM_DISK)));
}

void DownloadFileManager::EraseDownload(DownloadId global_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!ContainsKey(downloads_, global_id))
    return;

  DownloadFile* download_file = downloads_[global_id];

  VLOG(20) << " " << __FUNCTION__ << "()"
           << " id = " << global_id
           << " download_file = " << download_file->DebugString();

  downloads_.erase(global_id);

  delete download_file;

  if (downloads_.empty())
    StopUpdateTimer();
}
