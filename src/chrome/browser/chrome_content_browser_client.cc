// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include "base/command_line.h"
#include "chrome/app/breakpad_mac.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/chrome_benchmarking_message_filter.h"
#include "chrome/browser/chrome_plugin_message_filter.h"
#include "chrome/browser/chrome_quota_permission_context.h"
#include "chrome/browser/chrome_worker_message_filter.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_message_handler.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/extension_webrequest_api.h"
#include "chrome/browser/geolocation/chrome_access_token_store.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/printing/printing_message_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/browser/renderer_host/chrome_render_view_host_observer.h"
#include "chrome/browser/search_engines/search_provider_install_state_message_filter.h"
#include "chrome/browser/speech/chrome_speech_input_manager.h"
#include "chrome/browser/spellchecker/spellcheck_message_filter.h"
#include "chrome/browser/ssl/ssl_add_cert_handler.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "chrome/browser/tab_contents/tab_contents_ssl_helper.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/chrome_web_ui_factory.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_url_handler.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/plugin_process_host.h"
#include "content/browser/renderer_host/browser_render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/resource_context.h"
#include "content/browser/site_instance.h"
#include "content/browser/ssl/ssl_cert_error_handler.h"
#include "content/browser/ssl/ssl_client_auth_handler.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "content/common/desktop_notification_messages.h"
#include "grit/ui_resources.h"
#include "net/base/cookie_monster.h"
#include "net/base/cookie_options.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/chrome_browser_main_chromeos.h"
#elif defined(USE_AURA)
#include "chrome/browser/chrome_browser_main_aura.h"
#elif defined(OS_WIN)
#include "chrome/browser/chrome_browser_main_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/chrome_browser_main_mac.h"
#elif defined(OS_LINUX)
#include "chrome/browser/chrome_browser_main_gtk.h"
#endif

#if defined(OS_LINUX)
#include "base/linux_util.h"
#include "chrome/browser/crash_handler_host_linux.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/tab_contents/tab_contents_view_views.h"
#elif defined(OS_LINUX)
#include "chrome/browser/tab_contents/tab_contents_view_gtk.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/tab_contents/tab_contents_view_mac.h"
#endif

#if defined(USE_NSS)
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#endif


#if defined(USE_AURA) || defined(TOUCH_UI)
#include "chrome/browser/renderer_host/render_widget_host_view_views.h"
#elif defined(OS_WIN)
#include "chrome/browser/renderer_host/render_widget_host_view_views.h"
#include "content/browser/renderer_host/render_widget_host_view_win.h"
#elif defined(OS_LINUX)
#include "content/browser/renderer_host/render_widget_host_view_gtk.h"
#elif defined(OS_MACOSX)
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#endif

namespace {

// Handles rewriting Web UI URLs.
bool HandleWebUI(GURL* url, content::BrowserContext* browser_context) {
  if (!ChromeWebUIFactory::GetInstance()->UseWebUIForURL(browser_context, *url))
    return false;

  // Special case the new tab page. In older versions of Chrome, the new tab
  // page was hosted at chrome-internal:<blah>. This might be in people's saved
  // sessions or bookmarks, so we say any URL with that scheme triggers the new
  // tab page.
  if (url->SchemeIs(chrome::kChromeInternalScheme)) {
    // Rewrite it with the proper new tab URL.
    *url = GURL(chrome::kChromeUINewTabURL);
  }

  return true;
}

// Used by the GetPrivilegeRequiredByUrl() and GetProcessPrivilege() functions
// below.  Extension, and isolated apps require different privileges to be
// granted to their RenderProcessHosts.  This classification allows us to make
// sure URLs are served by hosts with the right set of privileges.
enum RenderProcessHostPrivilege {
  PRIV_NORMAL,
  PRIV_EXTENSION,
  PRIV_ISOLATED,
};

RenderProcessHostPrivilege GetPrivilegeRequiredByUrl(
    const GURL& url,
    ExtensionService* service) {
  // Default to a normal renderer cause it is lower privileged. This should only
  // occur if the URL on a site instance is either malformed, or uninitialized.
  // If it is malformed, then there is no need for better privileges anyways.
  // If it is uninitialized, but eventually settles on being an a scheme other
  // than normal webrenderer, the navigation logic will correct us out of band
  // anyways.
  if (!url.is_valid())
    return PRIV_NORMAL;

  if (url.SchemeIs(chrome::kExtensionScheme)) {
    const Extension* extension = service->GetExtensionByURL(url);
    if (extension && extension->is_storage_isolated()) {
      return PRIV_ISOLATED;
    }

    return PRIV_EXTENSION;
  }

  return PRIV_NORMAL;
}

RenderProcessHostPrivilege GetProcessPrivilege(
    RenderProcessHost* process_host,
    ExtensionProcessManager* extension_process_manager) {
  if (extension_process_manager->IsExtensionProcess(process_host->id())) {
    if (extension_process_manager->IsStorageIsolatedForProcess(
        process_host->id())) {
      return PRIV_ISOLATED;
    }
    return PRIV_EXTENSION;
  }

  return PRIV_NORMAL;
}

}  // namespace

