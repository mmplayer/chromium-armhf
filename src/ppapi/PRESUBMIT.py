# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import subprocess

def CheckChange(input_api, output_api):
  results = []

  # Verify all modified *.idl have a matching *.h
  files = input_api.LocalPaths()
  h_files = []
  idl_files = []

  for filename in files:
    name, ext = os.path.splitext(filename)
    name_parts = name.split(os.sep)
    if name_parts[0:2] == ['ppapi', 'c'] and ext == '.h':
      h_files.append('/'.join(name_parts[2:]))
    if name_parts[0:2] == ['ppapi', 'api'] and ext == '.idl':
      idl_files.append('/'.join(name_parts[2:]))

  # Generate a list of all appropriate *.h and *.idl changes in this CL.
  both = h_files + idl_files

  # If there aren't any, we are done checking.
  if not both: return results

  missing = []
  for filename in idl_files:
    if filename not in set(h_files):
      missing.append('  ppapi/c/%s.h' % filename)
  for filename in h_files:
    if filename not in set(idl_files):
      missing.append('  ppapi/api/%s.idl' % filename)

  if missing:
    results.append(
        output_api.PresubmitPromptWarning('Missing matching PPAPI definition:',
                                          long_text='\n'.join(missing)))

  # Verify all *.h files match *.idl definitions, use:
  #   --test to prevent output to disk
  #   --diff to generate a unified diff
  #   --out to pick which files to examine (only the ones in the CL)
  ppapi_dir = input_api.PresubmitLocalPath()
  cmd = [ sys.executable, 'generator.py',
          '--wnone', '--diff', '--test','--cgen', '--range=M13,M16']

  # Only generate output for IDL files references (as *.h or *.idl) in this CL
  cmd.append('--out=' + ','.join([name + '.idl' for name in both]))

  p = subprocess.Popen(cmd, cwd=os.path.join(ppapi_dir, 'generators'),
                       stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE)
  (p_stdout, p_stderr) = p.communicate()
  if p.returncode:
    results.append(
        output_api.PresubmitError('PPAPI IDL Diff detected: Run the generator.',
                                  long_text=p_stderr))
  return results

def CheckChangeOnUpload(input_api, output_api):
#  return []
  return CheckChange(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
#  return []
  return CheckChange(input_api, output_api)

