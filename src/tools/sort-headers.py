#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Given a filename as an argument, sort the #include/#imports in that file.

Shows a diff and prompts for confirmation before doing the deed.
"""

import optparse
import os
import sys
import termios
import tty

def YesNo(prompt):
  """Prompts with a yes/no question, returns True if yes."""
  print prompt,
  sys.stdout.flush()
  # http://code.activestate.com/recipes/134892/
  fd = sys.stdin.fileno()
  old_settings = termios.tcgetattr(fd)
  ch = 'n'
  try:
    tty.setraw(sys.stdin.fileno())
    ch = sys.stdin.read(1)
  finally:
    termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
  print ch
  return ch in ('Y', 'y')


def IncludeCompareKey(line):
  """Sorting comparator key used for comparing two #include lines.
  Returns the filename without the #include/#import prefix.
  """
  for prefix in ('#include ', '#import '):
    if line.startswith(prefix):
      line = line[len(prefix):]
      break

  # The win32 api has all sorts of implicit include order dependencies :-/
  # Give a few headers special sort keys that make sure they appear before all
  # other headers.
  if line.startswith('<windows.h>'):  # Must be before e.g. shellapi.h
    return '0'
  if line.startswith('<unknwn.h>'):  # Must be before e.g. intshcut.h
    return '1'

  return line


def IsInclude(line):
  """Returns True if the line is an #include/#import line."""
  return line.startswith('#include ') or line.startswith('#import ')


def SortHeader(infile, outfile):
  """Sorts the headers in infile, writing the sorted file to outfile."""
  for line in infile:
    if IsInclude(line):
      headerblock = []
      while IsInclude(line):
        headerblock.append(line)
        line = infile.next()
      for header in sorted(headerblock, key=IncludeCompareKey):
        outfile.write(header)
      # Intentionally fall through, to write the line that caused
      # the above while loop to exit.
    outfile.write(line)


def main():
  parser = optparse.OptionParser(usage='%prog filename1 filename2 ...')
  opts, args = parser.parse_args()

  if len(args) < 1:
    parser.print_help()
    sys.exit(1)

  for filename in args:
    fixfilename = filename + '.new'
    infile = open(filename, 'r')
    outfile = open(fixfilename, 'w')
    SortHeader(infile, outfile)
    infile.close()
    outfile.close()  # Important so the below diff gets the updated contents.

    try:
      diff = os.system('diff -u %s %s' % (filename, fixfilename))
      if diff >> 8 == 0:  # Check exit code.
        print '%s: no change' % filename
        continue

      if YesNo('Use new file (y/N)?'):
        os.rename(fixfilename, filename)
    finally:
      try:
        os.remove(fixfilename)
      except OSError:
        # If the file isn't there, we don't care.
        pass


if __name__ == '__main__':
  main()