namespace chrome {

content::BrowserMainParts* ChromeContentBrowserClient::CreateBrowserMainParts(
    const MainFunctionParams& parameters) {
#if defined(OS_CHROMEOS)
  return new ChromeBrowserMainPartsChromeos(parameters);
#elif defined(USE_AURA)
  return new ChromeBrowserMainPartsAura(parameters);
#elif defined(OS_WIN)
  return new ChromeBrowserMainPartsWin(parameters);
#elif defined(OS_MACOSX)
  return new ChromeBrowserMainPartsMac(parameters);
#elif defined(OS_LINUX)
  return new ChromeBrowserMainPartsGtk(parameters);
#else
  return NULL;
#endif
}

RenderWidgetHostView* ChromeContentBrowserClient::CreateViewForWidget(
    RenderWidgetHost* widget) {
#if defined(USE_AURA) || defined(TOUCH_UI)
  return new RenderWidgetHostViewViews(widget);
#elif defined(OS_WIN)
  if (views::Widget::IsPureViews())
    return new RenderWidgetHostViewViews(widget);
  return new RenderWidgetHostViewWin(widget);
#elif defined(OS_LINUX)
  return new RenderWidgetHostViewGtk(widget);
#elif defined(OS_MACOSX)
  return render_widget_host_view_mac::CreateRenderWidgetHostView(widget);
#else
#error Need to create your platform ViewForWidget here.
#endif
}

TabContentsView* ChromeContentBrowserClient::CreateTabContentsView(
    TabContents* tab_contents) {
#if defined(TOOLKIT_VIEWS)
  return new TabContentsViewViews(tab_contents);
#elif defined(OS_LINUX)
  return new TabContentsViewGtk(tab_contents);
#elif defined(OS_MACOSX)
  return tab_contents_view_mac::CreateTabContentsView(tab_contents);
#else
#error Need to create your platform TabContentsView here.
#endif
}

void ChromeContentBrowserClient::RenderViewHostCreated(
    RenderViewHost* render_view_host) {

  SiteInstance* site_instance = render_view_host->site_instance();
  Profile* profile = Profile::FromBrowserContext(
      site_instance->browsing_instance()->browser_context());

  new ChromeRenderViewHostObserver(render_view_host,
                                   profile->GetNetworkPredictor());
  new ExtensionMessageHandler(render_view_host);
}

void ChromeContentBrowserClient::BrowserRenderProcessHostCreated(
    BrowserRenderProcessHost* host) {
  int id = host->id();
  Profile* profile = Profile::FromBrowserContext(host->browser_context());
  host->channel()->AddFilter(new ChromeRenderMessageFilter(
      id, profile, profile->GetRequestContextForRenderProcess(id)));
  host->channel()->AddFilter(new PrintingMessageFilter());
  host->channel()->AddFilter(
      new SearchProviderInstallStateMessageFilter(id, profile));
  host->channel()->AddFilter(new SpellCheckMessageFilter(id));
  host->channel()->AddFilter(new ChromeBenchmarkingMessageFilter(
      id, profile, profile->GetRequestContextForRenderProcess(id)));

  host->Send(new ChromeViewMsg_SetIsIncognitoProcess(
      profile->IsOffTheRecord()));

  SendExtensionWebRequestStatusToHost(host);
}

void ChromeContentBrowserClient::PluginProcessHostCreated(
    PluginProcessHost* host) {
  host->AddFilter(new ChromePluginMessageFilter(host));
}

void ChromeContentBrowserClient::WorkerProcessHostCreated(
    WorkerProcessHost* host) {
  host->AddFilter(new ChromeWorkerMessageFilter(host));
}

content::WebUIFactory* ChromeContentBrowserClient::GetWebUIFactory() {
  return ChromeWebUIFactory::GetInstance();
}

GURL ChromeContentBrowserClient::GetEffectiveURL(
    content::BrowserContext* browser_context, const GURL& url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  // Get the effective URL for the given actual URL. If the URL is part of an
  // installed app, the effective URL is an extension URL with the ID of that
  // extension as the host. This has the effect of grouping apps together in
  // a common SiteInstance.
  if (!profile || !profile->GetExtensionService())
    return url;

  const Extension* extension =
      profile->GetExtensionService()->GetExtensionByWebExtent(url);
  if (!extension)
    return url;

  // Bookmark apps do not use the hosted app process model, and should be
  // treated as normal URLs.
  if (extension->from_bookmark())
    return url;

  // If the URL is part of an extension's web extent, convert it to an
  // extension URL.
  return extension->GetResourceURL(url.path());
}

bool ChromeContentBrowserClient::ShouldUseProcessPerSite(
    content::BrowserContext* browser_context, const GURL& effective_url) {
  // Non-extension URLs should generally use process-per-site-instance.
  // Because we expect to use the effective URL, URLs for hosted apps (apart
  // from bookmark apps) should have an extension scheme by now.
  if (!effective_url.SchemeIs(chrome::kExtensionScheme))
    return false;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile || !profile->GetExtensionService())
    return false;

