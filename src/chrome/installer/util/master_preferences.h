// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains functions processing master preference file used by
// setup and first run.

#ifndef CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_H_
#define CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_H_
#pragma once

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"

class FilePath;

namespace base {
class DictionaryValue;
}

namespace installer {

#if !defined(OS_MACOSX)
// This is the default name for the master preferences file used to pre-set
// values in the user profile at first run.
const char kDefaultMasterPrefs[] = "master_preferences";
#endif

// The master preferences is a JSON file with the same entries as the
// 'Default\Preferences' file. This function parses the distribution
// section of the preferences file.
//
// A prototypical 'master_preferences' file looks like this:
//
// {
//   "distribution": {
//      "alternate_shortcut_text": false,
//      "oem_bubble": false,
//      "chrome_shortcut_icon_index": 0,
//      "create_all_shortcuts": true,
//      "import_bookmarks": false,
//      "import_bookmarks_from_file": "c:\\path",
//      "import_history": false,
//      "import_home_page": false,
//      "import_search_engine": true,
//      "ping_delay": 40,
//      "show_welcome_page": true,
//      "skip_first_run_ui": true,
//      "do_not_launch_chrome": false,
//      "make_chrome_default": false,
//      "make_chrome_default_for_user": true,
//      "require_eula": true,
//      "system_level": false,
//      "verbose_logging": true
//   },
//   "browser": {
//      "show_home_button": true
//   },
//   "bookmark_bar": {
//      "show_on_all_tabs": true
//   },
//   "first_run_tabs": [
//      "http://gmail.com",
//      "https://igoogle.com"
//   ],
//   "homepage": "http://example.org",
//   "homepage_is_newtabpage": false
// }
//
// A reserved "distribution" entry in the file is used to group related
// installation properties. This entry will be ignored at other times.
// This function parses the 'distribution' entry and returns a combination
// of MasterPrefResult.

class MasterPreferences {
 public:
  // Construct a master preferences from the current process' current command
  // line. Equivalent to calling
  // MasterPreferences(*CommandLine::ForCurrentProcess()).
  MasterPreferences();

  // Parses the command line and optionally reads the master preferences file
  // to get distribution related install options (if the "installerdata" switch
  // is present in the command line.
  // The options from the preference file and command line are merged, with the
  // ones from the command line taking precedence in case of a conflict.
  explicit MasterPreferences(const CommandLine& cmd_line);

  // Parses a specific preferences file and does not merge any command line
  // switches with the distribution dictionary.
  explicit MasterPreferences(const FilePath& prefs_path);

  ~MasterPreferences();

  // Each of the Get methods below returns true if the named value was found in
  // the distribution dictionary and its value assigned to the 'value'
  // parameter.  If the value wasn't found, the return value is false.
  bool GetBool(const std::string& name, bool* value) const;
  bool GetInt(const std::string& name, int* value) const;
  bool GetString(const std::string& name, std::string* value) const;

  // As part of the master preferences an optional section indicates the tabs
  // to open during first run. An example is the following:
  //
  //  {
  //    "first_run_tabs": [
  //       "http://google.com/f1",
  //       "https://google.com/f2"
  //    ]
  //  }
  //
  // Note that the entries are usually urls but they don't have to be.
  //
  // This function returns the list as a vector of GURLs.  If the master
  // preferences file does not contain such a list the vector is empty.
  std::vector<GURL> GetFirstRunTabs() const;

  // The master preferences can also contain a regular extensions
  // preference block. If so, the extensions referenced there will be
  // installed during the first run experience.
  // An extension can go in the master prefs needs just the basic
  // elements such as:
  //   1- An extension entry under settings, assigned by the gallery
  //   2- The "location" : 1 entry
  //   3- A minimal "manifest" block with key, name, permissions, update url
  //      and version. The version needs to be lower than the version of
  //      the extension that is hosted in the gallery.
  //   4- The "path" entry with the version as last component
  //   5- The "state" : 1 entry
  //
  // The following is an example of a master pref file that installs
  // Google XYZ:
  //
  //  {
  //     "extensions": {
  //        "settings": {
  //           "ppflmjolhbonpkbkooiamcnenbmbjcbb": {
  //              "location": 1,
  //              "manifest": {
  //                 "key": "MIGfMA0GCSqGSIb3DQEBAQUAA4<rest of key ommited>",
  //                 "name": "Google XYZ (Installing...)",
  //                 "permissions": [ "tabs", "http://xyz.google.com/" ],
  //                 "update_url": "http://fixme.com/fixme/fixme/crx",
  //                 "version": "0.0"
  //              },
  //              "path": "ppflmjolhbonpkbkooiamcnenbmbjcbb\\0.0",
  //              "state": 1
  //           }
  //        }
  //     }
  //  }
  //
  bool GetExtensionsBlock(base::DictionaryValue** extensions) const;

  // Returns true iff the master preferences were successfully read from a file.
  bool read_from_file() const {
    return preferences_read_from_file_;
  }

  bool install_ceee() const {
    return ceee_;
  }

  bool install_chrome() const {
    return chrome_;
  }

  bool install_chrome_frame() const {
    return chrome_frame_;
  }

  bool is_multi_install() const {
    return multi_install_;
  }

  // Returns a static preference object that has been initialized with the
  // CommandLine object for the current process.
  // NOTE: Must not be called before CommandLine::Init() is called!
  // OTHER NOTE: Not thread safe.
  static const MasterPreferences& ForCurrentProcess();

 protected:
  void InitializeProductFlags();

  void InitializeFromCommandLine(const CommandLine& cmd_line);

 protected:
  scoped_ptr<base::DictionaryValue> master_dictionary_;
  base::DictionaryValue* distribution_;
  bool preferences_read_from_file_;
  bool ceee_;
  bool chrome_;
  bool chrome_frame_;
  bool multi_install_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MasterPreferences);
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_H_
