// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_process_host.h"

#include "base/command_line.h"
#include "base/rand_util.h"
#include "base/sys_info.h"
#include "content/browser/browser_main.h"
#include "content/browser/browser_thread.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/content_browser_client.h"
#include "content/browser/webui/web_ui_factory.h"
#include "content/common/child_process_info.h"
#include "content/common/content_client.h"
#include "content/common/content_constants.h"
#include "content/common/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"

namespace {

size_t max_renderer_count_override = 0;

size_t GetMaxRendererProcessCount() {
  if (max_renderer_count_override)
    return max_renderer_count_override;

  // Defines the maximum number of renderer processes according to the
  // amount of installed memory as reported by the OS. The table
  // values are calculated by assuming that you want the renderers to
  // use half of the installed ram and assuming that each tab uses
  // ~40MB, however the curve is not linear but piecewise linear with
  // interleaved slopes of 3 and 2.
  // If you modify this table you need to adjust browser\browser_uitest.cc
  // to match the expected number of processes.

  static const size_t kMaxRenderersByRamTier[] = {
    3,                        // less than 256MB
    6,                        //  256MB
    9,                        //  512MB
    12,                       //  768MB
    14,                       // 1024MB
    18,                       // 1280MB
    20,                       // 1536MB
    22,                       // 1792MB
    24,                       // 2048MB
    26,                       // 2304MB
    29,                       // 2560MB
    32,                       // 2816MB
    35,                       // 3072MB
    38,                       // 3328MB
    40                        // 3584MB
  };

  static size_t max_count = 0;
  if (!max_count) {
    size_t memory_tier = base::SysInfo::AmountOfPhysicalMemoryMB() / 256;
    if (memory_tier >= arraysize(kMaxRenderersByRamTier))
      max_count = content::kMaxRendererProcessCount;
    else
      max_count = kMaxRenderersByRamTier[memory_tier];
  }
  return max_count;
}

// Returns true if the given host is suitable for launching a new view
// associated with the given browser context.
static bool IsSuitableHost(RenderProcessHost* host,
                           content::BrowserContext* browser_context,
                           const GURL& site_url) {
  if (host->browser_context() != browser_context)
    return false;

  if (ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(host->id()) !=
      content::WebUIFactory::Get()->HasWebUIScheme(site_url))
    return false;

  return content::GetContentClient()->browser()->IsSuitableHost(host, site_url);
}

// the global list of all renderer processes
IDMap<RenderProcessHost> all_hosts;

}  // namespace

// static
bool RenderProcessHost::run_renderer_in_process_ = false;

// static
void RenderProcessHost::SetMaxRendererProcessCountForTest(size_t count) {
  max_renderer_count_override = count;
}

RenderProcessHost::RenderProcessHost(content::BrowserContext* browser_context)
    : max_page_id_(-1),
      fast_shutdown_started_(false),
      deleting_soon_(false),
      pending_views_(0),
      id_(ChildProcessInfo::GenerateChildProcessUniqueId()),
      browser_context_(browser_context),
      sudden_termination_allowed_(true),
      ignore_input_events_(false) {
  CHECK(!content::ExitedMainMessageLoop());
  all_hosts.AddWithID(this, id());
  all_hosts.set_check_on_null_data(true);
  // Initialize |child_process_activity_time_| to a reasonable value.
  mark_child_process_activity_time();
}

RenderProcessHost::~RenderProcessHost() {
  // In unit tests, Release() might not have been called.
  if (all_hosts.Lookup(id()))
    all_hosts.Remove(id());
}

bool RenderProcessHost::HasConnection() const {
  return channel_.get() != NULL;
}

void RenderProcessHost::Attach(IPC::Channel::Listener* listener,
                               int routing_id) {
  listeners_.AddWithID(listener, routing_id);
}

void RenderProcessHost::Release(int listener_id) {
  DCHECK(listeners_.Lookup(listener_id) != NULL);
  listeners_.Remove(listener_id);

  // Make sure that all associated resource requests are stopped.
  CancelResourceRequests(listener_id);

#if defined(OS_WIN)
  // Dump the handle table if handle auditing is enabled.
  const CommandLine& browser_command_line =
      *CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(switches::kAuditHandles) ||
      browser_command_line.HasSwitch(switches::kAuditAllHandles)) {
    DumpHandles();

    // We wait to close the channels until the child process has finished
    // dumping handles and sends us ChildProcessHostMsg_DumpHandlesDone.
    return;
  }
#endif
  Cleanup();
}

void RenderProcessHost::Cleanup() {
  // When no other owners of this object, we can delete ourselves
  if (listeners_.IsEmpty()) {
    NotificationService::current()->Notify(
        content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
        Source<RenderProcessHost>(this), NotificationService::NoDetails());
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
    deleting_soon_ = true;
    // It's important not to wait for the DeleteTask to delete the channel
    // proxy. Kill it off now. That way, in case the profile is going away, the
    // rest of the objects attached to this RenderProcessHost start going
    // away first, since deleting the channel proxy will post a
    // OnChannelClosed() to IPC::ChannelProxy::Context on the IO thread.
    channel_.reset();

    // Remove ourself from the list of renderer processes so that we can't be
    // reused in between now and when the Delete task runs.
    all_hosts.Remove(id());
  }
}

void RenderProcessHost::ReportExpectingClose(int32 listener_id) {
  listeners_expecting_close_.insert(listener_id);
}

void RenderProcessHost::AddPendingView() {
  pending_views_++;
}

void RenderProcessHost::RemovePendingView() {
  DCHECK(pending_views_);
  pending_views_--;
}

void RenderProcessHost::UpdateMaxPageID(int32 page_id) {
  if (page_id > max_page_id_)
    max_page_id_ = page_id;
}

bool RenderProcessHost::FastShutdownForPageCount(size_t count) {
  if (listeners_.size() == count)
    return FastShutdownIfPossible();
  return false;
}

// static
RenderProcessHost::iterator RenderProcessHost::AllHostsIterator() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return iterator(&all_hosts);
}

// static
RenderProcessHost* RenderProcessHost::FromID(int render_process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return all_hosts.Lookup(render_process_id);
}

// static
bool RenderProcessHost::ShouldTryToUseExistingProcessHost() {
  size_t renderer_process_count = all_hosts.size();

  // NOTE: Sometimes it's necessary to create more render processes than
  //       GetMaxRendererProcessCount(), for instance when we want to create
  //       a renderer process for a browser context that has no existing
  //       renderers. This is OK in moderation, since the
  //       GetMaxRendererProcessCount() is conservative.

  return run_renderer_in_process() ||
         (renderer_process_count >= GetMaxRendererProcessCount());
}

// static
RenderProcessHost* RenderProcessHost::GetExistingProcessHost(
    content::BrowserContext* browser_context,
    const GURL& site_url) {
  // First figure out which existing renderers we can use.
  std::vector<RenderProcessHost*> suitable_renderers;
  suitable_renderers.reserve(all_hosts.size());

  iterator iter(AllHostsIterator());
  while (!iter.IsAtEnd()) {
    if (run_renderer_in_process() ||
        IsSuitableHost(iter.GetCurrentValue(), browser_context, site_url))
      suitable_renderers.push_back(iter.GetCurrentValue());

    iter.Advance();
  }

  // Now pick a random suitable renderer, if we have any.
  if (!suitable_renderers.empty()) {
    int suitable_count = static_cast<int>(suitable_renderers.size());
    int random_index = base::RandInt(0, suitable_count - 1);
    return suitable_renderers[random_index];
  }

  return NULL;
}
