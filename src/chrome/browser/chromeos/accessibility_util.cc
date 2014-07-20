// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility_util.h"

#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/speech_synthesis_library.h"
#include "chrome/browser/extensions/extension_accessibility_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/file_reader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/pref_names.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/webui/web_ui.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {
namespace accessibility {

// Helper class that directly loads an extension's content scripts into
// all of the frames corresponding to a given RenderViewHost.
class ContentScriptLoader {
 public:
  // Initialize the ContentScriptLoader with the ID of the extension
  // and the RenderViewHost where the scripts should be loaded.
  ContentScriptLoader(const std::string& extension_id,
                      RenderViewHost* render_view_host)
      : extension_id_(extension_id),
        render_view_host_(render_view_host) {}

  // Call this once with the ExtensionResource corresponding to each
  // content script to be loaded.
  void AppendScript(ExtensionResource resource) {
    resources_.push(resource);
  }

  // Fianlly, call this method once to fetch all of the resources and
  // load them. This method will delete this object when done.
  void Run() {
    if (resources_.empty()) {
      delete this;
      return;
    }

    ExtensionResource resource = resources_.front();
    resources_.pop();
    scoped_refptr<FileReader> reader(new FileReader(resource, NewCallback(
        this, &ContentScriptLoader::OnFileLoaded)));
    reader->Start();
  }

 private:
  void OnFileLoaded(bool success, const std::string& data) {
    if (success) {
      ExtensionMsg_ExecuteCode_Params params;
      params.request_id = 0;
      params.extension_id = extension_id_;
      params.is_javascript = true;
      params.code = data;
      params.all_frames = true;
      params.in_main_world = false;
      render_view_host_->Send(new ExtensionMsg_ExecuteCode(
          render_view_host_->routing_id(), params));
    }
    Run();
  }

  std::string extension_id_;
  RenderViewHost* render_view_host_;
  std::queue<ExtensionResource> resources_;
};

void EnableAccessibility(bool enabled, WebUI* login_web_ui) {
  bool accessibility_enabled = g_browser_process &&
      g_browser_process->local_state()->GetBoolean(
          prefs::kAccessibilityEnabled);
  if (accessibility_enabled == enabled) {
    LOG(INFO) << "Accessibility is already " <<
        (enabled ? "enabled" : "disabled") << ".  Going to do nothing.";
    return;
  }

  g_browser_process->local_state()->SetBoolean(
      prefs::kAccessibilityEnabled, enabled);
  // Explicitly call SavePersistentPrefs instead of ScheduleSavePersistentPrefs
  // so that this change gets written immediately, in case the user shuts
  // down right now.  TODO(dmazzoni) switch this back to
  // ScheduleSavePersistentPrefs once http://crosbug.com/19491 is fixed.
  g_browser_process->local_state()->SavePersistentPrefs();
  ExtensionAccessibilityEventRouter::GetInstance()->
      SetAccessibilityEnabled(enabled);

  Speak(enabled ?
        l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_ACCESS_ENABLED).c_str() :
        l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_ACCESS_DISABLED).c_str(),
        false, true);

  // Load/Unload ChromeVox
  Profile* profile = ProfileManager::GetDefaultProfile();
  ExtensionService* extension_service =
      profile->GetExtensionService();
  std::string manifest = ResourceBundle::GetSharedInstance().
      GetRawDataResource(IDR_CHROMEVOX_MANIFEST).as_string();
  FilePath path = FilePath(extension_misc::kAccessExtensionPath)
      .AppendASCII(extension_misc::kChromeVoxDirectoryName);
  ExtensionService::ComponentExtensionInfo info(manifest, path);
  if (enabled) { // Load ChromeVox
    extension_service->register_component_extension(info);
    const Extension* extension =
        extension_service->LoadComponentExtension(info);

    if (login_web_ui) {
      RenderViewHost* render_view_host =
          login_web_ui->tab_contents()->render_view_host();
      ContentScriptLoader* loader = new ContentScriptLoader(
          extension->id(), render_view_host);

      for (size_t i = 0; i < extension->content_scripts().size(); i++) {
        const UserScript& script = extension->content_scripts()[i];
        for (size_t j = 0; j < script.js_scripts().size(); ++j) {
          const UserScript::File &file = script.js_scripts()[j];
          ExtensionResource resource = extension->GetResource(
              file.relative_path());
          loader->AppendScript(resource);
        }
      }
      loader->Run();  // It cleans itself up when done.
    }

    LOG(INFO) << "ChromeVox was Loaded.";
  } else { // Unload ChromeVox
    extension_service->UnloadComponentExtension(info);
    extension_service->UnregisterComponentExtension(info);
    LOG(INFO) << "ChromeVox was Unloaded.";
  }
}

void ToggleAccessibility(WebUI* login_web_ui) {
  bool accessibility_enabled = g_browser_process &&
      g_browser_process->local_state()->GetBoolean(
          prefs::kAccessibilityEnabled);
  accessibility_enabled = !accessibility_enabled;
  EnableAccessibility(accessibility_enabled, login_web_ui);
};

void Speak(const char* speak_str, bool queue, bool interruptible) {
  if (chromeos::CrosLibrary::Get()->EnsureLoaded()) {
    if (queue || !interruptible) {
      std::string props = "";
      props.append("enqueue=");
      props.append(queue ? "1;" : "0;");
      props.append("interruptible=");
      props.append(interruptible ? "1;" : "0;");
      chromeos::CrosLibrary::Get()->GetSpeechSynthesisLibrary()->
          SetSpeakProperties(props.c_str());
    }
    chromeos::CrosLibrary::Get()->GetSpeechSynthesisLibrary()->
        Speak(speak_str);
  }
}


}  // namespace accessibility
}  // namespace chromeos
