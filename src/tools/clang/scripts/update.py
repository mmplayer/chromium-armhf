#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Windows can't run .sh files, so this is a small python wrapper around
# update.sh.

import os
import subprocess
import sys

if __name__ == '__main__':
  if sys.platform in ['win32', 'cygwin']:
    sys.exit(0)

  # This script is called by gclient. gclient opens its hooks subprocesses with
  # (stdout=subprocess.PIPE, stderr=subprocess.STDOUT) and then does custom
  # output processing that breaks printing '\r' characters for single-line
  # updating status messages as printed by curl and wget.
  # Work around this by setting stderr of the update.sh process to stdin (!):
  # gclient doesn't redirect stdin, and while stdin itself is read-only, the
  # subprocess module dup()s it in the child process - and a dup()ed sys.stdin
  # is writable, try
  #   fd2 = os.dup(sys.stdin.fileno()); os.write(fd2, 'hi')
  # TODO: Fix gclient instead, http://crbug.com/95350
  subprocess.call(
      [os.path.join(os.path.dirname(__file__), 'update.sh')] +  sys.argv[1:],
      stderr=sys.stdin)
