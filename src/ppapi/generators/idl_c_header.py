#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Generator for C style prototypes and definitions """

import glob
import os
import sys
import subprocess

from idl_log import ErrOut, InfoOut, WarnOut
from idl_node import IDLAttribute, IDLNode
from idl_ast import IDLAst
from idl_option import GetOption, Option, ParseOptions
from idl_outfile import IDLOutFile
from idl_parser import ParseFiles
from idl_c_proto import CGen, GetNodeComments, CommentLines, Comment
from idl_generator import Generator, GeneratorByFile

Option('dstroot', 'Base directory of output', default=os.path.join('..', 'c'))
Option('guard', 'Include guard prefix', default=os.path.join('ppapi', 'c'))
Option('out', 'List of output files', default='')

def GetOutFileName(filenode, relpath=None, prefix=None):
  path, name = os.path.split(filenode.GetProperty('NAME'))
  name = os.path.splitext(name)[0] + '.h'
  if prefix: name = '%s%s' % (prefix, name)
  if path: name = os.path.join(path, name)
  if relpath: name = os.path.join(relpath, name)
  return name

def WriteGroupMarker(out, node, last_group):
  # If we are part of a group comment marker...
  if last_group and last_group != node.cls:
    pre = CommentLines(['*',' @}', '']) + '\n'
  else:
    pre = '\n'

  if node.cls in ['Typedef', 'Interface', 'Struct', 'Enum']:
    if last_group != node.cls:
      pre += CommentLines(['*',' @addtogroup %ss' % node.cls, ' @{', ''])
    last_group = node.cls
  else:
    last_group = None
  out.Write(pre)
  return last_group


def GenerateHeader(out, filenode, releases):
  gpath = GetOption('guard')
  cgen = CGen()
  pref = ''
  do_comments = True

  # Generate definitions.
  last_group = None
  top_types = ['Typedef', 'Interface', 'Struct', 'Enum', 'Inline']
  for node in filenode.GetListOf(*top_types):
    # Skip if this node is not in this release
    if not node.InReleases(releases):
      print "Skiping %s" % node
      continue

    # End/Start group marker
    if do_comments:
      last_group = WriteGroupMarker(out, node, last_group)

    if node.IsA('Inline'):
      item = node.GetProperty('VALUE')
      # If 'C++' use __cplusplus wrapper
      if node.GetName() == 'cc':
        item = '#ifdef __cplusplus\n%s\n#endif  // __cplusplus\n\n' % item
      # If not C++ or C, then skip it
      elif not node.GetName() == 'c':
        continue
      if item: out.Write(item)
      continue

    #
    # Otherwise we are defining a file level object, so generate the
    # correct document notation.
    #
    item = cgen.Define(node, releases, prefix=pref, comment=True)
    if not item: continue
    asize = node.GetProperty('assert_size()')
    if asize:
      name = '%s%s' % (pref, node.GetName())
      if node.IsA('Struct'):
        form = 'PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(%s, %s);\n'
      else:
        form = 'PP_COMPILE_ASSERT_SIZE_IN_BYTES(%s, %s);\n'
      item += form % (name, asize[0])

    if item: out.Write(item)
  if last_group:
    out.Write(CommentLines(['*',' @}', '']) + '\n')