  const Extension* extension =
      profile->GetExtensionService()->GetExtensionByURL(effective_url);
  if (!extension)
    return false;

  // If the URL is part of a hosted app that does not have the background
  // permission, we want to give each instance its own process to improve
  // responsiveness.
  if (extension->GetType() == Extension::TYPE_HOSTED_APP &&
      !extension->HasAPIPermission(ExtensionAPIPermission::kBackground))
    return false;

  // Hosted apps that have the background permission must use process per site,
  // since all instances can make synchronous calls to the background window.
  // Other extensions should use process per site as well.
  return true;
}

bool ChromeContentBrowserClient::IsURLSameAsAnySiteInstance(const GURL& url) {
  return url == GURL(chrome::kChromeUICrashURL) ||
         url == GURL(chrome::kChromeUIKillURL) ||
         url == GURL(chrome::kChromeUIHangURL) ||
         url == GURL(chrome::kChromeUIShorthangURL);
}

bool ChromeContentBrowserClient::IsSuitableHost(
    RenderProcessHost* process_host,
    const GURL& site_url) {
  Profile* profile =
      Profile::FromBrowserContext(process_host->browser_context());
  ExtensionProcessManager* extension_process_manager =
      profile->GetExtensionProcessManager();
  ExtensionService* service = profile->GetExtensionService();

  // These may be NULL during tests. In that case, just assume any site can
  // share any host.
  if (!extension_process_manager || !service)
    return true;

  return GetProcessPrivilege(process_host, extension_process_manager) ==
      GetPrivilegeRequiredByUrl(site_url, service);
}

bool ChromeContentBrowserClient::ShouldSwapProcessesForNavigation(
    const GURL& current_url,
    const GURL& new_url) {
  if (current_url.is_empty()) {
    // Always choose a new process when navigating to extension URLs. The
    // process grouping logic will combine all of a given extension's pages
    // into the same process.
    if (new_url.SchemeIs(chrome::kExtensionScheme))
      return true;

    return false;
  }

  // Also, we must switch if one is an extension and the other is not the exact
  // same extension.
  if (current_url.SchemeIs(chrome::kExtensionScheme) ||
      new_url.SchemeIs(chrome::kExtensionScheme)) {
    if (current_url.GetOrigin() != new_url.GetOrigin())
      return true;
  }

  return false;
}

