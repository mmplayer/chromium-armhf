// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by Chrome.

#ifndef CHROME_COMMON_CHROME_SWITCHES_H_
#define CHROME_COMMON_CHROME_SWITCHES_H_
#pragma once

#include "build/build_config.h"

#include "base/base_switches.h"
#include "content/public/common/content_switches.h"

namespace switches {

// -----------------------------------------------------------------------------
// Can't find the switch you are looking for? Try looking in
// ui/gfx/gl/gl_switches.cc or base/base_switches.cc or
// content/common/content_switches.cc or media/base/media_switches.cc instead.
// -----------------------------------------------------------------------------

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kAllowCrossOriginAuthPrompt[];
extern const char kAllowFileAccess[];
extern const char kAllowHTTPBackgroundPage[];
extern const char kAllowOutdatedPlugins[];
extern const char kAllowRunningInsecureContent[];
extern const char kAllowScriptingGallery[];
extern const char kAllowWebSocketProxy[];
extern const char kAllowWebUICompositing[];
extern const char kAlwaysAuthorizePlugins[];
extern const char kAppId[];
extern const char kApp[];
extern const char kAppsCheckoutURL[];
extern const char kAppsGalleryReturnTokens[];
extern const char kAppsGalleryURL[];
extern const char kAppsGalleryDownloadURL[];
extern const char kAppsGalleryUpdateURL[];
extern const char kAppsNewInstallBubble[];
extern const char kAppsNoThrob[];
extern const char kAuthNegotiateDelegateWhitelist[];
extern const char kAuthSchemes[];
extern const char kAuthServerWhitelist[];
extern const char kAutomationClientChannelID[];
extern const char kAutomationReinitializeOnChannelError[];
extern const char kBlockReadingThirdPartyCookies[];
extern const char kCheckForUpdateIntervalSec[];
extern const char kChromeVersion[];
extern const char kCipherSuiteBlacklist[];
extern const char kCloudPrintDeleteFile[];
extern const char kCloudPrintFile[];
extern const char kCloudPrintJobTitle[];
extern const char kCloudPrintFileType[];
extern const char kCloudPrintPrintTicket[];
extern const char kCloudPrintProxyId[];
extern const char kCloudPrintServiceURL[];
extern const char kComponentUpdaterDebug[];
extern const char kConflictingModulesCheck[];
extern const char kCountry[];
extern const char kCrashOnHangSeconds[];
extern const char kCrashOnHangThreads[];
extern const char kCrashOnLive[];
extern const char kDebugDevToolsFrontend[];
extern const char kDebugEnableFrameToggle[];
extern const char kDebugPrint[];
extern const char kDeviceManagementUrl[];
extern const char kDiagnostics[];
extern const char kDisableAuthNegotiateCnameLookup[];
extern const char kDisableBackgroundMode[];
extern const char kDisableBackgroundNetworking[];
extern const char kDisableClientSidePhishingDetection[];
extern const char kDisableConnectBackupJobs[];
extern const char kDisableCustomJumpList[];
extern const char kDisableCustomProtocolOSCheck[];
extern const char kDisableDefaultApps[];
extern const char kDisableDhcpWpad[];
extern const char kDisableExtensionsFileAccessCheck[];
extern const char kDisableExtensions[];
extern const char kDisableFlashSandbox[];
extern const char kDisableHistoryQuickProvider[];
extern const char kDisableHistoryURLProvider[];
extern const char kDisableInteractiveFormValidation[];
extern const char kDisableInternalFlash[];
extern const char kDisableIPv6[];
extern const char kDisableIPPooling[];
extern const char kDisablePreconnect[];
extern const char kDisablePromptOnRepost[];
extern const char kDisableRemoteFonts[];
extern const char kDisableRestoreBackgroundContents[];
extern const char kDisableSiteSpecificQuirks[];
extern const char kDisableSSL3[];
extern const char kDisableSync[];
extern const char kDisableSyncApps[];
extern const char kDisableSyncAutofill[];
extern const char kDisableSyncAutofillProfile[];
extern const char kDisableSyncBookmarks[];
extern const char kDisableSyncEncryption[];
extern const char kDisableSyncExtensions[];
extern const char kDisableSyncPasswords[];
extern const char kDisableSyncPreferences[];
extern const char kDisableSyncThemes[];
extern const char kDisableSyncTypedUrls[];
extern const char kDisableTabCloseableStateWatcher[];
extern const char kDisableTLS1[];
extern const char kDisableTranslate[];
extern const char kDisableWebResources[];
extern const char kDisableWebSecurity[];
extern const char kDisableXSSAuditor[];
extern const char kDiskCacheDir[];
extern const char kDiskCacheSize[];
extern const char kDnsLogDetails[];
extern const char kDnsPrefetchDisable[];
extern const char kDnsServer[];
extern const char kDomAutomationController[];
extern const char kDownloadsNewUI[];
extern const char kDumpHistogramsOnExit[];
extern const char kEnableAeroPeekTabs[];
extern const char kEnableAuthNegotiatePort[];
extern const char kEnableAutofillFeedback[];
extern const char kEnableAutologin[];
extern const char kEnableClearServerData[];
extern const char kEnableClickToPlay[];
extern const char kEnableCloudPrintProxy[];
extern const char kEnableCompositeToTexture[];
extern const char kEnableConnectBackupJobs[];
extern const char kEnableCrxlessWebApps[];
extern const char kEnableExperimentalExtensionApis[];
extern const char kEnableExtensionTimelineApi[];
extern const char kEnableFastback[];
extern const char kEnableFileCookies[];
extern const char kEnableImprovedDownloadProtection[];
extern const char kEnableInBrowserThumbnailing[];
extern const char kEnableIPv6[];
extern const char kEnableIPCFuzzing[];
extern const char kEnableIPPooling[];
extern const char kEnableLazyBackgroundPages[];
extern const char kEnableMacCookies[];
extern const char kEnableMemoryInfo[];
extern const char kEnableNaCl[];
extern const char kEnableNaClDebug[];
extern const char kEnableNTPBookmarkFeatures[];
extern const char kEnablePanels[];
extern const char kEnablePreconnect[];
extern const char kEnableResourceContentSettings[];
extern const char kEnableSearchProviderApiV2[];
extern const char kEnableShortcutsProvider[];
extern const char kEnableSmoothScrolling[];
// TODO(kalman): Add to about:flags when UI for syncing extension settings has
// been figured out.
extern const char kEnableSyncExtensionSettings[];
extern const char kEnableSyncOAuth[];
extern const char kEnableSyncSearchEngines[];
extern const char kEnableSyncTabs[];
extern const char kEnableSyncTabsForOtherClients[];
extern const char kEnableSyncedBookmarksFolder[];
extern const char kEnableTabGroupsContextMenu[];
extern const char kEnableTopSites[];
extern const char kEnableWatchdog[];
extern const char kEnableWebSocketOverSpdy[];
extern const char kEnableWebUITaskManager[];
extern const char kExperimentalSpellcheckerFeatures[];
extern const char kExplicitlyAllowedPorts[];
extern const char kExtensionProcess[];
extern const char kExtensionsUpdateFrequency[];
extern const char kFlagSwitchesBegin[];
extern const char kFlagSwitchesEnd[];
extern const char kFeedbackServer[];
extern const char kFileDescriptorLimit[];
extern const char kFocusExistingTabOnOpen[];
extern const char kFirstRun[];
extern const char kForceAppsPromoVisible[];
extern const char kForceCompositingMode[];
extern const char kGaiaHost[];
extern const char kGSSAPILibraryName[];
extern const char kHelp[];
extern const char kHelpShort[];
extern const char kHideIcons[];
extern const char kHomePage[];
extern const char kHostRules[];
extern const char kHostResolverParallelism[];
extern const char kHostResolverRetryAttempts[];
extern const char kHostResolverRules[];
extern const char kHstsHosts[];
extern const char kImport[];
extern const char kImportFromFile[];
extern const char kIncognito[];
extern const char kInstantFieldTrial[];
extern const char kInstantFieldTrialHidden[];
extern const char kInstantFieldTrialInstant[];
extern const char kInstantURL[];
extern const char kKeepAliveForTest[];
extern const char kLoadExtension[];
extern const char kLoadOpencryptoki[];
extern const char kUninstallExtension[];
extern const char kLogNetLog[];
extern const char kMakeDefaultBrowser[];
extern const char kMediaCacheSize[];
extern const char kMemoryProfiling[];
extern const char kMessageLoopHistogrammer[];
extern const char kMetricsRecordingOnly[];
extern const char kMinClearSiteDataFlashVersion[];
extern const char kMultiProfiles[];
extern const char kNaClDebugIP[];
extern const char kNaClDebugPorts[];
extern const char kNaClLoaderCmdPrefix[];
extern const char kNaClStartupDialog[];
extern const char kNetLogLevel[];
extern const char kNewTabPage[];
extern const char kNoDefaultBrowserCheck[];
extern const char kNoDisplayingInsecureContent[];
extern const char kNoEvents[];
extern const char kNoExperiments[];
extern const char kNoFirstRun[];
extern const char kNoProxyServer[];
extern const char kNoPings[];
extern const char kNoRunningInsecureContent[];
extern const char kNoServiceAutorun[];
extern const char kNoStartupWindow[];
extern const char kNotifyCloudPrintTokenExpired[];
extern const char kNumPacThreads[];
extern const char kOpenInNewWindow[];
extern const char kOrganicInstall[];
extern const char kPackExtension[];
extern const char kPackExtensionKey[];
extern const char kParentProfile[];
extern const char kPpapiFlashInProcess[];
extern const char kPreloadInstantSearch[];
extern const char kPrerenderFromOmnibox[];
extern const char kPrerenderFromOmniboxSwitchValueAuto[];
extern const char kPrerenderFromOmniboxSwitchValueDisabled[];
extern const char kPrerenderFromOmniboxSwitchValueEnabled[];
extern const char kPrerenderMode[];
extern const char kPrerenderModeSwitchValueAuto[];
extern const char kPrerenderModeSwitchValueDisabled[];
extern const char kPrerenderModeSwitchValueEnabled[];
extern const char kPrerenderModeSwitchValuePrefetchOnly[];
extern const char kPrint[];
extern const char kProductVersion[];
extern const char kProfileDirectory[];
extern const char kProfilingAtStart[];
extern const char kProfilingFile[];
extern const char kProfilingFlush[];
extern const char kPromoServerURL[];
extern const char kProxyAutoDetect[];
extern const char kProxyBypassList[];
extern const char kProxyPacUrl[];
extern const char kProxyServer[];
extern const char kPurgeMemoryButton[];
extern const char kReloadKilledTabs[];
extern const char kRemoteDebuggingPort[];
extern const char kRestoreLastSession[];
extern const char kSbInfoURLPrefix[];
extern const char kSbMacKeyURLPrefix[];
extern const char kSbDisableAutoUpdate[];
extern const char kSbDisableDownloadProtection[];
extern const char kEnableSdch[];
extern const char kSearchInOmniboxHint[];
extern const char kServiceAccountLsid[];
extern const char kShowAutofillTypePredictions[];
extern const char kShowCompositedLayerBorders[];
extern const char kShowCompositedLayerTree[];
extern const char kShowFPSCounter[];
extern const char kShowIcons[];
extern const char kSilentDumpOnDCHECK[];
extern const char kSocketReusePolicy[];
extern const char kStartMaximized[];
extern const char kSyncAllowInsecureXmppConnection[];
extern const char kSyncInvalidateXmppLogin[];
extern const char kSyncerThreadTimedStop[];
extern const char kSyncNotificationMethod[];
extern const char kSyncNotificationHost[];
extern const char kSyncServiceURL[];
extern const char kSyncTrySsltcpFirstForXmpp[];
extern const char kTestNaClSandbox[];
extern const char kTestName[];
extern const char kTestType[];
extern const char kTestingChannelID[];
extern const char kTryChromeAgain[];
extern const char kUninstall[];
extern const char kUseMoreWebUI[];
extern const char kUsePureViews[];
extern const char kUseSpdy[];
extern const char kIgnoreCertificateErrors[];
extern const char kMaxSpdySessionsPerDomain[];
extern const char kMaxSpdyConcurrentStreams[];
extern const char kUserDataDir[];
extern const char kVersion[];
extern const char kWinHttpProxyResolver[];

#if defined(OS_CHROMEOS)
extern const char kWebUILogin[];
extern const char kSkipOAuthLogin[];
extern const char kEnableBluetooth[];
extern const char kEnableDevicePolicy[];
extern const char kEnableGView[];
extern const char kEnableLoginImages[];
extern const char kEnableSensors[];
extern const char kEnableStaticIPConfig[];
extern const char kLoginManager[];
// TODO(avayvod): Remove this flag when it's unnecessary for testing
// purposes.
extern const char kLoginScreen[];
extern const char kLoginScreenSize[];
extern const char kMemoryWidget[];
extern const char kTestLoadLibcros[];
extern const char kLoginProfile[];
extern const char kLoginUser[];
extern const char kLoginPassword[];
extern const char kLoginUserWithNewPassword[];
extern const char kParallelAuth[];
extern const char kCandidateWindowLang[];
extern const char kGuestSession[];
extern const char kStubCros[];
extern const char kScreenSaverUrl[];
extern const char kCompressSystemFeedback[];
extern const char kAuthExtensionPath[];
extern const char kEnablePhotoEditor[];
#ifndef NDEBUG
extern const char kOobeSkipPostLogin[];
#endif
#endif

#if defined(OS_POSIX)
extern const char kEnableCrashReporter[];
extern const char kNoProcessSingletonDialog[];
#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
extern const char kPasswordStore[];
#endif
#endif

#if defined(OS_MACOSX)
extern const char kEnableExposeForTabs[];
extern const char kRelauncherProcess[];
extern const char kUseMockKeychain[];
#endif

#if !defined(OS_MACOSX)
extern const char kKioskMode[];
#endif

#if defined(TOOLKIT_VIEWS)
extern const char kDebugViewsPaint[];
extern const char kViewsDesktop[];
#endif

#if defined(TOUCH_UI)
extern const char kKeepMouseCursor[];
extern const char kTouchDevices[];
#endif

#ifndef NDEBUG
extern const char kOAuthHostUrl[];
extern const char kClearTokenService[];
extern const char kSetToken[];
extern const char kWebSocketLiveExperimentHost[];
extern const char kExposePrivateExtensionApi[];
extern const char kFileManagerExtensionPath[];
#endif

extern const char kDisablePrintPreview[];
extern const char kEnablePrintPreview[];
extern const char kEnableBenchmarking[];

bool IsPrintPreviewEnabled();

bool IsInBrowserThumbnailingEnabled();

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace switches

#endif  // CHROME_COMMON_CHROME_SWITCHES_H_
