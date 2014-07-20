// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Represents the browser side of the browser <--> renderer communication
// channel. There will be one RenderProcessHost per renderer process.

#include "content/browser/renderer_host/browser_render_process_host.h"

#include <algorithm>
#include <limits>
#include <vector>

#if defined(OS_POSIX)
#include <utility>  // for pair<>
#endif

#include "base/base_switches.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/appcache/appcache_dispatcher_host.h"
#include "content/browser/browser_child_process_host.h"
#include "content/browser/browser_context.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/content_browser_client.h"
#include "content/browser/device_orientation/message_filter.h"
#include "content/browser/download/mhtml_generation_manager.h"
#include "content/browser/file_system/file_system_dispatcher_host.h"
#include "content/browser/geolocation/geolocation_dispatcher_host.h"
#include "content/browser/gpu/gpu_data_manager.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/in_process_webkit/dom_storage_message_filter.h"
#include "content/browser/in_process_webkit/indexed_db_dispatcher_host.h"
#include "content/browser/mime_registry_message_filter.h"
#include "content/browser/plugin_service.h"
#include "content/browser/renderer_host/blob_message_filter.h"
#include "content/browser/renderer_host/clipboard_message_filter.h"
#include "content/browser/renderer_host/database_message_filter.h"
#include "content/browser/renderer_host/file_utilities_message_filter.h"
#include "content/browser/renderer_host/gpu_message_filter.h"
#include "content/browser/renderer_host/media/audio_input_renderer_host.h"
#include "content/browser/renderer_host/media/audio_renderer_host.h"
#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"
#include "content/browser/renderer_host/media/video_capture_host.h"
#include "content/browser/renderer_host/p2p/socket_dispatcher_host.h"
#include "content/browser/renderer_host/pepper_file_message_filter.h"
#include "content/browser/renderer_host/pepper_message_filter.h"
#include "content/browser/renderer_host/quota_dispatcher_host.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/resource_message_filter.h"
#include "content/browser/renderer_host/socket_stream_dispatcher_host.h"
#include "content/browser/renderer_host/text_input_client_message_filter.h"
#include "content/browser/resolve_proxy_msg_helper.h"
#include "content/browser/speech/speech_input_dispatcher_host.h"
#include "content/browser/trace_message_filter.h"
#include "content/browser/user_metrics.h"
#include "content/browser/worker_host/worker_message_filter.h"
#include "content/common/child_process_info.h"
#include "content/common/child_process_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/notification_service.h"
#include "content/common/process_watcher.h"
#include "content/common/resource_messages.h"
#include "content/common/result_codes.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_switches.h"
#include "media/base/media_switches.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/gl/gl_switches.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/glue/resource_type.h"
#include "webkit/plugins/plugin_switches.h"

#if defined(OS_WIN)
#include <objbase.h>
#include "base/synchronization/waitable_event.h"
#include "content/common/section_util_win.h"
#endif

#include "third_party/skia/include/core/SkBitmap.h"

// This class creates the IO thread for the renderer when running in
// single-process mode.  It's not used in multi-process mode.
class RendererMainThread : public base::Thread {
 public:
  explicit RendererMainThread(const std::string& channel_id)
      : base::Thread("Chrome_InProcRendererThread"),
        channel_id_(channel_id),
        render_process_(NULL) {
  }

  ~RendererMainThread() {
    Stop();
  }

 protected:
  virtual void Init() {
#if defined(OS_WIN)
    CoInitialize(NULL);
#endif

    render_process_ = new RenderProcessImpl();
    render_process_->set_main_thread(new RenderThreadImpl(channel_id_));
    // It's a little lame to manually set this flag.  But the single process
    // RendererThread will receive the WM_QUIT.  We don't need to assert on
    // this thread, so just force the flag manually.
    // If we want to avoid this, we could create the InProcRendererThread
    // directly with _beginthreadex() rather than using the Thread class.
    base::Thread::SetThreadWasQuitProperly(true);
  }

  virtual void CleanUp() {
    delete render_process_;

#if defined(OS_WIN)
    CoUninitialize();
#endif
  }

 private:
  std::string channel_id_;
  // Deleted in CleanUp() on the renderer thread, so don't use a smart pointer.
  RenderProcess* render_process_;
};