std::string ChromeContentBrowserClient::GetCanonicalEncodingNameByAliasName(
    const std::string& alias_name) {
  return CharacterEncoding::GetCanonicalEncodingNameByAliasName(alias_name);
}

void ChromeContentBrowserClient::AppendExtraCommandLineSwitches(
    CommandLine* command_line, int child_process_id) {
#if defined(USE_LINUX_BREAKPAD)
  if (IsCrashReporterEnabled()) {
    command_line->AppendSwitchASCII(switches::kEnableCrashReporter,
        child_process_logging::GetClientId() + "," + base::GetLinuxDistro());
  }
#elif defined(OS_MACOSX)
  if (IsCrashReporterEnabled()) {
    command_line->AppendSwitchASCII(switches::kEnableCrashReporter,
                                    child_process_logging::GetClientId());
  }
#endif  // OS_MACOSX

  if (logging::DialogsAreSuppressed())
    command_line->AppendSwitch(switches::kNoErrorDialogs);

  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  if (process_type == switches::kRendererProcess) {
    FilePath user_data_dir =
        browser_command_line.GetSwitchValuePath(switches::kUserDataDir);
    if (!user_data_dir.empty())
      command_line->AppendSwitchPath(switches::kUserDataDir, user_data_dir);
#if defined(OS_CHROMEOS)
    const std::string& login_profile =
        browser_command_line.GetSwitchValueASCII(switches::kLoginProfile);
    if (!login_profile.empty())
      command_line->AppendSwitchASCII(switches::kLoginProfile, login_profile);
#endif

    RenderProcessHost* process = RenderProcessHost::FromID(child_process_id);

    Profile* profile = Profile::FromBrowserContext(process->browser_context());

    ExtensionProcessManager* extension_process_manager =
        profile->GetExtensionProcessManager();
    if (extension_process_manager->IsExtensionProcess(
        process->id())) {
      command_line->AppendSwitch(switches::kExtensionProcess);
    }

    PrefService* prefs = profile->GetPrefs();
    // Currently this pref is only registered if applied via a policy.
    if (prefs->HasPrefPath(prefs::kDisable3DAPIs) &&
        prefs->GetBoolean(prefs::kDisable3DAPIs)) {
      // Turn this policy into a command line switch.
      command_line->AppendSwitch(switches::kDisable3DAPIs);
    }

    // Disable client-side phishing detection in the renderer if it is disabled
    // in the Profile preferences or the browser process.
    if (!prefs->GetBoolean(prefs::kSafeBrowsingEnabled) ||
        !g_browser_process->safe_browsing_detection_service()) {
      command_line->AppendSwitch(switches::kDisableClientSidePhishingDetection);
    }

    static const char* const kSwitchNames[] = {
      switches::kAllowHTTPBackgroundPage,
      switches::kAllowScriptingGallery,
      switches::kAppsCheckoutURL,
      switches::kAppsGalleryURL,
      switches::kCloudPrintServiceURL,
      switches::kDebugPrint,
      switches::kDisablePrintPreview,
      switches::kDomAutomationController,
      switches::kDumpHistogramsOnExit,
      switches::kEnableClickToPlay,
      switches::kEnableCrxlessWebApps,
      switches::kEnableExperimentalExtensionApis,
      switches::kEnableInBrowserThumbnailing,
      switches::kEnableIPCFuzzing,
      switches::kEnableNaCl,
      switches::kEnablePrintPreview,
      switches::kEnableSearchProviderApiV2,
      switches::kEnableWatchdog,
      switches::kExperimentalSpellcheckerFeatures,
      switches::kMemoryProfiling,
      switches::kMessageLoopHistogrammer,
      switches::kNoRunningInsecureContent,
      switches::kPpapiFlashArgs,
      switches::kPpapiFlashInProcess,
      switches::kPpapiFlashPath,
      switches::kPpapiFlashVersion,
      switches::kProfilingAtStart,
      switches::kProfilingFile,
      switches::kProfilingFlush,
      switches::kSilentDumpOnDCHECK,
      switches::kEnableBenchmarking,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   arraysize(kSwitchNames));
  } else if (process_type == switches::kUtilityProcess) {
    if (browser_command_line.HasSwitch(
            switches::kEnableExperimentalExtensionApis)) {
      command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
    }
  } else if (process_type == switches::kPluginProcess) {
    static const char* const kSwitchNames[] = {
  #if defined(OS_CHROMEOS)
      switches::kLoginProfile,
  #endif
      switches::kMemoryProfiling,
      switches::kSilentDumpOnDCHECK,
      switches::kUserDataDir,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   arraysize(kSwitchNames));
  } else if (process_type == switches::kZygoteProcess) {
    static const char* const kSwitchNames[] = {
      switches::kUserDataDir,  // Make logs go to the right file.
      // Load (in-process) Pepper plugins in-process in the zygote pre-sandbox.
      switches::kPpapiFlashInProcess,
      switches::kPpapiFlashPath,
      switches::kPpapiFlashVersion,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   arraysize(kSwitchNames));
  }

  // The command line switch kEnableBenchmarking needs to be specified along
  // with the kEnableStatsTable switch to ensure that the stats table global
  // is initialized correctly.
  if (command_line->HasSwitch(switches::kEnableBenchmarking))
    DCHECK(command_line->HasSwitch(switches::kEnableStatsTable));
}

