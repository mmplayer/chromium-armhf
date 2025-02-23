// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_WEBPLUGININFO_H_
#define WEBKIT_PLUGINS_WEBPLUGININFO_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"

namespace webkit {

struct WebPluginMimeType {
  WebPluginMimeType();
  // A constructor for the common case of a single file extension and an ASCII
  // description.
  WebPluginMimeType(const std::string& m,
                    const std::string& f,
                    const std::string& d);
  ~WebPluginMimeType();

  // The name of the mime type (e.g., "application/x-shockwave-flash").
  std::string mime_type;

  // A list of all the file extensions for this mime type.
  std::vector<std::string> file_extensions;

  // Description of the mime type.
  string16 description;

  // Extra parameters to include when instantiating the plugin.
  std::vector<string16> additional_param_names;
  std::vector<string16> additional_param_values;
};

// Describes an available NPAPI or Pepper plugin.
struct WebPluginInfo {
  enum PluginType {
    PLUGIN_TYPE_NPAPI,
    PLUGIN_TYPE_PEPPER_IN_PROCESS,
    PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS
  };

  WebPluginInfo();
  WebPluginInfo(const WebPluginInfo& rhs);
  ~WebPluginInfo();
  WebPluginInfo& operator=(const WebPluginInfo& rhs);

  // Special constructor only used during unit testing:
  WebPluginInfo(const string16& fake_name,
                const FilePath& fake_path,
                const string16& fake_version,
                const string16& fake_desc);

  // The name of the plugin (i.e. Flash).
  string16 name;

  // The path to the plugin file (DLL/bundle/library).
  FilePath path;

  // The version number of the plugin file (may be OS-specific)
  string16 version;

  // A description of the plugin that we get from its version info.
  string16 desc;

  // A list of all the mime types that this plugin supports.
  std::vector<WebPluginMimeType> mime_types;

  // Plugin type. See the PluginType enum.
  int type;
};

// Checks whether a plugin is a Pepper plugin, enabled or disabled.
bool IsPepperPlugin(const WebPluginInfo& plugin);

}  // namespace webkit

#endif  // WEBKIT_PLUGINS_WEBPLUGININFO_H_