namespace {

// Helper class that we pass to ResourceMessageFilter so that it can find the
// right net::URLRequestContext for a request.
class RendererURLRequestContextSelector
    : public ResourceMessageFilter::URLRequestContextSelector {
 public:
  RendererURLRequestContextSelector(content::BrowserContext* browser_context,
                                    int render_child_id)
      : request_context_(browser_context->GetRequestContextForRenderProcess(
                             render_child_id)),
        media_request_context_(browser_context->GetRequestContextForMedia()) {
  }

  virtual net::URLRequestContext* GetRequestContext(
      ResourceType::Type resource_type) {
    net::URLRequestContextGetter* request_context = request_context_;
    // If the request has resource type of ResourceType::MEDIA, we use a request
    // context specific to media for handling it because these resources have
    // specific needs for caching.
    if (resource_type == ResourceType::MEDIA)
      request_context = media_request_context_;
    return request_context->GetURLRequestContext();
  }

 private:
  virtual ~RendererURLRequestContextSelector() {}

  scoped_refptr<net::URLRequestContextGetter> request_context_;
  scoped_refptr<net::URLRequestContextGetter> media_request_context_;
};

}  // namespace

BrowserRenderProcessHost::BrowserRenderProcessHost(
    content::BrowserContext* browser_context)
        : RenderProcessHost(browser_context),
          visible_widgets_(0),
          backgrounded_(true),
          ALLOW_THIS_IN_INITIALIZER_LIST(cached_dibs_cleaner_(
                FROM_HERE, base::TimeDelta::FromSeconds(5),
                this, &BrowserRenderProcessHost::ClearTransportDIBCache)),
          accessibility_enabled_(false),
          is_initialized_(false) {
  widget_helper_ = new RenderWidgetHelper();

  ChildProcessSecurityPolicy::GetInstance()->Add(id());

  // Grant most file permissions to this renderer.
  // PLATFORM_FILE_TEMPORARY, PLATFORM_FILE_HIDDEN and
  // PLATFORM_FILE_DELETE_ON_CLOSE are not granted, because no existing API
  // requests them.
  // This is for the filesystem sandbox.
  ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
      id(), browser_context->GetPath().Append(
          fileapi::SandboxMountPointProvider::kNewFileSystemDirectory),
      base::PLATFORM_FILE_OPEN |
      base::PLATFORM_FILE_CREATE |
      base::PLATFORM_FILE_OPEN_ALWAYS |
      base::PLATFORM_FILE_CREATE_ALWAYS |
      base::PLATFORM_FILE_OPEN_TRUNCATED |
      base::PLATFORM_FILE_READ |
      base::PLATFORM_FILE_WRITE |
      base::PLATFORM_FILE_EXCLUSIVE_READ |
      base::PLATFORM_FILE_EXCLUSIVE_WRITE |
      base::PLATFORM_FILE_ASYNC |
      base::PLATFORM_FILE_WRITE_ATTRIBUTES |
      base::PLATFORM_FILE_ENUMERATE);
  // This is so that we can read and move stuff out of the old filesystem
  // sandbox.
  ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
      id(), browser_context->GetPath().Append(
          fileapi::SandboxMountPointProvider::kOldFileSystemDirectory),
      base::PLATFORM_FILE_READ | base::PLATFORM_FILE_WRITE |
      base::PLATFORM_FILE_WRITE_ATTRIBUTES | base::PLATFORM_FILE_ENUMERATE);
  // This is so that we can rename the old sandbox out of the way so that we
  // know we've taken care of it.
  ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
      id(), browser_context->GetPath().Append(
          fileapi::SandboxMountPointProvider::kRenamedOldFileSystemDirectory),
      base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_CREATE_ALWAYS |
      base::PLATFORM_FILE_WRITE);

  // Note: When we create the BrowserRenderProcessHost, it's technically
  //       backgrounded, because it has no visible listeners.  But the process
  //       doesn't actually exist yet, so we'll Background it later, after
  //       creation.
}

BrowserRenderProcessHost::~BrowserRenderProcessHost() {
  ChildProcessSecurityPolicy::GetInstance()->Remove(id());

  // We may have some unsent messages at this point, but that's OK.
  channel_.reset();
  while (!queued_messages_.empty()) {
    delete queued_messages_.front();
    queued_messages_.pop();
  }

  ClearTransportDIBCache();
}

void BrowserRenderProcessHost::EnableSendQueue() {
  is_initialized_ = false;
}