std::string ChromeContentBrowserClient::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

std::string ChromeContentBrowserClient::GetAcceptLangs(const TabContents* tab) {
  Profile* profile = Profile::FromBrowserContext(tab->browser_context());
  return profile->GetPrefs()->GetString(prefs::kAcceptLanguages);
}

SkBitmap* ChromeContentBrowserClient::GetDefaultFavicon() {
  ResourceBundle &rb = ResourceBundle::GetSharedInstance();
#if defined(TOUCH_UI)
  // In touch builds, we want large default favicons for the tabstrip, but in
  // other places (such as bookmark, manage search engines, homepage) we assume
  // default favicons are 16x16.
  return rb.GetBitmapNamed(IDR_DEFAULT_LARGE_FAVICON);
#else
  return rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
#endif
}

bool ChromeContentBrowserClient::AllowAppCache(
    const GURL& manifest_url,
    const GURL& first_party,
    const content::ResourceContext& context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data =
      reinterpret_cast<ProfileIOData*>(context.GetUserData(NULL));
  ContentSetting setting = io_data->GetHostContentSettingsMap()->
      GetCookieContentSetting(manifest_url, first_party, true);
  DCHECK(setting != CONTENT_SETTING_DEFAULT);
  return setting != CONTENT_SETTING_BLOCK;
}

bool ChromeContentBrowserClient::AllowGetCookie(
    const GURL& url,
    const GURL& first_party,
    const net::CookieList& cookie_list,
    const content::ResourceContext& context,
    int render_process_id,
    int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data =
      reinterpret_cast<ProfileIOData*>(context.GetUserData(NULL));
  ContentSetting setting = io_data->GetHostContentSettingsMap()->
      GetCookieContentSetting(url, first_party, false);
  bool allow = setting == CONTENT_SETTING_ALLOW ||
      setting == CONTENT_SETTING_SESSION_ONLY;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(
          &TabSpecificContentSettings::CookiesRead,
          render_process_id, render_view_id, url, cookie_list, !allow));
  return allow;
}

bool ChromeContentBrowserClient::AllowSetCookie(
    const GURL& url,
    const GURL& first_party,
    const std::string& cookie_line,
    const content::ResourceContext& context,
    int render_process_id,
    int render_view_id,
    net::CookieOptions* options) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data =
      reinterpret_cast<ProfileIOData*>(context.GetUserData(NULL));
  ContentSetting setting = io_data->GetHostContentSettingsMap()->
      GetCookieContentSetting(url, first_party, true);

  if (setting == CONTENT_SETTING_SESSION_ONLY)
    options->set_force_session();

  bool allow = setting == CONTENT_SETTING_ALLOW ||
      setting == CONTENT_SETTING_SESSION_ONLY;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(
          &TabSpecificContentSettings::CookieChanged,
          render_process_id, render_view_id, url, cookie_line, *options,
          !allow));
  return allow;
}

