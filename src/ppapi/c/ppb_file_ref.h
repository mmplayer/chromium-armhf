/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_file_ref.idl modified Wed Aug 24 20:52:42 2011. */

#ifndef PPAPI_C_PPB_FILE_REF_H_
#define PPAPI_C_PPB_FILE_REF_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/pp_var.h"

#define PPB_FILEREF_INTERFACE_1_0 "PPB_FileRef;1.0"
#define PPB_FILEREF_INTERFACE PPB_FILEREF_INTERFACE_1_0

/**
 * @file
 * This file defines the API to create a file reference or "weak pointer" to a
 * file in a file system.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_FileRef</code> struct represents a "weak pointer" to a file in
 * a file system.  This struct contains a <code>PP_FileSystemType</code>
 * identifier and a file path string.
 */
struct PPB_FileRef {
  /**
   * Create() creates a weak pointer to a file in the given file system. File
   * paths are POSIX style.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a file
   * system.
   * @param[in] path A path to the file.
   *
   * @return A <code>PP_Resource</code> corresponding to a file reference if
   * successful or 0 if the path is malformed.
   */
  PP_Resource (*Create)(PP_Resource file_system, const char* path);
  /**
   * IsFileRef() determines if the provided resource is a file reference.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a file
   * reference.
   *
   * @return <code>PP_TRUE</code> if the resource is a
   * <code>PPB_FileRef</code>, <code>PP_FALSE</code> if the resource is
   * invalid or some type other than <code>PPB_FileRef</code>.
   */
  PP_Bool (*IsFileRef)(PP_Resource resource);
  /**
   * GetFileSystemType() returns the type of the file system.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   *
   * @return A <code>PP_FileSystemType</code> with the file system type if
   * valid or <code>PP_FILESYSTEMTYPE_INVALID</code> if the provided resource
   * is not a valid file reference.
   */
  PP_FileSystemType (*GetFileSystemType)(PP_Resource file_ref);
  /**
   * GetName() returns the name of the file.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   *
   * @return A <code>PP_Var</code> containing the name of the file.  The value
   * returned by this function does not include any path components (such as
   * the name of the parent directory, for example). It is just the name of the
   * file. Use GetPath() to get the full file path.
   */
  struct PP_Var (*GetName)(PP_Resource file_ref);
  /**
   * GetPath() returns the absolute path of the file.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   *
   * @return A <code>PP_Var</code> containing the absolute path of the file.
   * This function fails if the file system type is
   * <code>PP_FileSystemType_External</code>.
   */
  struct PP_Var (*GetPath)(PP_Resource file_ref);
  /**
   * GetParent() returns the parent directory of this file.  If
   * <code>file_ref</code> points to the root of the filesystem, then the root
   * is returned.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   *
   * @return A <code>PP_Resource</code> containing the parent directory of the
   * file. This function fails if the file system type is
   * <code>PP_FileSystemType_External</code>.
   */
  PP_Resource (*GetParent)(PP_Resource file_ref);
  /**
   * MakeDirectory() makes a new directory in the file system as well as any
   * parent directories if the <code>make_ancestors</code> argument is
   * <code>PP_TRUE</code>.  It is not valid to make a directory in the external
   * file system.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   * @param[in] make_ancestors A <code>PP_Bool</code> set to
   * <code>PP_TRUE</code> to make ancestor directories or <code>PP_FALSE</code>
   * if ancestor directories are not needed.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   * Fails if the directory already exists or if ancestor directories do not
   * exist and <code>make_ancestors</code> was not passed as
   * <code>PP_TRUE</code>.
   */
  int32_t (*MakeDirectory)(PP_Resource directory_ref,
                           PP_Bool make_ancestors,
                           struct PP_CompletionCallback callback);
  /**
   * Touch() Updates time stamps for a file.  You must have write access to the
   * file if it exists in the external filesystem.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   * @param[in] last_access_time The last time the file was accessed.
   * @param[in] last_modified_time The last time the file was modified.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion of Touch().
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   */
  int32_t (*Touch)(PP_Resource file_ref,
                   PP_Time last_access_time,
                   PP_Time last_modified_time,
                   struct PP_CompletionCallback callback);
  /**
   * Delete() deletes a file or directory. If <code>file_ref</code> refers to
   * a directory, then the directory must be empty. It is an error to delete a
   * file or directory that is in use.  It is not valid to delete a file in
   * the external file system.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion of Delete().
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   */
  int32_t (*Delete)(PP_Resource file_ref,
                    struct PP_CompletionCallback callback);
  /**
   * Rename() renames a file or directory.  Arguments <code>file_ref</code> and
   * <code>new_file_ref</code> must both refer to files in the same file
   * system. It is an error to rename a file or directory that is in use.  It
   * is not valid to rename a file in the external file system.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   * @param[in] new_file_ref A <code>PP_Resource</code> corresponding to a new
   * file reference.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion of Rename().
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   */
  int32_t (*Rename)(PP_Resource file_ref,
                    PP_Resource new_file_ref,
                    struct PP_CompletionCallback callback);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_FILE_REF_H_ */

