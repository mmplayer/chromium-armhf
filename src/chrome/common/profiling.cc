// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiling.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/profiler.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_switches.h"

namespace {
std::string GetProfileName() {
  static const char kDefaultProfileName[] = "chrome-profile-{type}-{pid}";
  static std::string profile_name;

  if (profile_name.empty()) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch(switches::kProfilingFile))
      profile_name = command_line.GetSwitchValueASCII(switches::kProfilingFile);
    else
      profile_name = std::string(kDefaultProfileName);
    std::string process_type =
        command_line.GetSwitchValueASCII(switches::kProcessType);
    std::string type = process_type.empty() ?
        std::string("browser") : std::string(process_type);
    ReplaceSubstringsAfterOffset(&profile_name, 0, "{type}", type.c_str());
  }
  return profile_name;
}

void FlushProfilingData(base::Thread* thread) {
  static const int kProfilingFlushSeconds = 10;

  if (!Profiling::BeingProfiled())
    return;

  base::debug::FlushProfiling();
  static int flush_seconds = 0;
  if (!flush_seconds) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    std::string profiling_flush =
        command_line.GetSwitchValueASCII(switches::kProfilingFlush);
    if (!profiling_flush.empty()) {
      flush_seconds = atoi(profiling_flush.c_str());
      DCHECK(flush_seconds > 0);
    } else {
      flush_seconds = kProfilingFlushSeconds;
    }
  }
  thread->message_loop()->PostDelayedTask(
      FROM_HERE, base::Bind(FlushProfilingData, thread), flush_seconds * 1000);
}

class ProfilingThreadControl {
 public:
  ProfilingThreadControl() {}

  void Start() {
    base::AutoLock locked(lock_);

    if (thread_ && thread_->IsRunning())
      return;
    thread_ = new base::Thread("Profiling_Flush");
    thread_->Start();
    thread_->message_loop()->PostTask(
        FROM_HERE, base::Bind(FlushProfilingData, thread_));
  }

  void Stop() {
    base::AutoLock locked(lock_);

    if (!thread_ || !thread_->IsRunning())
      return;
    thread_->Stop();
    delete thread_;
    thread_ = NULL;
  }

 private:
  base::Thread* thread_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(ProfilingThreadControl);
};

base::LazyInstance<ProfilingThreadControl,
                   base::LeakyLazyInstanceTraits<ProfilingThreadControl> >
    g_flush_thread_control(base::LINKER_INITIALIZED);

} // namespace

// static
void Profiling::ProcessStarted() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  if (command_line.HasSwitch(switches::kProfilingAtStart)) {
    std::string process_type_to_start =
        command_line.GetSwitchValueASCII(switches::kProfilingAtStart);
    if (process_type == process_type_to_start)
      Start();
  }
}

// static
void Profiling::Start() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool flush = command_line.HasSwitch(switches::kProfilingFlush);
  base::debug::StartProfiling(GetProfileName());

  // Schedule profile data flushing for single process because it doesn't
  // get written out correctly on exit.
  if (flush)
    g_flush_thread_control.Get().Start();
}

// static
void Profiling::Stop() {
  g_flush_thread_control.Get().Stop();
  base::debug::StopProfiling();
}

// static
bool Profiling::BeingProfiled() {
  return base::debug::BeingProfiled();
}

// static
void Profiling::Toggle() {
  if (BeingProfiled())
    Stop();
  else
    Start();
}