bool ChromeContentBrowserClient::AllowSaveLocalState(
    const content::ResourceContext& context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data =
      reinterpret_cast<ProfileIOData*>(context.GetUserData(NULL));
  return !io_data->clear_local_state_on_exit()->GetValue();
}

net::URLRequestContext*
ChromeContentBrowserClient::OverrideRequestContextForURL(
    const GURL& url, const content::ResourceContext& context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (url.SchemeIs(chrome::kExtensionScheme)) {
    ProfileIOData* io_data =
        reinterpret_cast<ProfileIOData*>(context.GetUserData(NULL));
    return io_data->extensions_request_context();
  }

  return NULL;
}

QuotaPermissionContext*
ChromeContentBrowserClient::CreateQuotaPermissionContext() {
  return new ChromeQuotaPermissionContext();
}

void ChromeContentBrowserClient::OpenItem(const FilePath& path) {
  // On Mac, this call needs to be done on the UI thread.  On other platforms,
  // do it on the FILE thread so we don't slow down UI.
#if defined(OS_MACOSX)
  platform_util::OpenItem(path);
#else
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(&platform_util::OpenItem, path));
#endif
}

void ChromeContentBrowserClient::ShowItemInFolder(const FilePath& path) {
#if defined(OS_MACOSX)
  // Mac needs to run this operation on the UI thread.
  platform_util::ShowItemInFolder(path);
#else
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(&platform_util::ShowItemInFolder, path));
#endif
}

void ChromeContentBrowserClient::AllowCertificateError(
    SSLCertErrorHandler* handler,
    bool overridable,
    const base::Callback<void(SSLCertErrorHandler*, bool)>& callback) {
  // If the tab is being prerendered, cancel the prerender and the request.
  TabContents* tab = tab_util::GetTabContentsByID(
      handler->render_process_host_id(),
      handler->tab_contents_id());
  if (!tab) {
    NOTREACHED();
    return;
  }
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(
          Profile::FromBrowserContext(tab->browser_context()));
  if (prerender_manager && prerender_manager->IsTabContentsPrerendering(tab)) {
    if (prerender_manager->prerender_tracker()->TryCancel(
            handler->render_process_host_id(),
            handler->tab_contents_id(),
            prerender::FINAL_STATUS_SSL_ERROR)) {
      handler->CancelRequest();
      return;
    }
  }

  // Otherwise, display an SSL blocking page.
  SSLBlockingPage* blocking_page = new SSLBlockingPage(
      handler, overridable, callback);
  blocking_page->Show();
}

void ChromeContentBrowserClient::SelectClientCertificate(
    int render_process_id,
    int render_view_id,
    SSLClientAuthHandler* handler) {
  TabContents* tab = tab_util::GetTabContentsByID(
      render_process_id, render_view_id);
  if (!tab) {
    NOTREACHED();
    return;
  }

  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(tab);
  wrapper->ssl_helper()->SelectClientCertificate(handler);
}

void ChromeContentBrowserClient::AddNewCertificate(
    net::URLRequest* request,
    net::X509Certificate* cert,
    int render_process_id,
    int render_view_id) {
  // The handler will run the UI and delete itself when it's finished.
  new SSLAddCertHandler(request, cert, render_process_id, render_view_id);
}

void ChromeContentBrowserClient::RequestDesktopNotificationPermission(
    const GURL& source_origin,
    int callback_context,
    int render_process_id,
    int render_view_id) {
  RenderViewHost* rvh = RenderViewHost::FromID(
      render_process_id, render_view_id);
  if (!rvh) {
    NOTREACHED();
    return;
  }

  RenderProcessHost* process = rvh->process();
  Profile* profile = Profile::FromBrowserContext(process->browser_context());
  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(profile);
  service->RequestPermission(
      source_origin, render_process_id, render_view_id, callback_context,
      tab_util::GetTabContentsByID(render_process_id, render_view_id));
}

