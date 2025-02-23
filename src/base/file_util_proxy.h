// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILE_UTIL_PROXY_H_
#define BASE_FILE_UTIL_PROXY_H_

#include <vector>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "base/tracked_objects.h"

namespace base {

class MessageLoopProxy;
class Time;

// This class provides asynchronous access to common file routines.
class BASE_EXPORT FileUtilProxy {
 public:
  // Holds metadata for file or directory entry. Used by ReadDirectoryCallback.
  struct Entry {
    FilePath::StringType name;
    bool is_directory;
    int64 size;
    base::Time last_modified_time;
  };

  // This callback is used by methods that report only an error code.  It is
  // valid to pass a null callback to any function that takes a StatusCallback,
  // in which case the operation will complete silently.
  typedef base::Callback<void(PlatformFileError /* error code */)>
      StatusCallback;

  typedef base::Callback<void(PlatformFileError /* error code */,
                              PassPlatformFile,
                              bool /* created */)> CreateOrOpenCallback;
  typedef base::Callback<void(PlatformFileError /* error code */,
                              PassPlatformFile,
                              FilePath)> CreateTemporaryCallback;
  typedef base::Callback<void(PlatformFileError /* error code */,
                              bool /* created */)> EnsureFileExistsCallback;
  typedef base::Callback<void(PlatformFileError /* error code */,
                              const PlatformFileInfo& /* file_info */)>
      GetFileInfoCallback;
  typedef base::Callback<void(PlatformFileError /* error code */,
                              const std::vector<Entry>&)> ReadDirectoryCallback;
  typedef base::Callback<void(PlatformFileError /* error code */,
                              const char* /* data */,
                              int /* bytes read/written */)> ReadCallback;
  typedef base::Callback<void(PlatformFileError /* error code */,
                              int /* bytes written */)> WriteCallback;

  // Creates or opens a file with the given flags. It is invalid to pass a null
  // callback. If PLATFORM_FILE_CREATE is set in |file_flags| it always tries to
  // create a new file at the given |file_path| and calls back with
  // PLATFORM_FILE_ERROR_FILE_EXISTS if the |file_path| already exists. Takes
  // ownership of |callback| and will delete it even on failure.
  static bool CreateOrOpen(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                           const FilePath& file_path,
                           int file_flags,
                           const CreateOrOpenCallback& callback);

  // Creates a temporary file for writing. The path and an open file handle are
  // returned. It is invalid to pass a null callback. The additional file flags
  // will be added on top of the default file flags which are:
  //   base::PLATFORM_FILE_CREATE_ALWAYS
  //   base::PLATFORM_FILE_WRITE
  //   base::PLATFORM_FILE_TEMPORARY.
  // Set |additional_file_flags| to 0 for synchronous writes and set to
  // base::PLATFORM_FILE_ASYNC to support asynchronous file operations.
  static bool CreateTemporary(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      int additional_file_flags,
      const CreateTemporaryCallback& callback);

  // Close the given file handle.
  static bool Close(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                    PlatformFile,
                    const StatusCallback& callback);

  // Ensures that the given |file_path| exist.  This creates a empty new file
  // at |file_path| if the |file_path| does not exist.
  // If a new file does not exist and is created at the |file_path|, |created|
  // of the callback argument is set true and |error code| is set
  // PLATFORM_FILE_OK. If the file already exists, |created| is set false and
  // |error code| is set PLATFORM_FILE_OK. If the file hasn't existed but it
  // couldn't be created for some other reasons, |created| is set false and
  // |error code| indicates the error.
  static bool EnsureFileExists(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const FilePath& file_path,
      const EnsureFileExistsCallback& callback);

  // Retrieves the information about a file. It is invalid to pass a null
  // callback.
  static bool GetFileInfo(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const FilePath& file_path,
      const GetFileInfoCallback& callback);

  static bool GetFileInfoFromPlatformFile(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      PlatformFile file,
      const GetFileInfoCallback& callback);

  static bool ReadDirectory(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                            const FilePath& file_path,
                            const ReadDirectoryCallback& callback);

  // Creates directory at given path. It's an error to create
  // if |exclusive| is true and dir already exists.
  static bool CreateDirectory(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const FilePath& file_path,
      bool exclusive,
      bool recursive,
      const StatusCallback& callback);

  // Copies a file or a directory from |src_file_path| to |dest_file_path|
  // Error cases:
  // If destination file doesn't exist or destination's parent
  // doesn't exists.
  // If source dir exists but destination path is an existing file.
  // If source file exists but destination path is an existing directory.
  // If source is a parent of destination.
  // If source doesn't exists.
  static bool Copy(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                   const FilePath& src_file_path,
                   const FilePath& dest_file_path,
                   const StatusCallback& callback);

  // Moves a file or a directory from src_file_path to dest_file_path.
  // Error cases are similar to Copy method's error cases.
  static bool Move(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const FilePath& src_file_path,
      const FilePath& dest_file_path,
      const StatusCallback& callback);

  // Deletes a file or a directory.
  // It is an error to delete a non-empty directory with recursive=false.
  static bool Delete(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                     const FilePath& file_path,
                     bool recursive,
                     const StatusCallback& callback);

  // Deletes a directory and all of its contents.
  static bool RecursiveDelete(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const FilePath& file_path,
      const StatusCallback& callback);

  // Reads from a file. On success, the file pointer is moved to position
  // |offset + bytes_to_read| in the file. The callback can be null.
  static bool Read(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      PlatformFile file,
      int64 offset,
      int bytes_to_read,
      const ReadCallback& callback);

  // Writes to a file. If |offset| is greater than the length of the file,
  // |false| is returned. On success, the file pointer is moved to position
  // |offset + bytes_to_write| in the file. The callback can be null.
  // |bytes_to_write| must be greater than zero.
  static bool Write(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      PlatformFile file,
      int64 offset,
      const char* buffer,
      int bytes_to_write,
      const WriteCallback& callback);

  // Touches a file. The callback can be null.
  static bool Touch(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      PlatformFile file,
      const Time& last_access_time,
      const Time& last_modified_time,
      const StatusCallback& callback);

  // Touches a file. The callback can be null.
  static bool Touch(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const FilePath& file_path,
      const Time& last_access_time,
      const Time& last_modified_time,
      const StatusCallback& callback);

  // Truncates a file to the given length. If |length| is greater than the
  // current length of the file, the file will be extended with zeroes.
  // The callback can be null.
  static bool Truncate(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      PlatformFile file,
      int64 length,
      const StatusCallback& callback);

  // Truncates a file to the given length. If |length| is greater than the
  // current length of the file, the file will be extended with zeroes.
  // The callback can be null.
  static bool Truncate(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const FilePath& path,
      int64 length,
      const StatusCallback& callback);

  // Flushes a file. The callback can be null.
  static bool Flush(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      PlatformFile file,
      const StatusCallback& callback);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FileUtilProxy);
};

}  // namespace base

#endif  // BASE_FILE_UTIL_PROXY_H_