bool BrowserRenderProcessHost::Init(bool is_accessibility_enabled) {
  // calling Init() more than once does nothing, this makes it more convenient
  // for the view host which may not be sure in some cases
  if (channel_.get())
    return true;

  accessibility_enabled_ = is_accessibility_enabled;

  CommandLine::StringType renderer_prefix;
#if defined(OS_POSIX)
  // A command prefix is something prepended to the command line of the spawned
  // process. It is supported only on POSIX systems.
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  renderer_prefix =
      browser_command_line.GetSwitchValueNative(switches::kRendererCmdPrefix);
#endif  // defined(OS_POSIX)

#if defined(OS_LINUX)
  int flags = renderer_prefix.empty() ? ChildProcessHost::CHILD_ALLOW_SELF :
                                        ChildProcessHost::CHILD_NORMAL;
#else
  int flags = ChildProcessHost::CHILD_NORMAL;
#endif

  // Find the renderer before creating the channel so if this fails early we
  // return without creating the channel.
  FilePath renderer_path = ChildProcessHost::GetChildPath(flags);
  if (renderer_path.empty())
    return false;

  // Setup the IPC channel.
  const std::string channel_id =
      ChildProcessInfo::GenerateRandomChannelID(this);
  channel_.reset(new IPC::ChannelProxy(
      channel_id, IPC::Channel::MODE_SERVER, this,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)));

  // Call the embedder first so that their IPC filters have priority.
  content::GetContentClient()->browser()->BrowserRenderProcessHostCreated(this);

  CreateMessageFilters();

  if (run_renderer_in_process()) {
    // Crank up a thread and run the initialization there.  With the way that
    // messages flow between the browser and renderer, this thread is required
    // to prevent a deadlock in single-process mode.  Since the primordial
    // thread in the renderer process runs the WebKit code and can sometimes
    // make blocking calls to the UI thread (i.e. this thread), they need to run
    // on separate threads.
    in_process_renderer_.reset(new RendererMainThread(channel_id));

    base::Thread::Options options;
#if !defined(TOOLKIT_USES_GTK)
    // In-process plugins require this to be a UI message loop.
    options.message_loop_type = MessageLoop::TYPE_UI;
#else
    // We can't have multiple UI loops on GTK, so we don't support
    // in-process plugins.
    options.message_loop_type = MessageLoop::TYPE_DEFAULT;
#endif
    in_process_renderer_->StartWithOptions(options);

    OnProcessLaunched();  // Fake a callback that the process is ready.
  } else {
    // Build command line for renderer.  We call AppendRendererCommandLine()
    // first so the process type argument will appear first.
    CommandLine* cmd_line = new CommandLine(renderer_path);
    if (!renderer_prefix.empty())
      cmd_line->PrependWrapper(renderer_prefix);
    AppendRendererCommandLine(cmd_line);
    cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id);

    // Spawn the child process asynchronously to avoid blocking the UI thread.
    // As long as there's no renderer prefix, we can use the zygote process
    // at this stage.
    child_process_launcher_.reset(new ChildProcessLauncher(
#if defined(OS_WIN)
        FilePath(),
#elif defined(OS_POSIX)
        renderer_prefix.empty(),
        base::environment_vector(),
        channel_->TakeClientFileDescriptor(),
#endif
        cmd_line,
        this));

    fast_shutdown_started_ = false;
  }

  is_initialized_ = true;
  return true;
}

