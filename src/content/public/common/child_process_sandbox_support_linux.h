// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PULIC_COMMON_CHILD_PROCESS_SANDBOX_SUPPORT_LINUX_H_
#define CONTENT_PULIC_COMMON_CHILD_PROCESS_SANDBOX_SUPPORT_LINUX_H_
#pragma once

#include <stdint.h>
#include <string>

#include "content/common/content_export.h"

namespace content {

// Returns a file descriptor for a shared memory segment.
// The second argument is ignored because SHM segments are always
// mappable with PROT_EXEC on Linux.
CONTENT_EXPORT int MakeSharedMemorySegmentViaIPC(size_t length,
                                                 bool executable);

// Return a read-only file descriptor to the font which best matches the given
// properties or -1 on failure.
//   charset: specifies the language(s) that the font must cover. See
// render_sandbox_host_linux.cc for more information.
CONTENT_EXPORT int MatchFontWithFallback(const std::string& face, bool bold,
                                         bool italic, int charset);

// GetFontTable loads a specified font table from an open SFNT file.
//   fd: a file descriptor to the SFNT file. The position doesn't matter.
//   table: the table in *big-endian* format, or 0 for the whole font file.
//   output: a buffer of size output_length that gets the data.  can be 0, in
//     which case output_length will be set to the required size in bytes.
//   output_length: size of output, if it's not 0.
//
//   returns: true on success.
CONTENT_EXPORT bool GetFontTable(int fd, uint32_t table, uint8_t* output,
                                 size_t* output_length);

};  // namespace content

#endif  // CONTENT_PULIC_COMMON_CHILD_PROCESS_SANDBOX_SUPPORT_LINUX_H_