class HGen(GeneratorByFile):
  def __init__(self):
    Generator.__init__(self, 'C Header', 'cgen', 'Generate the C headers.')

  def GetMacro(self, node):
    name = node.GetName()
    name = name.upper()
    return "%s_INTERFACE" % name

  def GetDefine(self, name, value):
    out = '#define %s %s' % (name, value)
    if len(out) > 80:
      out = '#define %s \\\n    %s' % (name, value)
    return '%s\n' % out

  def GetVersionString(self, node):
    # If an interface name is specified, use that
    iname = node.GetProperty('iname')
    if iname: return iname

    # Otherwise, the interface name is the object's name
    # With '_Dev' replaced by '(Dev)' if it's a Dev interface.
    name = node.GetName()
    if len(name) > 4 and name[-4:] == '_Dev':
      name = '%s(Dev)' % name[:-4]
    return name

  def GetOutFile(self, filenode, options):
    savename = GetOutFileName(filenode, GetOption('dstroot'))
    return IDLOutFile(savename)

  def GenerateHead(self, out, filenode, releases, options):
    cgen = CGen()
    gpath = GetOption('guard')
    release = releases[0]
    def_guard = GetOutFileName(filenode, relpath=gpath)
    def_guard = def_guard.replace(os.sep,'_').replace('.','_').upper() + '_'

    cright_node = filenode.GetChildren()[0]
    assert(cright_node.IsA('Copyright'))
    fileinfo = filenode.GetChildren()[1]
    assert(fileinfo.IsA('Comment'))

    out.Write('%s\n' % cgen.Copyright(cright_node))
    out.Write('/* From %s modified %s. */\n\n'% (
        filenode.GetProperty('NAME').replace(os.sep,'/'),
        filenode.GetProperty('DATETIME')))
    out.Write('#ifndef %s\n#define %s\n\n' % (def_guard, def_guard))
    # Generate set of includes

    deps = set()
    for release in releases:
      deps |= filenode.GetDeps(release)

    includes = set([])
    for dep in deps:
      depfile = dep.GetProperty('FILE')
      if depfile:
        includes.add(depfile)
    includes = [GetOutFileName(
        include, relpath=gpath).replace(os.sep, '/') for include in includes]
    includes.append('ppapi/c/pp_macros.h')

    # Assume we need stdint if we "include" C or C++ code
    if filenode.GetListOf('Include'):
      includes.append('ppapi/c/pp_stdint.h')

    includes = sorted(set(includes))
    cur_include = GetOutFileName(filenode, relpath=gpath).replace(os.sep, '/')
    for include in includes:
      if include == cur_include: continue
      out.Write('#include "%s"\n' % include)

    # Generate all interface defines
    out.Write('\n')
    for node in filenode.GetListOf('Interface'):
      idefs = ''
      name = self.GetVersionString(node)
      macro = node.GetProperty('macro')
      if not macro:
        macro = self.GetMacro(node)

      unique = node.GetUniqueReleases(releases)
      for rel in unique:
        version = node.GetVersion(rel)
        strver = str(version).replace('.', '_')
        idefs += self.GetDefine('%s_%s' % (macro, strver),
                                '"%s;%s"' % (name, version))
      idefs += self.GetDefine(macro, '%s_%s' % (macro, strver)) + '\n'
      out.Write(idefs)

    # Generate the @file comment
    out.Write('%s\n' % Comment(fileinfo, prefix='*\n @file'))

  def GenerateBody(self, out, filenode, releases, options):
    GenerateHeader(out, filenode, releases)

  def GenerateTail(self, out, filenode, releases, options):
    gpath = GetOption('guard')
    def_guard = GetOutFileName(filenode, relpath=gpath)
    def_guard = def_guard.replace(os.sep,'_').replace('.','_').upper() + '_'
    out.Write('#endif  /* %s */\n\n' % def_guard)


hgen = HGen()

def Main(args):
  # Default invocation will verify the golden files are unchanged.
  failed = 0
  if not args:
    args = ['--wnone', '--diff', '--test', '--dstroot=.']

  ParseOptions(args)

  idldir = os.path.split(sys.argv[0])[0]
  idldir = os.path.join(idldir, 'test_cgen', '*.idl')
  filenames = glob.glob(idldir)
  ast = ParseFiles(filenames)
  if hgen.GenerateRelease(ast, 'M14', {}):
    print "Golden file for M14 failed."
    failed = 1
  else:
    print "Golden file for M14 passed."


  idldir = os.path.split(sys.argv[0])[0]
  idldir = os.path.join(idldir, 'test_cgen_range', '*.idl')
  filenames = glob.glob(idldir)

  ast = ParseFiles(filenames)
  if hgen.GenerateRange(ast, ['M13', 'M14', 'M15'], {}):
    print "Golden file for M13-M15 failed."
    failed =1
  else:
    print "Golden file for M13-M15 passed."

  return failed

if __name__ == '__main__':
  retval = Main(sys.argv[1:])
  sys.exit(retval)