void BrowserRenderProcessHost::CreateMessageFilters() {
  scoped_refptr<RenderMessageFilter> render_message_filter(
      new RenderMessageFilter(
          id(),
          PluginService::GetInstance(),
          browser_context(),
          browser_context()->GetRequestContextForRenderProcess(id()),
          widget_helper_));
  channel_->AddFilter(render_message_filter);

  ResourceMessageFilter* resource_message_filter = new ResourceMessageFilter(
      id(), ChildProcessInfo::RENDER_PROCESS,
      &browser_context()->GetResourceContext(),
      new RendererURLRequestContextSelector(browser_context(), id()),
      content::GetContentClient()->browser()->GetResourceDispatcherHost());

  channel_->AddFilter(resource_message_filter);
  channel_->AddFilter(new AudioInputRendererHost(
      &browser_context()->GetResourceContext()));
  channel_->AddFilter(
      new AudioRendererHost(&browser_context()->GetResourceContext()));
  channel_->AddFilter(
      new VideoCaptureHost(&browser_context()->GetResourceContext()));
  channel_->AddFilter(
      new AppCacheDispatcherHost(browser_context()->GetAppCacheService(),
                                 id()));
  channel_->AddFilter(new ClipboardMessageFilter());
  channel_->AddFilter(
      new DOMStorageMessageFilter(id(), browser_context()->GetWebKitContext()));
  channel_->AddFilter(
      new IndexedDBDispatcherHost(id(), browser_context()->GetWebKitContext()));
  channel_->AddFilter(
      GeolocationDispatcherHost::New(
          id(), browser_context()->GetGeolocationPermissionContext()));
  channel_->AddFilter(new GpuMessageFilter(id(), widget_helper_.get()));
  channel_->AddFilter(new media_stream::MediaStreamDispatcherHost(
      &browser_context()->GetResourceContext(), id()));
  channel_->AddFilter(new PepperFileMessageFilter(id(), browser_context()));
  channel_->AddFilter(
      new PepperMessageFilter(&browser_context()->GetResourceContext()));
  channel_->AddFilter(new speech_input::SpeechInputDispatcherHost(
      id(), browser_context()->GetRequestContext(),
      browser_context()->GetSpeechInputPreferences()));
  channel_->AddFilter(
      new FileSystemDispatcherHost(browser_context()->GetRequestContext(),
                                   browser_context()->GetFileSystemContext()));
  channel_->AddFilter(new device_orientation::MessageFilter());
  channel_->AddFilter(
      new BlobMessageFilter(id(), browser_context()->GetBlobStorageContext()));
  channel_->AddFilter(new FileUtilitiesMessageFilter(id()));
  channel_->AddFilter(new MimeRegistryMessageFilter());
  channel_->AddFilter(new DatabaseMessageFilter(
      browser_context()->GetDatabaseTracker()));
#if defined(OS_MACOSX)
  channel_->AddFilter(new TextInputClientMessageFilter(id()));
#endif

  SocketStreamDispatcherHost* socket_stream_dispatcher_host =
      new SocketStreamDispatcherHost(
          new RendererURLRequestContextSelector(browser_context(), id()),
          &browser_context()->GetResourceContext());
  channel_->AddFilter(socket_stream_dispatcher_host);

  channel_->AddFilter(
      new WorkerMessageFilter(
          id(),
          &browser_context()->GetResourceContext(),
          content::GetContentClient()->browser()->GetResourceDispatcherHost(),
          NewCallbackWithReturnValue(
              widget_helper_.get(), &RenderWidgetHelper::GetNextRoutingID)));

#if defined(ENABLE_P2P_APIS)
  channel_->AddFilter(new content::P2PSocketDispatcherHost(
      &browser_context()->GetResourceContext()));
#endif

  channel_->AddFilter(new TraceMessageFilter());
  channel_->AddFilter(new ResolveProxyMsgHelper(
      browser_context()->GetRequestContextForRenderProcess(id())));
  channel_->AddFilter(new QuotaDispatcherHost(
      id(), browser_context()->GetQuotaManager(),
      content::GetContentClient()->browser()->CreateQuotaPermissionContext()));
}

int BrowserRenderProcessHost::GetNextRoutingID() {
  return widget_helper_->GetNextRoutingID();
}

void BrowserRenderProcessHost::UpdateAndSendMaxPageID(int32 page_id) {
  if (page_id > max_page_id_)
    Send(new ViewMsg_SetNextPageID(page_id + 1));
  UpdateMaxPageID(page_id);
}

void BrowserRenderProcessHost::CancelResourceRequests(int render_widget_id) {
  widget_helper_->CancelResourceRequests(render_widget_id);
}

void BrowserRenderProcessHost::CrossSiteSwapOutACK(
    const ViewMsg_SwapOut_Params& params) {
  widget_helper_->CrossSiteSwapOutACK(params);
}

bool BrowserRenderProcessHost::WaitForUpdateMsg(
    int render_widget_id,
    const base::TimeDelta& max_delay,
    IPC::Message* msg) {
  // The post task to this thread with the process id could be in queue, and we
  // don't want to dispatch a message before then since it will need the handle.
  if (child_process_launcher_.get() && child_process_launcher_->IsStarting())
    return false;

  return widget_helper_->WaitForUpdateMsg(render_widget_id, max_delay, msg);
}

void BrowserRenderProcessHost::ReceivedBadMessage() {
  if (run_renderer_in_process()) {
    // In single process mode it is better if we don't suicide but just
    // crash.
    CHECK(false);
  }
  NOTREACHED();
  base::KillProcess(GetHandle(), content::RESULT_CODE_KILLED_BAD_MESSAGE,
                    false);
}

void BrowserRenderProcessHost::WidgetRestored() {
  // Verify we were properly backgrounded.
  DCHECK_EQ(backgrounded_, (visible_widgets_ == 0));
  visible_widgets_++;
  SetBackgrounded(false);
}

void BrowserRenderProcessHost::WidgetHidden() {
  // On startup, the browser will call Hide
  if (backgrounded_)
    return;

  DCHECK_EQ(backgrounded_, (visible_widgets_ == 0));
  visible_widgets_--;
  DCHECK_GE(visible_widgets_, 0);
  if (visible_widgets_ == 0) {
    DCHECK(!backgrounded_);
    SetBackgrounded(true);
  }
}

int BrowserRenderProcessHost::VisibleWidgetCount() const {
  return visible_widgets_;
}