WebKit::WebNotificationPresenter::Permission
    ChromeContentBrowserClient::CheckDesktopNotificationPermission(
        const GURL& source_url,
        const content::ResourceContext& context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ProfileIOData* io_data =
      reinterpret_cast<ProfileIOData*>(context.GetUserData(NULL));

  const Extension* extension =
      io_data->GetExtensionInfoMap()->extensions().GetByURL(source_url);
  if (extension &&
      extension->HasAPIPermission(ExtensionAPIPermission::kNotification)) {
    return WebKit::WebNotificationPresenter::PermissionAllowed;
  }

  // Fall back to the regular notification preferences, which works on an
  // origin basis.
  return io_data->GetNotificationService() ?
      io_data->GetNotificationService()->HasPermission(source_url.GetOrigin()) :
      WebKit::WebNotificationPresenter::PermissionNotAllowed;
}

void ChromeContentBrowserClient::ShowDesktopNotification(
    const DesktopNotificationHostMsg_Show_Params& params,
    int render_process_id,
    int render_view_id,
    bool worker) {
  RenderViewHost* rvh = RenderViewHost::FromID(
      render_process_id, render_view_id);
  if (!rvh) {
    NOTREACHED();
    return;
  }

  RenderProcessHost* process = rvh->process();
  Profile* profile = Profile::FromBrowserContext(process->browser_context());
  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(profile);
  service->ShowDesktopNotification(
    params, render_process_id, render_view_id,
    worker ? DesktopNotificationService::WorkerNotification :
        DesktopNotificationService::PageNotification);
}

void ChromeContentBrowserClient::CancelDesktopNotification(
    int render_process_id,
    int render_view_id,
    int notification_id) {
  RenderViewHost* rvh = RenderViewHost::FromID(
      render_process_id, render_view_id);
  if (!rvh) {
    NOTREACHED();
    return;
  }

  RenderProcessHost* process = rvh->process();
  Profile* profile = Profile::FromBrowserContext(process->browser_context());
  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(profile);
  service->CancelDesktopNotification(
      render_process_id, render_view_id, notification_id);
}

bool ChromeContentBrowserClient::CanCreateWindow(
    const GURL& source_url,
    WindowContainerType container_type,
    const content::ResourceContext& context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // If the opener is trying to create a background window but doesn't have
  // the appropriate permission, fail the attempt.
  if (container_type == WINDOW_CONTAINER_TYPE_BACKGROUND) {
    ProfileIOData* io_data =
        reinterpret_cast<ProfileIOData*>(context.GetUserData(NULL));
    const Extension* extension =
        io_data->GetExtensionInfoMap()->extensions().GetByURL(source_url);
    return (extension &&
            extension->HasAPIPermission(ExtensionAPIPermission::kBackground));
  }
  return true;
}

std::string ChromeContentBrowserClient::GetWorkerProcessTitle(
    const GURL& url, const content::ResourceContext& context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Check if it's an extension-created worker, in which case we want to use
  // the name of the extension.
  ProfileIOData* io_data =
      reinterpret_cast<ProfileIOData*>(context.GetUserData(NULL));
  const Extension* extension =
      io_data->GetExtensionInfoMap()->extensions().GetByID(url.host());
  return extension ? extension->name() : std::string();
}

ResourceDispatcherHost*
    ChromeContentBrowserClient::GetResourceDispatcherHost() {
  return g_browser_process->resource_dispatcher_host();
}

ui::Clipboard* ChromeContentBrowserClient::GetClipboard() {
  return g_browser_process->clipboard();
}

MHTMLGenerationManager*
    ChromeContentBrowserClient::GetMHTMLGenerationManager() {
  return g_browser_process->mhtml_generation_manager();
}

DevToolsManager* ChromeContentBrowserClient::GetDevToolsManager() {
  return g_browser_process->devtools_manager();
}

net::NetLog* ChromeContentBrowserClient::GetNetLog() {
  return g_browser_process->net_log();
}

speech_input::SpeechInputManager*
    ChromeContentBrowserClient::GetSpeechInputManager() {
  return speech_input::ChromeSpeechInputManager::GetInstance();
}

AccessTokenStore* ChromeContentBrowserClient::CreateAccessTokenStore() {
  return new ChromeAccessTokenStore();
}

bool ChromeContentBrowserClient::IsFastShutdownPossible() {
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  return !browser_command_line.HasSwitch(switches::kChromeFrame);
}

