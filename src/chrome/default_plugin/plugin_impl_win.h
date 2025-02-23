// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_DEFAULT_PLUGIN_PLUGIN_IMPL_WIN_H_
#define CHROME_DEFAULT_PLUGIN_PLUGIN_IMPL_WIN_H_
#pragma once

#include <string>

#include "chrome/default_plugin/install_dialog.h"
#include "chrome/default_plugin/plugin_database_handler.h"
#include "chrome/default_plugin/plugin_installer_base.h"
#include "chrome/default_plugin/plugin_install_job_monitor.h"
#include "chrome/default_plugin/plugin_main.h"
#include "third_party/npapi/bindings/npapi.h"
#include "ui/base/win/window_impl.h"
#include "webkit/plugins/npapi/default_plugin_shared.h"

// Possible plugin installer states.
enum PluginInstallerState {
  PluginInstallerStateUndefined,
  PluginListDownloadInitiated,
  PluginListDownloaded,
  PluginListDownloadedPluginNotFound,
  PluginListDownloadFailed,
  PluginDownloadInitiated,
  PluginDownloadCompleted,
  PluginDownloadFailed,
  PluginInstallerLaunchSuccess,
  PluginInstallerLaunchFailure
};

class PluginInstallDialog;
class PluginDatabaseHandler;