void BrowserRenderProcessHost::AppendRendererCommandLine(
    CommandLine* command_line) const {
  // Pass the process type first, so it shows first in process listings.
  command_line->AppendSwitchASCII(switches::kProcessType,
                                  switches::kRendererProcess);

  if (accessibility_enabled_)
    command_line->AppendSwitch(switches::kEnableAccessibility);

  // Now send any options from our own command line we want to propagate.
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  PropagateBrowserCommandLineToRenderer(browser_command_line, command_line);

  // Pass on the browser locale.
  const std::string locale =
      content::GetContentClient()->browser()->GetApplicationLocale();
  command_line->AppendSwitchASCII(switches::kLang, locale);

  // If we run base::FieldTrials, we want to pass to their state to the
  // renderer so that it can act in accordance with each state, or record
  // histograms relating to the base::FieldTrial states.
  std::string field_trial_states;
  base::FieldTrialList::StatesToString(&field_trial_states);
  if (!field_trial_states.empty()) {
    command_line->AppendSwitchASCII(switches::kForceFieldTestNameAndValue,
                                    field_trial_states);
  }

  content::GetContentClient()->browser()->AppendExtraCommandLineSwitches(
      command_line, id());

  // Appending disable-gpu-feature switches due to software rendering list.
  GpuDataManager* gpu_data_manager = GpuDataManager::GetInstance();
  DCHECK(gpu_data_manager);
  gpu_data_manager->AppendRendererCommandLine(command_line);
}

void BrowserRenderProcessHost::PropagateBrowserCommandLineToRenderer(
    const CommandLine& browser_cmd,
    CommandLine* renderer_cmd) const {
  // Propagate the following switches to the renderer command line (along
  // with any associated values) if present in the browser command line.
  static const char* const kSwitchNames[] = {
    // We propagate the Chrome Frame command line here as well in case the
    // renderer is not run in the sandbox.
    switches::kAuditAllHandles,
    switches::kAuditHandles,
    switches::kChromeFrame,
    switches::kDisable3DAPIs,
    switches::kDisableAcceleratedCompositing,
    switches::kDisableApplicationCache,
    switches::kDisableAudio,
    switches::kDisableBreakpad,
    switches::kDisableDataTransferItems,
    switches::kDisableDatabases,
    switches::kDisableDesktopNotifications,
    switches::kDisableDeviceOrientation,
    switches::kDisableFileSystem,
    switches::kDisableGeolocation,
    switches::kDisableGLMultisampling,
    switches::kDisableGLSLTranslator,
    switches::kDisableGpuDriverBugWorkarounds,
    switches::kDisableGpuVsync,
    switches::kDisableIndexedDatabase,
    switches::kDisableJavaScriptI18NAPI,
    switches::kDisableLocalStorage,
    switches::kDisableLogging,
    switches::kDisableSeccompSandbox,
    switches::kDisableSessionStorage,
    switches::kDisableSharedWorkers,
    switches::kDisableSpeechInput,
    switches::kDisableWebAudio,
    switches::kDisableWebSockets,
    switches::kEnableAccelerated2dCanvas,
    switches::kEnableAccessibilityLogging,
    switches::kEnableDCHECK,
    switches::kEnableGPUServiceLogging,
    switches::kEnableGPUClientLogging,
    switches::kEnableLogging,
    switches::kEnableMediaStream,
    switches::kDisableFullScreen,
    switches::kEnablePepperTesting,
#if defined(OS_MACOSX)
    // Allow this to be set when invoking the browser and relayed along.
    switches::kEnableSandboxLogging,
#endif
    switches::kEnableSeccompSandbox,
    switches::kEnableStatsTable,
    switches::kEnableThreadedCompositing,
    switches::kEnableVideoFullscreen,
    switches::kEnableVideoLogging,
    switches::kEnableVideoTrack,
    switches::kFullMemoryCrashReport,
#if !defined (GOOGLE_CHROME_BUILD)
    // These are unsupported and not fully tested modes, so don't enable them
    // for official Google Chrome builds.
    switches::kInProcessPlugins,
#endif  // GOOGLE_CHROME_BUILD
    switches::kInProcessWebGL,
    switches::kJavaScriptFlags,
    switches::kLoggingLevel,
    switches::kHighLatencyAudio,
    switches::kNoJsRandomness,
    switches::kNoReferrers,
    switches::kNoSandbox,
    switches::kPlaybackMode,
    switches::kPpapiOutOfProcess,
    switches::kRecordMode,
    switches::kRegisterPepperPlugins,
    switches::kRemoteShellPort,
    switches::kRendererAssertTest,
#if !defined(OFFICIAL_BUILD)
    switches::kRendererCheckFalseTest,
#endif  // !defined(OFFICIAL_BUILD)
    switches::kRendererCrashTest,
    switches::kRendererStartupDialog,
    switches::kShowPaintRects,
    switches::kSimpleDataSource,
    switches::kTestSandbox,
    // This flag needs to be propagated to the renderer process for
    // --in-process-webgl.
    switches::kUseGL,
    switches::kUserAgent,
    switches::kV,
    switches::kVideoThreads,
    switches::kVModule,
    switches::kWebCoreLogChannels,
  };
  renderer_cmd->CopySwitchesFrom(browser_cmd, kSwitchNames,
                                 arraysize(kSwitchNames));

  // Disable databases in incognito mode.
  if (browser_context()->IsOffTheRecord() &&
      !browser_cmd.HasSwitch(switches::kDisableDatabases)) {
    renderer_cmd->AppendSwitch(switches::kDisableDatabases);
  }
}

