// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test validates that the ProcessSingleton class properly makes sure
// that there is only one main browser process.
//
// It is currently compiled and run on Windows and Posix(non-Mac) platforms.
// Mac uses system services and ProcessSingletonMac is a noop.  (Maybe it still
// makes sense to test that the system services are giving the behavior we
// want?)

#include <list>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_temp_dir.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/ui/ui_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This is for the code that is to be ran in multiple threads at once,
// to stress a race condition on first process start.
// We use the thread safe ref counted base class so that we can use the
// NewRunnableMethod class to run the StartChrome methods in many threads.
class ChromeStarter : public base::RefCountedThreadSafe<ChromeStarter> {
 public:
  ChromeStarter(int timeout_ms, const FilePath& user_data_dir)
      : ready_event_(false /* manual */, false /* signaled */),
        done_event_(false /* manual */, false /* signaled */),
        process_handle_(base::kNullProcessHandle),
        process_terminated_(false),
        timeout_ms_(timeout_ms),
        user_data_dir_(user_data_dir) {
  }

  // We must reset some data members since we reuse the same ChromeStarter
  // object and start/stop it a few times. We must start fresh! :-)
  void Reset() {
    ready_event_.Reset();
    done_event_.Reset();
    if (process_handle_ != base::kNullProcessHandle)
      base::CloseProcessHandle(process_handle_);
    process_handle_ = base::kNullProcessHandle;
    process_terminated_ = false;
  }

  void StartChrome(base::WaitableEvent* start_event, bool first_run) {
    // TODO(mattm): maybe stuff should be refactored to use
    // UITest::LaunchBrowserHelper somehow?
    FilePath browser_directory;
    PathService::Get(chrome::DIR_APP, &browser_directory);
    CommandLine command_line(browser_directory.Append(
        chrome::kBrowserProcessExecutablePath));

    command_line.AppendSwitchPath(switches::kUserDataDir, user_data_dir_);

    if (first_run)
      command_line.AppendSwitch(switches::kFirstRun);
    else
      command_line.AppendSwitch(switches::kNoFirstRun);

    // Add the normal test-mode switches, except for the ones we're adding
    // ourselves.
    CommandLine standard_switches(CommandLine::NO_PROGRAM);
    test_launcher_utils::PrepareBrowserCommandLineForTests(&standard_switches);
    const CommandLine::SwitchMap& switch_map = standard_switches.GetSwitches();
    for (CommandLine::SwitchMap::const_iterator i = switch_map.begin();
         i != switch_map.end(); ++i) {
      const std::string& switch_name = i->first;
      if (switch_name == switches::kUserDataDir ||
          switch_name == switches::kFirstRun ||
          switch_name == switches::kNoFirstRun)
        continue;

      command_line.AppendSwitchNative(switch_name, i->second);
    }

    // Try to get all threads to launch the app at the same time.
    // So let the test know we are ready.
    ready_event_.Signal();
    // And then wait for the test to tell us to GO!
    ASSERT_NE(static_cast<base::WaitableEvent*>(NULL), start_event);
    start_event->Wait();

    // Here we don't wait for the app to be terminated because one of the
    // process will stay alive while the others will be restarted. If we would
    // wait here, we would never get a handle to the main process...
    base::LaunchProcess(command_line, base::LaunchOptions(), &process_handle_);
    ASSERT_NE(base::kNullProcessHandle, process_handle_);

    // We can wait on the handle here, we should get stuck on one and only
    // one process. The test below will take care of killing that process
    // to unstuck us once it confirms there is only one.
    process_terminated_ = base::WaitForSingleProcess(process_handle_,
                                                     timeout_ms_);
    // Let the test know we are done.
    done_event_.Signal();
  }

  // Public access to simplify the test code using them.
  base::WaitableEvent ready_event_;
  base::WaitableEvent done_event_;
  base::ProcessHandle process_handle_;
  bool process_terminated_;

 private:
  friend class base::RefCountedThreadSafe<ChromeStarter>;

  ~ChromeStarter() {
    if (process_handle_ != base::kNullProcessHandle)
      base::CloseProcessHandle(process_handle_);
  }

  int timeout_ms_;
  FilePath user_data_dir_;