// Provides the plugin installation functionality. This class is
// instantiated with the information like the mime type of the
// target plugin, the display mode, etc.
class PluginInstallerImpl : public PluginInstallerBase,
                            public ui::WindowImpl {
 public:
  static const int kRefreshPluginsMessage  = WM_APP + 1;

  // mode is the plugin instantiation mode, i.e. whether it is a full
  // page plugin (NP_FULL) or an embedded plugin (NP_EMBED)
  explicit PluginInstallerImpl(int16 mode);
  virtual ~PluginInstallerImpl();

  BEGIN_MSG_MAP_EX(PluginInstallerImpl)
    MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackGround)
    MESSAGE_HANDLER(WM_PAINT, OnPaint)
    MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
    MESSAGE_HANDLER(kRefreshPluginsMessage, OnRefreshPlugins)
    MESSAGE_HANDLER(WM_COPYDATA, OnCopyData)
    MESSAGE_HANDLER(WM_SETCURSOR, OnSetCursor)
    MESSAGE_HANDLER(
        webkit::npapi::default_plugin::kInstallMissingPluginMessage,
        OnInstallPluginMessage)
  END_MSG_MAP()

  // Initializes the plugin with the instance information, mime type
  // and the list of parameters passed down to the plugin from the webpage.
  //
  // Parameters:
  // module_handle
  //   The handle to the dll in which this object is instantiated.
  // instance
  //   The plugins opaque instance handle.
  // mime_type
  //   Identifies the third party plugin which would be eventually installed.
  // argc
  //   Indicates the count of arguments passed in from the webpage.
  // argv
  //   Pointer to the arguments.
  // Returns true on success.
  bool Initialize(HINSTANCE module_handle, NPP instance, NPMIMEType mime_type,
                  int16 argc, char* argn[], char* argv[]);

  // Displays the default plugin UI.
  //
  // Parameters:
  // window_info
  //   The window info passed to npapi.
  bool NPP_SetWindow(NPWindow* window_info);

  // Destroys the install dialog and the plugin window.
  void Shutdown();

  // Starts plugin download. Spawns the plugin installer after it is
  // downloaded.
  void DownloadPlugin();

  // Indicates that the plugin download was cancelled.
  void DownloadCancelled();

  // Initializes the plugin download stream.
  //
  // Parameters:
  // stream
  //   Pointer to the new stream being created.
  void NewStream(NPStream* stream);

  // Uninitializes the plugin download stream.
  //
  // Parameters:
  // stream
  //   Pointer to the stream being destroyed.
  // reason
  //   Indicates why the stream is being destroyed.
  //
  void DestroyStream(NPStream* stream, NPError reason);

  // Determines whether the plugin is ready to accept data.
  // We only accept data when we have initiated a download for the plugin
  // database.
  //
  // Parameters:
  // stream
  //   Pointer to the stream being destroyed.
  // Returns true if the plugin is ready to accept data.
  bool WriteReady(NPStream* stream);

  // Delivers data to the plugin instance.
  //
  // Parameters:
  // stream
  //   Pointer to the stream being destroyed.
  // offset
  //   Indicates the data offset.
  // buffer_length
  //   Indicates the length of the data buffer.
  // buffer
  //   Pointer to the actual buffer.
  // Returns the number of bytes actually written, 0 on error.
  int32 Write(NPStream* stream, int32 offset, int32 buffer_length,
              void* buffer);

  // Handles notifications received in response to GetURLNotify calls issued
  // by the plugin.
  //
  // Parameters:
  // url
  //   Pointer to the URL.
  // reason
  //   Describes why the notification was sent.
  void URLNotify(const char* url, NPReason reason);

  // Used by the renderer to indicate plugin install through the infobar.
  int16 NPP_HandleEvent(void* event);

  const std::string& mime_type() const { return mime_type_; }

  // Replaces a resource string with the placeholder passed in as an argument
  //
  // Parameters:
  // message_id_with_placeholders
  //   The resource id of the string with placeholders. This is only used if
  //   the placeholder string (the replacement_string) parameter is valid.
  // message_id_without_placeholders
  //   The resource id of the string to be returned if the placeholder is
  //   empty.
  // replacement_string
  //   The placeholder which replaces tokens in the string identified by
  //   resource id message_id_with_placeholders.
  // Returns a string which has the placeholders replaced, or the string
  // without placeholders.
  static std::wstring ReplaceStringForPossibleEmptyReplacement(
      int message_id_with_placeholders, int message_id_without_placeholders,
      const std::wstring& replacement_string);

  // Setter/getter combination to set and retreieve the current
  // state of the plugin installer.
  void set_plugin_installer_state(PluginInstallerState new_state) {
    plugin_installer_state_ = new_state;
  }

  PluginInstallerState plugin_installer_state() const {
    return plugin_installer_state_;
  }

  // Getter for the NPP instance member.
  NPP instance() const {
    return instance_;
  }

  // Returns whether or not the UI layout is right-to-left (such as Hebrew or
  // Arabic).
  static bool IsRTLLayout();

 protected:
  // Window message handlers.
  LRESULT OnPaint(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnEraseBackGround(UINT message, WPARAM wparam, LPARAM lparam,
                            BOOL& handled);
  LRESULT OnLButtonDown(UINT message, WPARAM wparam, LPARAM lparam,
                        BOOL& handled);
  LRESULT OnSetCursor(UINT message, WPARAM wparam, LPARAM lparam,
                      BOOL& handled);
  // Handles the install plugin message coming from the plugin infobar.
  LRESULT OnInstallPluginMessage(UINT message, WPARAM wparam, LPARAM lparam,
                                 BOOL& handled);

  // Refreshes the loaded plugin list and reloads the current page.
  LRESULT OnRefreshPlugins(UINT message, WPARAM wparam, LPARAM lparam,
                           BOOL& handled);

  // Launches the third party plugin installer. This message is
  // received when the request to download the installer, initiated by
  // plugin completes.
  LRESULT OnCopyData(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

  // Displays the plugin install confirmation dialog.
  void ShowInstallDialog();

  // Clears the current display state.
  void ClearDisplay();

  // Displays the status message identified by the message resource id
  // passed in.
  //
  // Parameters:
  // message_resource_id parameter
  //   The resource id of the message to be displayed.
  void DisplayStatus(int message_resource_id);

  // Displays status information for the third party plugin which is needed
  // by the page.
  void DisplayAvailablePluginStatus();

  // Displays information related to third party plugin download failure.
  void DisplayPluginDownloadFailedStatus();

  // Enables the plugin window if required and initiates an update of the
  // the plugin window.
  void RefreshDisplay();

  // Create tooltip window.
  bool CreateToolTip();

  // Update ToolTip text with the message shown inside the default plugin.
  void UpdateToolTip();

  // Initializes resources like the icon, fonts, etc needed by the plugin
  // installer
  //
  // Parameters:
  // module_handle
  //   Handle to the dll in which this object is instantiated.
  // Returns true on success.
  bool InitializeResources(HINSTANCE module_handle);

  // Paints user action messages to the plugin window. These include messages
  // like whether the user should click on the plugin window to download the
  // plugin, etc.
  //
  // Parameters:
  // paint_dc
  //   The device context returned in BeginPaint.
  // x_origin
  //   Horizontal reference point indicating where the text is to be displayed.
  // y_origin
  //   Vertical reference point indicating where the text is to be displayed.
  //
  void PaintUserActionInformation(HDC paint_dc, int x_origin, int y_origin);

 private:
  // Notify the renderer that plugin is available to download.
  void NotifyPluginStatus(int status);

  // The plugins opaque instance handle
  NPP instance_;
  // The plugin instantiation mode (NP_FULL or NP_EMBED)
  int16 mode_;
  // The handle to the icon displayed in the plugin installation window.
  HICON icon_;
  // The Get plugin link message string displayed at the top left corner of
  // the plugin window.
  std::wstring get_plugin_link_message_;
  // The command string displayed in the plugin installation window.
  std::wstring command_;
  // An additional message displayed at times by the plugin.
  std::wstring optional_additional_message_;
  // Set to true if plugin finder has been disabled.
  bool disable_plugin_finder_;
  // The current stream.
  NPStream* plugin_install_stream_;
  // The plugin finder URL.
  std::string plugin_finder_url_;
  // The desired mime type.
  std::string mime_type_;
  // The desired language.
  std::string desired_language_;
  // The plugin name.
  std::wstring plugin_name_;
  // The actual download URL.
  std::string plugin_download_url_;
  // Indicates if the plugin download URL points to an exe.
  bool plugin_download_url_for_display_;
  // The current state of the plugin installer.
  PluginInstallerState plugin_installer_state_;
  // Used to display the UI for plugin installation.
  PluginInstallDialog* install_dialog_;
  // To enable auto refresh of the plugin window once the installation
  // is complete, we spawn the installation process in a job, and monitor
  // IO completion events on the job. When the active process count of the
  // job falls to zero, we initiate an auto refresh of the plugin list
  // which enables the downloaded plugin to be instantiated.
  // The completion events from the job are monitored in an independent
  // thread.
  scoped_refptr<PluginInstallationJobMonitorThread>
      installation_job_monitor_thread_;
  // This object handles download and parsing of the plugins database.
  PluginDatabaseHandler plugin_database_handler_;
  // Indicates if the left click to download/refresh should be enabled or not.
  bool enable_click_;
  // Handles to the fonts used to display text in the plugin window.
  HFONT bold_font_;
  HFONT regular_font_;
  HFONT underline_font_;
  // Tooltip Window.
  HWND tooltip_;

  // Count of plugin instances.
  static int instance_count_;

  // Set to true if the plugin install infobar is to be shown.
  static bool show_install_infobar_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstallerImpl);
};


#endif  // CHROME_DEFAULT_PLUGIN_PLUGIN_IMPL_WIN_H_