base::ProcessHandle BrowserRenderProcessHost::GetHandle() {
  // child_process_launcher_ is null either because we're in single process
  // mode, we have done fast termination, or the process has crashed.
  if (run_renderer_in_process() || !child_process_launcher_.get())
    return base::Process::Current().handle();

  if (child_process_launcher_->IsStarting())
    return base::kNullProcessHandle;

  return child_process_launcher_->GetHandle();
}

bool BrowserRenderProcessHost::FastShutdownIfPossible() {
  if (run_renderer_in_process())
    return false;  // Single process mode can't do fast shutdown.

  if (!content::GetContentClient()->browser()->IsFastShutdownPossible())
    return false;

  if (!child_process_launcher_.get() ||
      child_process_launcher_->IsStarting() ||
      !GetHandle())
    return false;  // Render process hasn't started or is probably crashed.

  // Test if there's an unload listener.
  // NOTE: It's possible that an onunload listener may be installed
  // while we're shutting down, so there's a small race here.  Given that
  // the window is small, it's unlikely that the web page has much
  // state that will be lost by not calling its unload handlers properly.
  if (!sudden_termination_allowed())
    return false;

  child_process_launcher_.reset();
  ProcessDied(base::TERMINATION_STATUS_NORMAL_TERMINATION, 0, false);
  fast_shutdown_started_ = true;
  return true;
}

void BrowserRenderProcessHost::DumpHandles() {
#if defined(OS_WIN)
  Send(new ChildProcessMsg_DumpHandles());
  return;
#endif

  NOTIMPLEMENTED();
}

// This is a platform specific function for mapping a transport DIB given its id
TransportDIB* BrowserRenderProcessHost::MapTransportDIB(
    TransportDIB::Id dib_id) {
#if defined(OS_WIN)
  // On Windows we need to duplicate the handle from the remote process
  HANDLE section = chrome::GetSectionFromProcess(
      dib_id.handle, GetHandle(), false /* read write */);
  return TransportDIB::Map(section);
#elif defined(OS_MACOSX)
  // On OSX, the browser allocates all DIBs and keeps a file descriptor around
  // for each.
  return widget_helper_->MapTransportDIB(dib_id);
#elif defined(OS_POSIX)
  return TransportDIB::Map(dib_id.shmkey);
#endif  // defined(OS_POSIX)
}

TransportDIB* BrowserRenderProcessHost::GetTransportDIB(
    TransportDIB::Id dib_id) {
  if (!TransportDIB::is_valid_id(dib_id))
    return NULL;

  const std::map<TransportDIB::Id, TransportDIB*>::iterator
      i = cached_dibs_.find(dib_id);
  if (i != cached_dibs_.end()) {
    cached_dibs_cleaner_.Reset();
    return i->second;
  }

  TransportDIB* dib = MapTransportDIB(dib_id);
  if (!dib)
    return NULL;

  if (cached_dibs_.size() >= MAX_MAPPED_TRANSPORT_DIBS) {
    // Clean a single entry from the cache
    std::map<TransportDIB::Id, TransportDIB*>::iterator smallest_iterator;
    size_t smallest_size = std::numeric_limits<size_t>::max();

    for (std::map<TransportDIB::Id, TransportDIB*>::iterator
         i = cached_dibs_.begin(); i != cached_dibs_.end(); ++i) {
      if (i->second->size() <= smallest_size) {
        smallest_iterator = i;
        smallest_size = i->second->size();
      }
    }

    delete smallest_iterator->second;
    cached_dibs_.erase(smallest_iterator);
  }

  cached_dibs_[dib_id] = dib;
  cached_dibs_cleaner_.Reset();
  return dib;
}

void BrowserRenderProcessHost::ClearTransportDIBCache() {
  STLDeleteContainerPairSecondPointers(
      cached_dibs_.begin(), cached_dibs_.end());
  cached_dibs_.clear();
}

