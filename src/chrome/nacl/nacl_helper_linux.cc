// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A mini-zygote specifically for Native Client.

#include "chrome/common/nacl_helper_linux.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "chrome/nacl/nacl_listener.h"
#include "content/common/main_function_params.h"
#include "content/common/unix_domain_socket_posix.h"
#include "ipc/ipc_switches.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"

namespace {

bool g_suid_sandbox_active;

// The child must mimic the behavior of zygote_main_linux.cc on the child
// side of the fork. See zygote_main_linux.cc:HandleForkRequest from
//   if (!child) {
// Note: this code doesn't attempt to support SELINUX or the SECCOMP sandbox.
void BecomeNaClLoader(const std::vector<int>& child_fds) {
  VLOG(1) << "NaCl loader: setting up IPC descriptor";
  // don't need zygote FD any more
  if (HANDLE_EINTR(close(kNaClZygoteDescriptor)) != 0)
    LOG(ERROR) << "close(kNaClZygoteDescriptor) failed.";
  // Set up browser descriptor as expected by Chrome on fd 3
  // The zygote takes care of putting the sandbox IPC channel on fd 5
  int zfd = dup2(child_fds[kNaClBrowserFDIndex], kNaClBrowserDescriptor);
  if (zfd != kNaClBrowserDescriptor) {
    LOG(ERROR) << "Could not initialize kNaClBrowserDescriptor";
    _exit(-1);
  }

  MessageLoopForIO main_message_loop;
  NaClListener listener;
  listener.Listen();
  _exit(0);
}

// Some of this code was lifted from
// content/browser/zygote_main_linux.cc:ForkWithRealPid()
void HandleForkRequest(const std::vector<int>& child_fds) {
  VLOG(1) << "nacl_helper: forking";
  pid_t childpid = fork();
  if (childpid < 0) {
    perror("fork");
    LOG(ERROR) << "*** HandleForkRequest failed\n";
    // fall through to parent case below
  } else if (childpid == 0) {  // In the child process.
    bool validack = false;
    const size_t kMaxReadSize = 1024;
    char buffer[kMaxReadSize];
    // Wait until the parent process has discovered our PID.  We
    // should not fork any child processes (which the seccomp
    // sandbox does) until then, because that can interfere with the
    // parent's discovery of our PID.
    const int nread = HANDLE_EINTR(read(child_fds[kNaClParentFDIndex], buffer,
                                        kMaxReadSize));
    const std::string switch_prefix = std::string("--") +
        switches::kProcessChannelID + std::string("=");
    const size_t len = switch_prefix.length();

    if (nread < 0) {
      perror("read");
      LOG(ERROR) << "read returned " << nread;
    } else if (nread > static_cast<int>(len)) {
      if (switch_prefix.compare(0, len, buffer, 0, len) == 0) {
        VLOG(1) << "NaCl loader is synchronised with Chrome zygote";
        CommandLine::ForCurrentProcess()->AppendSwitchASCII(
            switches::kProcessChannelID,
            std::string(&buffer[len], nread - len));
        validack = true;
      }
    }
    if (HANDLE_EINTR(close(child_fds[kNaClDummyFDIndex])) != 0)
      LOG(ERROR) << "close(child_fds[kNaClDummyFDIndex]) failed";
    if (HANDLE_EINTR(close(child_fds[kNaClParentFDIndex])) != 0)
      LOG(ERROR) << "close(child_fds[kNaClParentFDIndex]) failed";
    if (validack) {
      BecomeNaClLoader(child_fds);
    } else {
      LOG(ERROR) << "Failed to synch with zygote";
    }
    // NOTREACHED
    return;
  }
  // I am the parent.
  // First, close the dummy_fd so the sandbox won't find me when
  // looking for the child's pid in /proc. Also close other fds.
  for (size_t i = 0; i < child_fds.size(); i++) {
    if (HANDLE_EINTR(close(child_fds[i])) != 0)
      LOG(ERROR) << "close(child_fds[i]) failed";
  }
  VLOG(1) << "nacl_helper: childpid is " << childpid;
  // Now tell childpid to the Chrome zygote.
  if (HANDLE_EINTR(send(kNaClZygoteDescriptor,
                        &childpid, sizeof(childpid), MSG_EOR))
      != sizeof(childpid)) {
    LOG(ERROR) << "*** send() to zygote failed";
  }
}

}  // namespace

static const char kNaClHelperAtZero[] = "at-zero";

int main(int argc, char *argv[]) {
  CommandLine::Init(argc, argv);
  base::AtExitManager exit_manager;
  base::RandUint64();  // acquire /dev/urandom fd before sandbox is raised
  std::vector<int> empty; // for SendMsg() calls

  g_suid_sandbox_active = (NULL != getenv("SBX_D"));

  if (CommandLine::ForCurrentProcess()->HasSwitch(kNaClHelperAtZero)) {
    g_nacl_prereserved_sandbox_addr = (void *) (uintptr_t) 0x10000;
  }

  // Send the zygote a message to let it know we are ready to help
  if (!UnixDomainSocket::SendMsg(kNaClZygoteDescriptor,
                                 kNaClHelperStartupAck,
                                 sizeof(kNaClHelperStartupAck), empty)) {
    LOG(ERROR) << "*** send() to zygote failed";
  }

  while (true) {
    int badpid = -1;
    std::vector<int> fds;
    static const unsigned kMaxMessageLength = 2048;
    char buf[kMaxMessageLength];
    const ssize_t msglen = UnixDomainSocket::RecvMsg(kNaClZygoteDescriptor,
                                                     &buf, sizeof(buf), &fds);
    if (msglen == 0 || (msglen == -1 && errno == ECONNRESET)) {
      // EOF from the browser. Goodbye!
      _exit(0);
    }
    if (msglen == sizeof(kNaClForkRequest) - 1 &&
        memcmp(buf, kNaClForkRequest, msglen) == 0) {
      if (kNaClParentFDIndex + 1 == fds.size()) {
        HandleForkRequest(fds);
        continue;  // fork succeeded. Note: child does not return
      } else {
        LOG(ERROR) << "nacl_helper: unexpected number of fds, got "
                   << fds.size();
      }
    } else {
      if (msglen != 0) {
        LOG(ERROR) << "nacl_helper unrecognized request: %s";
        _exit(-1);
      }
    }
    // if fork fails, send PID=-1 to zygote
    if (!UnixDomainSocket::SendMsg(kNaClZygoteDescriptor, &badpid,
                                   sizeof(badpid), empty)) {
      LOG(ERROR) << "*** send() to zygote failed";
    }
  }
  CHECK(false);  // This routine must not return
}
