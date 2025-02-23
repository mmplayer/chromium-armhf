// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/relauncher.h"

#include <ApplicationServices/ApplicationServices.h>
#include <AvailabilityMacros.h>
#include <dlfcn.h>
#include <string.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/eintr_wrapper.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/mac/install_from_dmg.h"
#include "chrome/common/chrome_switches.h"
#include "content/common/content_paths.h"
#include "content/common/main_function_params.h"
#include "content/public/common/content_switches.h"

// RTLD_MAIN_ONLY is supported as of Mac OS X 10.5, but <dlfcn.h> does not
// define it in the 10.5 SDK. It is present in the 10.6 SDK and is documented
// as working on 10.5 and later. The source code for the version of dyld that
// shipped in 10.5, dyld-95.3/src/dyldAPIs.cpp, confirms that this feature is
// supported. Provide a fallback definition here.
#if MAC_OS_X_VERSION_MAX_ALLOWED == MAC_OS_X_VERSION_10_5  // 10.5 SDK
#define RTLD_MAIN_ONLY ((void*)-5)  // Search main executable only.
#endif

namespace mac_relauncher {

const char* const kRelauncherDMGDeviceArg = "--dmg-device=";

namespace {

// The "magic" file descriptor that the relauncher process' write side of the
// pipe shows up on. Chosen to avoid conflicting with stdin, stdout, and
// stderr.
const int kRelauncherSyncFD = STDERR_FILENO + 1;

// The argument separating arguments intended for the relauncher process from
// those intended for the relaunched process. "---" is chosen instead of "--"
// because CommandLine interprets "--" as meaning "end of switches", but
// for many purposes, the relauncher process' CommandLine ought to interpret
// arguments intended for the relaunched process, to get the correct settings
// for such things as logging and the user-data-dir in case it affects crash
// reporting.
const char kRelauncherArgSeparator[] = "---";

// When this argument is supplied to the relauncher process, it will launch
// the relaunched process without bringing it to the foreground.
const char kRelauncherBackgroundArg[] = "--background";

// The beginning of the "process serial number" argument that Launch Services
// sometimes inserts into command lines. A process serial number is only valid
// for a single process, so any PSN arguments will be stripped from command
// lines during relaunch to avoid confusion.
const char kPSNArg[] = "-psn_";

// Returns the "type" argument identifying a relauncher process
// ("--type=relauncher").
std::string RelauncherTypeArg() {
  return base::StringPrintf("--%s=%s",
                            switches::kProcessType,
                            switches::kRelauncherProcess);
}

}  // namespace

bool RelaunchApp(const std::vector<std::string>& args) {
  // Use the currently-running application's helper process. The automatic
  // update feature is careful to leave the currently-running version alone,
  // so this is safe even if the relaunch is the result of an update having
  // been applied. In fact, it's safer than using the updated version of the
  // helper process, because there's no guarantee that the updated version's
  // relauncher implementation will be compatible with the running version's.
  FilePath child_path;
  if (!PathService::Get(content::CHILD_PROCESS_EXE, &child_path)) {
    LOG(ERROR) << "No CHILD_PROCESS_EXE";
    return false;
  }

  std::vector<std::string> relauncher_args;
  return RelaunchAppWithHelper(child_path.value(), relauncher_args, args);
}

bool RelaunchAppWithHelper(const std::string& helper,
                           const std::vector<std::string>& relauncher_args,
                           const std::vector<std::string>& args) {
  std::vector<std::string> relaunch_args;
  relaunch_args.push_back(helper);
  relaunch_args.push_back(RelauncherTypeArg());

  // If this application isn't in the foreground, the relaunched one shouldn't
  // be either.
  if (!base::mac::AmIForeground()) {
    relaunch_args.push_back(kRelauncherBackgroundArg);
  }

  relaunch_args.insert(relaunch_args.end(),
                       relauncher_args.begin(), relauncher_args.end());

  relaunch_args.push_back(kRelauncherArgSeparator);

  // When using the CommandLine interface, -psn_ may have been rewritten as
  // --psn_. Look for both.
  const char alt_psn_arg[] = "--psn_";
  for (size_t index = 0; index < args.size(); ++index) {
    // Strip any -psn_ arguments, as they apply to a specific process.
    if (args[index].compare(0, strlen(kPSNArg), kPSNArg) != 0 &&
        args[index].compare(0, strlen(alt_psn_arg), alt_psn_arg) != 0) {
      relaunch_args.push_back(args[index]);
    }
  }

  int pipe_fds[2];
  if (HANDLE_EINTR(pipe(pipe_fds)) != 0) {
    PLOG(ERROR) << "pipe";
    return false;
  }

  // The parent process will only use pipe_read_fd as the read side of the
  // pipe. It can close the write side as soon as the relauncher process has
  // forked off. The relauncher process will only use pipe_write_fd as the
  // write side of the pipe. In that process, the read side will be closed by
  // base::LaunchApp because it won't be present in fd_map, and the write side
  // will be remapped to kRelauncherSyncFD by fd_map.
  file_util::ScopedFD pipe_read_fd(&pipe_fds[0]);
  file_util::ScopedFD pipe_write_fd(&pipe_fds[1]);

  // Make sure kRelauncherSyncFD is a safe value. base::LaunchProcess will
  // preserve these three FDs in forked processes, so kRelauncherSyncFD should
  // not conflict with them.
  COMPILE_ASSERT(kRelauncherSyncFD != STDIN_FILENO &&
                 kRelauncherSyncFD != STDOUT_FILENO &&
                 kRelauncherSyncFD != STDERR_FILENO,
                 kRelauncherSyncFD_must_not_conflict_with_stdio_fds);

  base::file_handle_mapping_vector fd_map;
  fd_map.push_back(std::make_pair(*pipe_write_fd, kRelauncherSyncFD));

  base::LaunchOptions options;
  options.fds_to_remap = &fd_map;
  if (!base::LaunchProcess(relaunch_args, options, NULL)) {
    LOG(ERROR) << "base::LaunchProcess failed";
    return false;
  }

  // The relauncher process is now starting up, or has started up. The
  // original parent process continues.

  pipe_write_fd.reset();  // close(pipe_fds[1]);

  // Synchronize with the relauncher process.
  char read_char;
  int read_result = HANDLE_EINTR(read(*pipe_read_fd, &read_char, 1));
  if (read_result != 1) {
    if (read_result < 0) {
      PLOG(ERROR) << "read";
    } else {
      LOG(ERROR) << "read: unexpected result " << read_result;
    }
    return false;
  }

  // Since a byte has been successfully read from the relauncher process, it's
  // guaranteed to have set up its kqueue monitoring this process for exit.
  // It's safe to exit now.
  return true;
}

namespace {

// In the relauncher process, performs the necessary synchronization steps
// with the parent by setting up a kqueue to watch for it to exit, writing a
// byte to the pipe, and then waiting for the exit notification on the kqueue.
// If anything fails, this logs a message and returns immediately. In those
// situations, it can be assumed that something went wrong with the parent
// process and the best recovery approach is to attempt relaunch anyway.
void RelauncherSynchronizeWithParent() {
  // file_util::ScopedFD needs something non-const to operate on.
  int relauncher_sync_fd = kRelauncherSyncFD;
  file_util::ScopedFD relauncher_sync_fd_closer(&relauncher_sync_fd);

  int parent_pid = getppid();

  // PID 1 identifies init. launchd, that is. launchd never starts the
  // relauncher process directly, having this parent_pid means that the parent
  // already exited and launchd "inherited" the relauncher as its child.
  // There's no reason to synchronize with launchd.
  if (parent_pid == 1) {
    LOG(ERROR) << "unexpected parent_pid";
    return;
  }

  // Set up a kqueue to monitor the parent process for exit.
  int kq = kqueue();
  if (kq < 0) {
    PLOG(ERROR) << "kqueue";
    return;
  }
  file_util::ScopedFD kq_closer(&kq);

  struct kevent change = { 0 };
  EV_SET(&change, parent_pid, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, NULL);
  if (kevent(kq, &change, 1, NULL, 0, NULL) == -1) {
    PLOG(ERROR) << "kevent (add)";
    return;
  }

  // Write a '\0' character to the pipe.
  if (HANDLE_EINTR(write(relauncher_sync_fd, "", 1)) != 1) {
    PLOG(ERROR) << "write";
    return;
  }

  // Up until now, the parent process was blocked in a read waiting for the
  // write above to complete. The parent process is now free to exit. Wait for
  // that to happen.
  struct kevent event;
  int events = kevent(kq, NULL, 0, &event, 1, NULL);
  if (events != 1) {
    if (events < 0) {
      PLOG(ERROR) << "kevent (monitor)";
    } else {
      LOG(ERROR) << "kevent (monitor): unexpected result " << events;
    }
    return;
  }

  if (event.filter != EVFILT_PROC ||
      event.fflags != NOTE_EXIT ||
      event.ident != static_cast<uintptr_t>(parent_pid)) {
    LOG(ERROR) << "kevent (monitor): unexpected event, filter " << event.filter
               << ", fflags " << event.fflags << ", ident " << event.ident;
    return;
  }
}

}  // namespace

namespace internal {

int RelauncherMain(const MainFunctionParams& main_parameters) {
  // CommandLine rearranges the order of the arguments returned by
  // main_parameters.argv(), rendering it impossible to determine which
  // arguments originally came before kRelauncherArgSeparator and which came
  // after. It's crucial to distinguish between these because only those
  // after the separator should be given to the relaunched process; it's also
  // important to not treat the path to the relaunched process as a "loose"
  // argument. NXArgc and NXArgv are pointers to the original argc and argv as
  // passed to main(), so use those. The typical mechanism to do this is to
  // provide "extern" declarations to access these, but they're only present
  // in the crt1.o start file. This function will be linked into the framework
  // dylib, having no direct access to anything in crt1.o. dlsym to the
  // rescue.
  const int* argcp = static_cast<const int*>(dlsym(RTLD_MAIN_ONLY, "NXArgc"));
  if (!argcp) {
    LOG(ERROR) << "dlsym NXArgc: " << dlerror();
    return 1;
  }
  int argc = *argcp;

  const char* const** argvp =
      static_cast<const char* const**>(dlsym(RTLD_MAIN_ONLY, "NXArgv"));
  if (!argvp) {
    LOG(ERROR) << "dlsym NXArgv: " << dlerror();
    return 1;
  }
  const char* const* argv = *argvp;

  if (argc < 4 || RelauncherTypeArg() != argv[1]) {
    LOG(ERROR) << "relauncher process invoked with unexpected arguments";
    return 1;
  }

  RelauncherSynchronizeWithParent();

  // The capacity for relaunch_args is 4 less than argc, because it
  // won't contain the argv[0] of the relauncher process, the
  // RelauncherTypeArg() at argv[1], kRelauncherArgSeparator, or the
  // executable path of the process to be launched.
  base::mac::ScopedCFTypeRef<CFMutableArrayRef> relaunch_args(
      CFArrayCreateMutable(NULL, argc - 4, &kCFTypeArrayCallBacks));
  if (!relaunch_args) {
    LOG(ERROR) << "CFArrayCreateMutable";
    return 1;
  }

  // Figure out what to execute, what arguments to pass it, and whether to
  // start it in the background.
  bool background = false;
  bool in_relaunch_args = false;
  std::string dmg_bsd_device_name;
  bool seen_relaunch_executable = false;
  std::string relaunch_executable;
  const std::string relauncher_arg_separator(kRelauncherArgSeparator);
  for (int argv_index = 2; argv_index < argc; ++argv_index) {
    const std::string arg(argv[argv_index]);

    // Strip any -psn_ arguments, as they apply to a specific process.
    if (arg.compare(0, strlen(kPSNArg), kPSNArg) == 0) {
      continue;
    }

    if (!in_relaunch_args) {
      if (arg == relauncher_arg_separator) {
        in_relaunch_args = true;
      } else if (arg == kRelauncherBackgroundArg) {
        background = true;
      } else if (arg.compare(0, strlen(kRelauncherDMGDeviceArg),
                             kRelauncherDMGDeviceArg) == 0) {
        dmg_bsd_device_name.assign(arg.substr(strlen(kRelauncherDMGDeviceArg)));
      }
    } else {
      if (!seen_relaunch_executable) {
        // The first argument after kRelauncherBackgroundArg is the path to
        // the executable file or .app bundle directory. The Launch Services
        // interface wants this separate from the rest of the arguments. In
        // the relaunched process, this path will still be visible at argv[0].
        relaunch_executable.assign(arg);
        seen_relaunch_executable = true;
      } else {
        base::mac::ScopedCFTypeRef<CFStringRef> arg_cf(
            base::SysUTF8ToCFStringRef(arg));
        if (!arg_cf) {
          LOG(ERROR) << "base::SysUTF8ToCFStringRef failed for " << arg;
          return 1;
        }
        CFArrayAppendValue(relaunch_args, arg_cf);
      }
    }
  }

  if (!seen_relaunch_executable) {
    LOG(ERROR) << "nothing to relaunch";
    return 1;
  }

  FSRef app_fsref;
  if (!base::mac::FSRefFromPath(relaunch_executable, &app_fsref)) {
    LOG(ERROR) << "base::mac::FSRefFromPath failed for " << relaunch_executable;
    return 1;
  }

  LSApplicationParameters ls_parameters = {
    0,  // version
    kLSLaunchDefaults | kLSLaunchAndDisplayErrors | kLSLaunchNewInstance |
        (background ? kLSLaunchDontSwitch : 0),
    &app_fsref,
    NULL,  // asyncLaunchRefCon
    NULL,  // environment
    relaunch_args,
    NULL   // initialEvent
  };

  OSStatus status = LSOpenApplication(&ls_parameters, NULL);
  if (status != noErr) {
    LOG(ERROR) << "LSOpenApplication: " << status;
    return 1;
  }

  // The application should have relaunched (or is in the process of
  // relaunching). From this point on, only clean-up tasks should occur, and
  // failures are tolerable.

  if (!dmg_bsd_device_name.empty()) {
    EjectAndTrashDiskImage(dmg_bsd_device_name);
  }

  return 0;
}

}  // namespace internal

}  // namespace mac_relauncher