void BrowserRenderProcessHost::SetCompositingSurface(
    int render_widget_id,
    gfx::PluginWindowHandle compositing_surface) {
  widget_helper_->SetCompositingSurface(render_widget_id, compositing_surface);
}

bool BrowserRenderProcessHost::Send(IPC::Message* msg) {
  if (!channel_.get()) {
    if (!is_initialized_) {
      queued_messages_.push(msg);
      return true;
    } else {
      delete msg;
      return false;
    }
  }

  if (child_process_launcher_.get() && child_process_launcher_->IsStarting()) {
    queued_messages_.push(msg);
    return true;
  }

  return channel_->Send(msg);
}

bool BrowserRenderProcessHost::OnMessageReceived(const IPC::Message& msg) {
  // If we're about to be deleted, or have initiated the fast shutdown sequence,
  // we ignore incoming messages.

  if (deleting_soon_ || fast_shutdown_started_)
    return false;

  mark_child_process_activity_time();
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    // Dispatch control messages.
    bool msg_is_ok = true;
    IPC_BEGIN_MESSAGE_MAP_EX(BrowserRenderProcessHost, msg, msg_is_ok)
      IPC_MESSAGE_HANDLER(ChildProcessHostMsg_ShutdownRequest,
                          OnShutdownRequest)
      IPC_MESSAGE_HANDLER(ChildProcessHostMsg_DumpHandlesDone,
                          OnDumpHandlesDone)
      IPC_MESSAGE_HANDLER(ViewHostMsg_SuddenTerminationChanged,
                          SuddenTerminationChanged)
      IPC_MESSAGE_HANDLER(ViewHostMsg_UserMetricsRecordAction,
                          OnUserMetricsRecordAction)
      IPC_MESSAGE_HANDLER(ViewHostMsg_RevealFolderInOS, OnRevealFolderInOS)
      IPC_MESSAGE_HANDLER(ViewHostMsg_SavedPageAsMHTML, OnSavedPageAsMHTML)
      IPC_MESSAGE_UNHANDLED_ERROR()
    IPC_END_MESSAGE_MAP_EX()

    if (!msg_is_ok) {
      // The message had a handler, but its de-serialization failed.
      // We consider this a capital crime. Kill the renderer if we have one.
      LOG(ERROR) << "bad message " << msg.type() << " terminating renderer.";
      UserMetrics::RecordAction(UserMetricsAction("BadMessageTerminate_BRPH"));
      ReceivedBadMessage();
    }
    return true;
  }

  // Dispatch incoming messages to the appropriate RenderView/WidgetHost.
  IPC::Channel::Listener* listener = GetListenerByID(msg.routing_id());
  if (!listener) {
    if (msg.is_sync()) {
      // The listener has gone away, so we must respond or else the caller will
      // hang waiting for a reply.
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
      reply->set_reply_error();
      Send(reply);
    }
    return true;
  }
  return listener->OnMessageReceived(msg);
}

void BrowserRenderProcessHost::OnChannelConnected(int32 peer_pid) {
#if defined(IPC_MESSAGE_LOG_ENABLED)
  Send(new ChildProcessMsg_SetIPCLoggingEnabled(
      IPC::Logging::GetInstance()->Enabled()));
#endif

  // Make sure the child checks with us before exiting, so that we do not try
  // to schedule a new navigation in a swapped out and exiting renderer.
  Send(new ChildProcessMsg_AskBeforeShutdown());
}

void BrowserRenderProcessHost::OnChannelError() {
  if (!channel_.get())
    return;

  // child_process_launcher_ can be NULL in single process mode or if fast
  // termination happened.
  int exit_code = 0;
  base::TerminationStatus status =
      child_process_launcher_.get() ?
      child_process_launcher_->GetChildTerminationStatus(&exit_code) :
      base::TERMINATION_STATUS_NORMAL_TERMINATION;

#if defined(OS_WIN)
  if (!run_renderer_in_process()) {
    if (status == base::TERMINATION_STATUS_STILL_RUNNING) {
      HANDLE process = child_process_launcher_->GetHandle();
      child_process_watcher_.StartWatching(
          new base::WaitableEvent(process), this);
      return;
    }
  }
#endif
  ProcessDied(status, exit_code, false);
}

// Called when the renderer process handle has been signaled.
void BrowserRenderProcessHost::OnWaitableEventSignaled(
    base::WaitableEvent* waitable_event) {
#if defined (OS_WIN)
  int exit_code = 0;
  base::TerminationStatus status =
      base::GetTerminationStatus(waitable_event->Release(), &exit_code);
  delete waitable_event;
  ProcessDied(status, exit_code, true);
#endif
}

