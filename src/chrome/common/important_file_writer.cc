// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/important_file_writer.h"

#include <stdio.h>

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "base/time.h"

using base::TimeDelta;

namespace {

const int kDefaultCommitIntervalMs = 10000;

class WriteToDiskTask : public Task {
 public:
  WriteToDiskTask(const FilePath& path, const std::string& data)
      : path_(path),
        data_(data) {
  }

  virtual void Run() {
    // Write the data to a temp file then rename to avoid data loss if we crash
    // while writing the file. Ensure that the temp file is on the same volume
    // as target file, so it can be moved in one step, and that the temp file
    // is securely created.
    FilePath tmp_file_path;
    if (!file_util::CreateTemporaryFileInDir(path_.DirName(), &tmp_file_path)) {
      LogFailure(FAILED_CREATING, "could not create temporary file");
      return;
    }

    int flags = base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE;
    base::PlatformFile tmp_file =
        base::CreatePlatformFile(tmp_file_path, flags, NULL, NULL);
    if (tmp_file == base::kInvalidPlatformFileValue) {
      LogFailure(FAILED_OPENING, "could not open temporary file");
      return;
    }

    CHECK_LE(data_.length(), static_cast<size_t>(kint32max));
    int bytes_written = base::WritePlatformFile(
        tmp_file, 0, data_.data(), static_cast<int>(data_.length()));
    base::FlushPlatformFile(tmp_file);  // Ignore return value.

    if (!base::ClosePlatformFile(tmp_file)) {
      LogFailure(FAILED_CLOSING, "failed to close temporary file");
      file_util::Delete(tmp_file_path, false);
      return;
    }

    if (bytes_written < static_cast<int>(data_.length())) {
      LogFailure(FAILED_WRITING, "error writing, bytes_written=" +
                 base::IntToString(bytes_written));
      file_util::Delete(tmp_file_path, false);
      return;
    }

    if (!file_util::ReplaceFile(tmp_file_path, path_)) {
      LogFailure(FAILED_RENAMING, "could not rename temporary file");
      file_util::Delete(tmp_file_path, false);
      return;
    }
  }

 private:
  enum TempFileFailure {
    FAILED_CREATING,
    FAILED_OPENING,
    FAILED_CLOSING,
    FAILED_WRITING,
    FAILED_RENAMING,
    TEMP_FILE_FAILURE_MAX
  };

  void LogFailure(TempFileFailure failure_code, const std::string& message) {
    UMA_HISTOGRAM_ENUMERATION("ImportantFile.TempFileFailures", failure_code,
                              TEMP_FILE_FAILURE_MAX);
    PLOG(WARNING) << "temp file failure: " << path_.value()
                  << " : " << message;
  }

  const FilePath path_;
  const std::string data_;

  DISALLOW_COPY_AND_ASSIGN(WriteToDiskTask);
};

}  // namespace

ImportantFileWriter::ImportantFileWriter(
    const FilePath& path, base::MessageLoopProxy* file_message_loop_proxy)
        : path_(path),
          file_message_loop_proxy_(file_message_loop_proxy),
          serializer_(NULL),
          commit_interval_(TimeDelta::FromMilliseconds(
              kDefaultCommitIntervalMs)) {
  DCHECK(CalledOnValidThread());
  DCHECK(file_message_loop_proxy_.get());
}

ImportantFileWriter::~ImportantFileWriter() {
  // We're usually a member variable of some other object, which also tends
  // to be our serializer. It may not be safe to call back to the parent object
  // being destructed.
  DCHECK(!HasPendingWrite());
}

bool ImportantFileWriter::HasPendingWrite() const {
  DCHECK(CalledOnValidThread());
  return timer_.IsRunning();
}

void ImportantFileWriter::WriteNow(const std::string& data) {
  DCHECK(CalledOnValidThread());
  if (data.length() > static_cast<size_t>(kint32max)) {
    NOTREACHED();
    return;
  }

  if (HasPendingWrite())
    timer_.Stop();

  if (!file_message_loop_proxy_->PostTask(
      FROM_HERE, new WriteToDiskTask(path_, data))) {
    // Posting the task to background message loop is not expected
    // to fail, but if it does, avoid losing data and just hit the disk
    // on the current thread.
    NOTREACHED();

    WriteToDiskTask write_task(path_, data);
    write_task.Run();
  }
}

void ImportantFileWriter::ScheduleWrite(DataSerializer* serializer) {
  DCHECK(CalledOnValidThread());

  DCHECK(serializer);
  serializer_ = serializer;

  if (!timer_.IsRunning()) {
    timer_.Start(FROM_HERE, commit_interval_, this,
                 &ImportantFileWriter::DoScheduledWrite);
  }
}

void ImportantFileWriter::DoScheduledWrite() {
  DCHECK(serializer_);
  std::string data;
  if (serializer_->SerializeData(&data)) {
    WriteNow(data);
  } else {
    LOG(WARNING) << "failed to serialize data to be saved in "
                 << path_.value();
  }
  serializer_ = NULL;
}