WebPreferences ChromeContentBrowserClient::GetWebkitPrefs(
    content::BrowserContext* browser_context, bool is_web_ui) {
  return RenderViewHostDelegateHelper::GetWebkitPrefs(browser_context,
                                                      is_web_ui);
}

void ChromeContentBrowserClient::UpdateInspectorSetting(
    RenderViewHost* rvh, const std::string& key, const std::string& value) {
  RenderViewHostDelegateHelper::UpdateInspectorSetting(
      rvh->process()->browser_context(), key, value);
}

void ChromeContentBrowserClient::ClearInspectorSettings(RenderViewHost* rvh) {
  RenderViewHostDelegateHelper::ClearInspectorSettings(
      rvh->process()->browser_context());
}

void ChromeContentBrowserClient::BrowserURLHandlerCreated(
    BrowserURLHandler* handler) {
  // Add the default URL handlers.
  handler->AddHandlerPair(&ExtensionWebUI::HandleChromeURLOverride,
                          BrowserURLHandler::null_handler());
  handler->AddHandlerPair(BrowserURLHandler::null_handler(),
                          &ExtensionWebUI::HandleChromeURLOverrideReverse);

  // about:
  handler->AddHandlerPair(&WillHandleBrowserAboutURL,
                          BrowserURLHandler::null_handler());
  // chrome: & friends.
  handler->AddHandlerPair(&HandleWebUI,
                          BrowserURLHandler::null_handler());
}

void ChromeContentBrowserClient::ClearCache(RenderViewHost* rvh) {
  Profile* profile = Profile::FromBrowserContext(
      rvh->site_instance()->GetProcess()->browser_context());
  BrowsingDataRemover* remover = new BrowsingDataRemover(profile,
      BrowsingDataRemover::EVERYTHING,
      base::Time());
  remover->Remove(BrowsingDataRemover::REMOVE_CACHE);
  // BrowsingDataRemover takes care of deleting itself when done.
}

void ChromeContentBrowserClient::ClearCookies(RenderViewHost* rvh) {
  Profile* profile = Profile::FromBrowserContext(
      rvh->site_instance()->GetProcess()->browser_context());
  BrowsingDataRemover* remover = new BrowsingDataRemover(profile,
      BrowsingDataRemover::EVERYTHING,
      base::Time());
  int remove_mask = BrowsingDataRemover::REMOVE_SITE_DATA;
  remover->Remove(remove_mask);
  // BrowsingDataRemover takes care of deleting itself when done.
}

FilePath ChromeContentBrowserClient::GetDefaultDownloadDirectory() {
  return download_util::GetDefaultDownloadDirectory();
}

#if defined(OS_LINUX)
int ChromeContentBrowserClient::GetCrashSignalFD(
    const CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kExtensionProcess)) {
    ExtensionCrashHandlerHostLinux* crash_handler =
        ExtensionCrashHandlerHostLinux::GetInstance();
    return crash_handler->GetDeathSignalSocket();
  }

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  if (process_type == switches::kRendererProcess)
    return RendererCrashHandlerHostLinux::GetInstance()->GetDeathSignalSocket();

  if (process_type == switches::kPluginProcess)
    return PluginCrashHandlerHostLinux::GetInstance()->GetDeathSignalSocket();

  if (process_type == switches::kPpapiPluginProcess)
    return PpapiCrashHandlerHostLinux::GetInstance()->GetDeathSignalSocket();

  if (process_type == switches::kGpuProcess)
    return GpuCrashHandlerHostLinux::GetInstance()->GetDeathSignalSocket();

  return -1;
}
#endif  // defined(OS_LINUX)

#if defined(OS_WIN)
const wchar_t* ChromeContentBrowserClient::GetResourceDllName() {
  return chrome::kBrowserResourcesDll;
}
#endif

#if defined(USE_NSS)
crypto::CryptoModuleBlockingPasswordDelegate*
    ChromeContentBrowserClient::GetCryptoPasswordDelegate(
        const GURL& url) {
  return browser::NewCryptoModuleBlockingDialogDelegate(
      browser::kCryptoModulePasswordKeygen, url.host());
}
#endif

}  // namespace chrome
