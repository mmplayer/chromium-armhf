// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_TRUSTED_FILE_CHOOSER_TRUSTED_H_
#define PPAPI_CPP_TRUSTED_FILE_CHOOSER_TRUSTED_H_

#include <string>

#include "ppapi/cpp/dev/file_chooser_dev.h"

namespace pp {

class FileChooser_Trusted : public FileChooser_Dev {
 public:
  /// Creates an is_null() FileChooser_Trusted object.
  FileChooser_Trusted();

  FileChooser_Trusted(const Instance* instance,
                      PP_FileChooserMode_Dev mode,
                      const Var& accept_mime_types,
                      bool save_as,
                      const std::string& suggested_file_name);

  FileChooser_Trusted(const FileChooser_Trusted& other);

  // Overrides of method in superclass. This shows without requiring a user
  // gesture (and can also show save dialogs).
  virtual int32_t Show(const CompletionCallback& cc);

 private:
  const bool save_as_;
  const std::string suggested_file_name_;
};

}  // namespace pp

#endif  // PPAPI_CPP_TRUSTED_FILE_CHOOSER_TRUSTED_H_
