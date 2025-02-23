// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_CHILD_PROCESS_HOST_H_
#define CONTENT_BROWSER_BROWSER_CHILD_PROCESS_HOST_H_
#pragma once

#include <list>

#include "base/synchronization/waitable_event_watcher.h"
#include "content/browser/child_process_launcher.h"
#include "content/common/child_process_host.h"
#include "content/common/child_process_info.h"
#include "content/common/content_export.h"

namespace base {
class WaitableEvent;
}

// Plugins/workers and other child processes that live on the IO thread should
// derive from this class.
//
// [Browser]RenderProcessHost is the main exception that doesn't derive from
// this class. That project lives on the UI thread.
class CONTENT_EXPORT BrowserChildProcessHost :
    public ChildProcessHost,
    public ChildProcessInfo,
    public ChildProcessLauncher::Client,
    public base::WaitableEventWatcher::Delegate {
 public:
  virtual ~BrowserChildProcessHost();

  virtual void OnWaitableEventSignaled(
      base::WaitableEvent* waitable_event) OVERRIDE;

  // Terminates all child processes and deletes each ChildProcessHost instance.
  static void TerminateAll();

  // The Iterator class allows iteration through either all child processes, or
  // ones of a specific type, depending on which constructor is used.  Note that
  // this should be done from the IO thread and that the iterator should not be
  // kept around as it may be invalidated on subsequent event processing in the
  // event loop.
  class CONTENT_EXPORT Iterator {
   public:
    Iterator();
    explicit Iterator(ChildProcessInfo::ProcessType type);
    BrowserChildProcessHost* operator->() { return *iterator_; }
    BrowserChildProcessHost* operator*() { return *iterator_; }
    BrowserChildProcessHost* operator++();
    bool Done();

   private:
    bool all_;
    ChildProcessInfo::ProcessType type_;
    std::list<BrowserChildProcessHost*>::iterator iterator_;
  };

 protected:
  explicit BrowserChildProcessHost(ChildProcessInfo::ProcessType type);

  // Derived classes call this to launch the child process asynchronously.
  void Launch(
#if defined(OS_WIN)
      const FilePath& exposed_dir,
#elif defined(OS_POSIX)
      bool use_zygote,
      const base::environment_vector& environ,
#endif
      CommandLine* cmd_line);

  // Returns the handle of the child process. This can be called only after
  // OnProcessLaunched is called or it will be invalid and may crash.
  base::ProcessHandle GetChildProcessHandle() const;

  // ChildProcessLauncher::Client implementation.
  virtual void OnProcessLaunched() {}

  // Derived classes can override this to know if the process crashed.
  // |exit_code| is the status returned when the process crashed (for
  // posix, as returned from waitpid(), for Windows, as returned from
  // GetExitCodeProcess()).
  virtual void OnProcessCrashed(int exit_code) {}

  // Derived classes can override this to know if the process was
  // killed.  |exit_code| is the status returned when the process
  // was killed (for posix, as returned from waitpid(), for Windows,
  // as returned from GetExitCodeProcess()).
  virtual void OnProcessWasKilled(int exit_code) {}

  // Returns the termination status of a child.  |exit_code| is the
  // status returned when the process exited (for posix, as returned
  // from waitpid(), for Windows, as returned from
  // GetExitCodeProcess()).  |exit_code| may be NULL.
  virtual base::TerminationStatus GetChildTerminationStatus(int* exit_code);

  // Overrides from ChildProcessHost
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChildDisconnected() OVERRIDE;
  virtual void ShutdownStarted() OVERRIDE;
  // Extends the base class implementation and removes this host from
  // the host list. Calls ChildProcessHost::ForceShutdown
  virtual void ForceShutdown() OVERRIDE;

  // Controls whether the child process should be terminated on browser
  // shutdown. Default is to always terminate.
  void SetTerminateChildOnShutdown(bool terminate_on_shutdown);

  // Sends the given notification on the UI thread.
  void Notify(int type);

 private:
  // By using an internal class as the ChildProcessLauncher::Client, we can
  // intercept OnProcessLaunched and do our own processing before
  // calling the subclass' implementation.
  class ClientHook : public ChildProcessLauncher::Client {
   public:
    explicit ClientHook(BrowserChildProcessHost* host);
    virtual void OnProcessLaunched();
   private:
    BrowserChildProcessHost* host_;
  };

  ClientHook client_;
  scoped_ptr<ChildProcessLauncher> child_process_;
#if defined(OS_WIN)
  base::WaitableEventWatcher child_watcher_;
#endif
  bool disconnect_was_alive_;
};

#endif  // CONTENT_BROWSER_BROWSER_CHILD_PROCESS_HOST_H_