  DISALLOW_COPY_AND_ASSIGN(ChromeStarter);
};

// Our test fixture that initializes and holds onto a few global vars.
class ProcessSingletonTest : public UITest {
 public:
  ProcessSingletonTest()
      // We use a manual reset so that all threads wake up at once when signaled
      // and thus we must manually reset it for each attempt.
      : threads_waker_(true /* manual */, false /* signaled */) {
    EXPECT_TRUE(temp_profile_dir_.CreateUniqueTempDir());
  }

  void SetUp() {
    // Start the threads and create the starters.
    for (size_t i = 0; i < kNbThreads; ++i) {
      chrome_starter_threads_[i].reset(new base::Thread("ChromeStarter"));
      ASSERT_TRUE(chrome_starter_threads_[i]->Start());
      chrome_starters_[i] = new ChromeStarter(
          TestTimeouts::action_max_timeout_ms(), temp_profile_dir_.path());
    }
  }

  void TearDown() {
    // Stop the threads.
    for (size_t i = 0; i < kNbThreads; ++i)
      chrome_starter_threads_[i]->Stop();
  }

  // This method is used to make sure we kill the main browser process after
  // all of its child processes have successfully attached to it. This was added
  // when we realized that if we just kill the parent process right away, we
  // sometimes end up with dangling child processes. If we Sleep for a certain
  // amount of time, we are OK... So we introduced this method to avoid a
  // flaky wait. Instead, we kill all descendants of the main process after we
  // killed it, relying on the fact that we can still get the parent id of a
  // child process, even when the parent dies.
  void KillProcessTree(base::ProcessHandle process_handle) {
    class ProcessTreeFilter : public base::ProcessFilter {
     public:
      explicit ProcessTreeFilter(base::ProcessId parent_pid) {
        ancestor_pids_.insert(parent_pid);
      }
      virtual bool Includes(const base::ProcessEntry & entry) const {
        if (ancestor_pids_.find(entry.parent_pid()) != ancestor_pids_.end()) {
          ancestor_pids_.insert(entry.pid());
          return true;
        } else {
          return false;
        }
      }
     private:
      mutable std::set<base::ProcessId> ancestor_pids_;
    } process_tree_filter(base::GetProcId(process_handle));

    // Start by explicitly killing the main process we know about...
    static const int kExitCode = 42;
    EXPECT_TRUE(base::KillProcess(process_handle, kExitCode, true /* wait */));

    // Then loop until we can't find any of its descendant.
    // But don't try more than kNbTries times...
    static const int kNbTries = 10;
    int num_tries = 0;
    while (base::GetProcessCount(chrome::kBrowserProcessExecutablePath,
        &process_tree_filter) > 0 && num_tries++ < kNbTries) {
      base::KillProcesses(chrome::kBrowserProcessExecutablePath,
                          kExitCode, &process_tree_filter);
    }
    DLOG_IF(ERROR, num_tries >= kNbTries) << "Failed to kill all processes!";
  }

  // Since this is a hard to reproduce problem, we make a few attempts.
  // We stop the attempts at the first error, and when there are no errors,
  // we don't time-out of any wait, so it executes quite fast anyway.
  static const size_t kNbAttempts = 5;

  // The idea is to start chrome from multiple threads all at once.
  static const size_t kNbThreads = 5;
  scoped_refptr<ChromeStarter> chrome_starters_[kNbThreads];
  scoped_ptr<base::Thread> chrome_starter_threads_[kNbThreads];

  // The event that will get all threads to wake up simultaneously and try
  // to start a chrome process at the same time.
  base::WaitableEvent threads_waker_;

  // We don't want to use the default profile, but can't use UITest's since we
  // don't use UITest::LaunchBrowser.
  ScopedTempDir temp_profile_dir_;
};

