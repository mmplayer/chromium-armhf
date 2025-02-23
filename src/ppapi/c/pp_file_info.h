/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From pp_file_info.idl modified Sat Jul 16 16:50:26 2011. */

#ifndef PPAPI_C_PP_FILE_INFO_H_
#define PPAPI_C_PP_FILE_INFO_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"

/**
 * @file
 * This file defines three enumerations for use in the PPAPI C file IO APIs.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * The <code>PP_FileType</code> enum contains file type constants.
 */
typedef enum {
  /** A regular file type */
  PP_FILETYPE_REGULAR,
  /** A directory */
  PP_FILETYPE_DIRECTORY,
  /** A catch-all for unidentified types */
  PP_FILETYPE_OTHER
} PP_FileType;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_FileType, 4);

/**
 * The <code>PP_FileSystemType</code> enum contains file system type constants.
 */
typedef enum {
  /** For identified invalid return values */
  PP_FILESYSTEMTYPE_INVALID = 0,
  /** For external file system types */
  PP_FILESYSTEMTYPE_EXTERNAL,
  /** For local persistant file system types */
  PP_FILESYSTEMTYPE_LOCALPERSISTENT,
  /** For local temporary file system types */
  PP_FILESYSTEMTYPE_LOCALTEMPORARY
} PP_FileSystemType;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_FileSystemType, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
/**
 * The <code>PP_FileInfo</code> struct represents all information about a file,
 * such as size, type, and creation time.
 */
struct PP_FileInfo {
  /** This value represents the size of the file measured in bytes */
  int64_t size;
  /**
   * This value represents the type of file as defined by the
   * <code>PP_FileType</code> enum
   */
  PP_FileType type;
  /**
   * This value represents the file system type of the file as defined by the
   * <code>PP_FileSystemType</code> enum.
   */
  PP_FileSystemType system_type;
  /**
   * This value represents the creation time of the file.
   */
  PP_Time creation_time;
  /**
   * This value represents the last time the file was accessed.
   */
  PP_Time last_access_time;
  /**
   * This value represents the last time the file was modified.
   */
  PP_Time last_modified_time;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_FileInfo, 40);
/**
 * @}
 */

#endif  /* PPAPI_C_PP_FILE_INFO_H_ */

