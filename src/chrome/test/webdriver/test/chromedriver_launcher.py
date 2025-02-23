#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Launches and kills ChromeDriver.

For ChromeDriver documentation, refer to:
  http://dev.chromium.org/developers/testing/webdriver-for-chrome
"""

import os
import socket
import subprocess

import chromedriver_server


class ChromeDriverLauncher(object):
  """Launches the ChromeDriver server process."""

  def __init__(self, exe_path, root_path=None, url_base=None):
    """Initializes the launcher.

    Args:
      exe_path:   path to the ChromeDriver executable, which must exist
      root_path:  base path from which ChromeDriver webserver will serve files;
                  if None, the webserver will not serve files
      url_base:   base URL which ChromeDriver webserver will listen from
    Raises:
      RuntimeError if ChromeDriver executable does not exist
    """
    self._exe_path = exe_path
    self._root_path = root_path
    self._url_base = url_base
    if not os.path.exists(self._exe_path):
      raise RuntimeError('ChromeDriver exe not found at: ' + self._exe_path)

  def Launch(self):
    """Starts a new ChromeDriver server process."""
    port = self._FindOpenPort()
    chromedriver_args = [self._exe_path, '--port=%d' % port]
    if self._root_path is not None:
      chromedriver_args += ['--root=%s' % self._root_path]
    if self._url_base is not None:
      chromedriver_args += ['--url-base=%s' % self._url_base]
    proc = subprocess.Popen(chromedriver_args,
                            stdout=subprocess.PIPE)
    if proc is None:
      raise RuntimeError('ChromeDriver cannot be started')

    return chromedriver_server.ChromeDriverServer(proc, port, self._url_base)

  def _FindOpenPort(self):
    for port in range(9500, 10000):
      try:
        socket.create_connection(('127.0.0.1', port), 0.2).close()
      except socket.error:
        return port
    raise RuntimeError('Cannot find open port to launch ChromeDriver')
