// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/test_server.h"

#include <poll.h>

#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/test/test_timeouts.h"

namespace {

// Helper class used to detect and kill orphaned python test server processes.
// Checks if the command line of a process contains |path_string| (the path
// from which the test server was launched) and |port_string| (the port used by
// the test server), and if the parent pid of the process is 1 (indicating that
// it is an orphaned process).
class OrphanedTestServerFilter : public base::ProcessFilter {
 public:
  OrphanedTestServerFilter(
      const std::string& path_string, const std::string& port_string)
      : path_string_(path_string),
        port_string_(port_string) {}

  virtual bool Includes(const base::ProcessEntry& entry) const {
    if (entry.parent_pid() != 1)
      return false;
    bool found_path_string = false;
    bool found_port_string = false;
    for (std::vector<std::string>::const_iterator it =
         entry.cmd_line_args().begin();
         it != entry.cmd_line_args().end();
         ++it) {
      if (it->find(path_string_) != std::string::npos)
        found_path_string = true;
      if (it->find(port_string_) != std::string::npos)
        found_port_string = true;
    }
    return found_path_string && found_port_string;
  }

 private:
  std::string path_string_;
  std::string port_string_;
  DISALLOW_COPY_AND_ASSIGN(OrphanedTestServerFilter);
};

// Given a file descriptor, reads into |buffer| until |bytes_max|
// bytes has been read or an error has been encountered.  Returns true
// if the read was successful.  |remaining_time| is used as a timeout.
bool ReadData(int fd, ssize_t bytes_max, uint8* buffer,
              base::TimeDelta* remaining_time) {
  ssize_t bytes_read = 0;
  base::Time previous_time = base::Time::Now();
  while (bytes_read < bytes_max) {
    struct pollfd poll_fds[1];

    poll_fds[0].fd = fd;
    poll_fds[0].events = POLLIN | POLLPRI;
    poll_fds[0].revents = 0;

    int rv = HANDLE_EINTR(poll(poll_fds, 1,
                               remaining_time->InMilliseconds()));
    if (rv != 1) {
      LOG(ERROR) << "Failed to poll for the child file descriptor.";
      return false;
    }

    base::Time current_time = base::Time::Now();
    base::TimeDelta elapsed_time_cycle = current_time - previous_time;
    DCHECK(elapsed_time_cycle.InMilliseconds() >= 0);
    *remaining_time -= elapsed_time_cycle;
    previous_time = current_time;

    ssize_t num_bytes = HANDLE_EINTR(read(fd, buffer + bytes_read,
                                          bytes_max - bytes_read));
    if (num_bytes <= 0)
      return false;
    bytes_read += num_bytes;
  }
  return true;
}

}  // namespace

namespace net {

bool TestServer::LaunchPython(const FilePath& testserver_path) {
  CommandLine python_command(FilePath(FILE_PATH_LITERAL("python")));
  python_command.AppendArgPath(testserver_path);
  if (!AddCommandLineArguments(&python_command))
    return false;

  int pipefd[2];
  if (pipe(pipefd) != 0) {
    PLOG(ERROR) << "Could not create pipe.";
    return false;
  }

  // Save the read half. The write half is sent to the child.
  child_fd_ = pipefd[0];
  child_fd_closer_.reset(&child_fd_);
  file_util::ScopedFD write_closer(&pipefd[1]);
  base::file_handle_mapping_vector map_write_fd;
  map_write_fd.push_back(std::make_pair(pipefd[1], pipefd[1]));

  python_command.AppendArg("--startup-pipe=" + base::IntToString(pipefd[1]));

  // Try to kill any orphaned testserver processes that may be running.
  OrphanedTestServerFilter filter(testserver_path.value(),
                                  base::IntToString(host_port_pair_.port()));
  if (!base::KillProcesses("python", -1, &filter)) {
    LOG(WARNING) << "Failed to clean up older orphaned testserver instances.";
  }

  // Launch a new testserver process.
  base::LaunchOptions options;
  options.fds_to_remap = &map_write_fd;
  if (!base::LaunchProcess(python_command, options, &process_handle_)) {
    LOG(ERROR) << "Failed to launch " << python_command.GetCommandLineString();
    return false;
  }

  return true;
}

bool TestServer::WaitToStart() {
  file_util::ScopedFD child_fd_closer(child_fd_closer_.release());

  base::TimeDelta remaining_time = base::TimeDelta::FromMilliseconds(
      TestTimeouts::action_timeout_ms());

  uint32 server_data_len = 0;
  if (!ReadData(child_fd_, sizeof(server_data_len),
                reinterpret_cast<uint8*>(&server_data_len),
                &remaining_time)) {
    LOG(ERROR) << "Could not read server_data_len";
    return false;
  }
  std::string server_data(server_data_len, '\0');
  if (!ReadData(child_fd_, server_data_len,
                reinterpret_cast<uint8*>(&server_data[0]),
                &remaining_time)) {
    LOG(ERROR) << "Could not read server_data (" << server_data_len
               << " bytes)";
    return false;
  }

  if (!ParseServerData(server_data)) {
    LOG(ERROR) << "Could not parse server_data: " << server_data;
    return false;
  }

  return true;
}

}  // namespace net