#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
// http://crbug.com/58219
#define MAYBE_StartupRaceCondition FAILS_StartupRaceCondition
#else
#define MAYBE_StartupRaceCondition StartupRaceCondition
#endif
TEST_F(ProcessSingletonTest, MAYBE_StartupRaceCondition) {
  // We use this to stop the attempts loop on the first failure.
  bool failed = false;
  for (size_t attempt = 0; attempt < kNbAttempts && !failed; ++attempt) {
    SCOPED_TRACE(testing::Message() << "Attempt: " << attempt << ".");
    // We use a single event to get all threads to do the AppLaunch at the same
    // time...
    threads_waker_.Reset();

    // Test both with and without the first-run dialog, since they exercise
    // different paths.
#if defined(OS_POSIX)
    // TODO(mattm): test first run dialog singleton handling on linux too.
    // On posix if we test the first run dialog, GracefulShutdownHandler gets
    // the TERM signal, but since the message loop isn't running during the gtk
    // first run dialog, the ShutdownDetector never handles it, and KillProcess
    // has to time out (60 sec!) and SIGKILL.
    bool first_run = false;
#else
    // Test for races in both regular start up and first run start up cases.
    bool first_run = attempt % 2;
#endif

    // Here we prime all the threads with a ChromeStarter that will wait for
    // our signal to launch its chrome process.
    for (size_t i = 0; i < kNbThreads; ++i) {
      ASSERT_NE(static_cast<ChromeStarter*>(NULL), chrome_starters_[i].get());
      chrome_starters_[i]->Reset();

      ASSERT_TRUE(chrome_starter_threads_[i]->IsRunning());
      ASSERT_NE(static_cast<MessageLoop*>(NULL),
                chrome_starter_threads_[i]->message_loop());

      chrome_starter_threads_[i]->message_loop()->PostTask(
          FROM_HERE, NewRunnableMethod(chrome_starters_[i].get(),
                                       &ChromeStarter::StartChrome,
                                       &threads_waker_,
                                       first_run));
    }

    // Wait for all the starters to be ready.
    // We could replace this loop if we ever implement a WaitAll().
    for (size_t i = 0; i < kNbThreads; ++i) {
      SCOPED_TRACE(testing::Message() << "Waiting on thread: " << i << ".");
      chrome_starters_[i]->ready_event_.Wait();
    }
    // GO!
    threads_waker_.Signal();

    // As we wait for all threads to signal that they are done, we remove their
    // index from this vector so that we get left with only the index of
    // the thread that started the main process.
    std::vector<size_t> pending_starters(kNbThreads);
    for (size_t i = 0; i < kNbThreads; ++i)
      pending_starters[i] = i;

    // We use a local array of starter's done events we must wait on...
    // These are collected from the starters that we have not yet been removed
    // from the pending_starters vector.
    base::WaitableEvent* starters_done_events[kNbThreads];
    // At the end, "There can be only one" main browser process alive.
    while (pending_starters.size() > 1) {
      SCOPED_TRACE(testing::Message() << pending_starters.size() <<
                   " starters left.");
      for (size_t i = 0; i < pending_starters.size(); ++i) {
        starters_done_events[i] =
            &chrome_starters_[pending_starters[i]]->done_event_;
      }
      size_t done_index = base::WaitableEvent::WaitMany(
          starters_done_events, pending_starters.size());
      size_t starter_index = pending_starters[done_index];
      // If the starter is done but has not marked itself as terminated,
      // it is because it timed out of its WaitForSingleProcess(). Only the
      // last one standing should be left waiting... So we failed...
      EXPECT_TRUE(chrome_starters_[starter_index]->process_terminated_ ||
                  failed) << "There is more than one main process.";
      if (!chrome_starters_[starter_index]->process_terminated_) {
        // This will stop the "for kNbAttempts" loop.
        failed = true;
        // But we let the last loop turn finish so that we can properly
        // kill all remaining processes. Starting with this one...
        if (chrome_starters_[starter_index]->process_handle_ !=
            base::kNullProcessHandle) {
          KillProcessTree(chrome_starters_[starter_index]->process_handle_);
        }
      }
      pending_starters.erase(pending_starters.begin() + done_index);
    }

    // "There can be only one!" :-)
    ASSERT_EQ(static_cast<size_t>(1), pending_starters.size());
    size_t last_index = pending_starters.front();
    pending_starters.empty();
    if (chrome_starters_[last_index]->process_handle_ !=
        base::kNullProcessHandle) {
      KillProcessTree(chrome_starters_[last_index]->process_handle_);
      chrome_starters_[last_index]->done_event_.Wait();
    }
  }
}

}  // namespace
