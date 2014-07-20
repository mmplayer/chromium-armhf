// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include "base/logging.h"
#include "base/file_path.h"
#include "chrome/browser/importer/importer_host.h"
#include "chrome/browser/importer/importer_list.h"

#if defined(OS_WIN)
bool FirstRun::WriteEULAtoTempFile(FilePath* eula_path) {
  // TODO(beng):
  NOTIMPLEMENTED();
  return true;
}

bool FirstRun::LaunchSetupWithParam(const std::string& param,
                                    const std::wstring& value,
                                    int* ret_code) {
  // TODO(beng):
  NOTIMPLEMENTED();
  return true;
}

void FirstRun::DoDelayedInstallExtensions() {
  // TODO(beng):
  NOTIMPLEMENTED();
}

bool FirstRun::ImportSettings(Profile* profile,
                              scoped_refptr<ImporterHost> importer_host,
                              scoped_refptr<ImporterList> importer_list,
                              int items_to_import) {
  return ImportSettings(
      profile,
      importer_list->GetSourceProfileAt(0).importer_type,
      items_to_import,
      FilePath(),
      false,
      NULL);
}

bool FirstRun::ImportSettings(Profile* profile,
                              int importer_type,
                              int items_to_import,
                              const FilePath& import_bookmarks_path,
                              bool skip_first_run_ui,
                              gfx::NativeWindow parent_window) {
  // TODO(beng):
  NOTIMPLEMENTED();
  return false;
}

int FirstRun::ImportFromBrowser(Profile* profile,
                                const CommandLine& cmdline) {
  // TODO(beng):
  NOTIMPLEMENTED();
  return 0;
}
#else
// static
bool FirstRun::ImportBookmarks(const FilePath& import_bookmarks_path) {
  // TODO(beng):
  NOTIMPLEMENTED();
  return true;
}

namespace first_run {
void ShowFirstRunDialog(Profile* profile,
                        bool randomize_search_engine_experiment) {
  // TODO(saintlou):
  NOTIMPLEMENTED();
}
} // namespace first_run
#endif

// static
void FirstRun::PlatformSetup() {
  // TODO(beng):
  NOTIMPLEMENTED();
}

// static
FilePath FirstRun::MasterPrefsPath() {
  // TODO(beng):
  NOTIMPLEMENTED();
  return FilePath();
}