void BrowserRenderProcessHost::ProcessDied(
    base::TerminationStatus status, int exit_code, bool was_alive) {
  // Our child process has died.  If we didn't expect it, it's a crash.
  // In any case, we need to let everyone know it's gone.
  // The OnChannelError notification can fire multiple times due to nested sync
  // calls to a renderer. If we don't have a valid channel here it means we
  // already handled the error.

  RendererClosedDetails details(status, exit_code, was_alive);
  NotificationService::current()->Notify(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      Source<RenderProcessHost>(this),
      Details<RendererClosedDetails>(&details));

  child_process_launcher_.reset();
  channel_.reset();

  IDMap<IPC::Channel::Listener>::iterator iter(&listeners_);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->OnMessageReceived(
        ViewHostMsg_RenderViewGone(iter.GetCurrentKey(),
                                   static_cast<int>(status),
                                   exit_code));
    iter.Advance();
  }

  ClearTransportDIBCache();

  // this object is not deleted at this point and may be reused later.
  // TODO(darin): clean this up
}

void BrowserRenderProcessHost::OnShutdownRequest() {
  // Don't shutdown if there are pending RenderViews being swapped back in.
  if (pending_views_)
    return;

  // Notify any tabs that might have swapped out renderers from this process.
  // They should not attempt to swap them back in.
  NotificationService::current()->Notify(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSING,
      Source<RenderProcessHost>(this), NotificationService::NoDetails());

  Send(new ChildProcessMsg_Shutdown());
}

void BrowserRenderProcessHost::OnDumpHandlesDone() {
  Cleanup();
}

void BrowserRenderProcessHost::SuddenTerminationChanged(bool enabled) {
  set_sudden_termination_allowed(enabled);
}

void BrowserRenderProcessHost::SetBackgrounded(bool backgrounded) {
  // Note: we always set the backgrounded_ value.  If the process is NULL
  // (and hence hasn't been created yet), we will set the process priority
  // later when we create the process.
  backgrounded_ = backgrounded;
  if (!child_process_launcher_.get() || child_process_launcher_->IsStarting())
    return;

#if defined(OS_WIN)
  // The cbstext.dll loads as a global GetMessage hook in the browser process
  // and intercepts/unintercepts the kernel32 API SetPriorityClass in a
  // background thread. If the UI thread invokes this API just when it is
  // intercepted the stack is messed up on return from the interceptor
  // which causes random crashes in the browser process. Our hack for now
  // is to not invoke the SetPriorityClass API if the dll is loaded.
  if (GetModuleHandle(L"cbstext.dll"))
    return;
#endif  // OS_WIN

  child_process_launcher_->SetProcessBackgrounded(backgrounded);
}

void BrowserRenderProcessHost::OnProcessLaunched() {
  // No point doing anything, since this object will be destructed soon.  We
  // especially don't want to send the RENDERER_PROCESS_CREATED notification,
  // since some clients might expect a RENDERER_PROCESS_TERMINATED afterwards to
  // properly cleanup.
  if (deleting_soon_)
    return;

  if (child_process_launcher_.get()) {
    if (!child_process_launcher_->GetHandle()) {
      OnChannelError();
      return;
    }

    child_process_launcher_->SetProcessBackgrounded(backgrounded_);
  }

  if (max_page_id_ != -1)
    Send(new ViewMsg_SetNextPageID(max_page_id_ + 1));

  // NOTE: This needs to be before sending queued messages because
  // ExtensionService uses this notification to initialize the renderer process
  // with state that must be there before any JavaScript executes.
  //
  // The queued messages contain such things as "navigate". If this notification
  // was after, we can end up executing JavaScript before the initialization
  // happens.
  NotificationService::current()->Notify(
      content::NOTIFICATION_RENDERER_PROCESS_CREATED,
      Source<RenderProcessHost>(this), NotificationService::NoDetails());

  while (!queued_messages_.empty()) {
    Send(queued_messages_.front());
    queued_messages_.pop();
  }
}

void BrowserRenderProcessHost::OnUserMetricsRecordAction(
    const std::string& action) {
  UserMetrics::RecordComputedAction(action);
}

void BrowserRenderProcessHost::OnRevealFolderInOS(const FilePath& path) {
  // Only honor the request if appropriate persmissions are granted.
  if (ChildProcessSecurityPolicy::GetInstance()->CanReadFile(id(), path))
    content::GetContentClient()->browser()->OpenItem(path);
}

void BrowserRenderProcessHost::OnSavedPageAsMHTML(int job_id, int64 data_size) {
  content::GetContentClient()->browser()->GetMHTMLGenerationManager()->
      MHTMLGenerated(job_id, data_size);
}
