#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from idl_log import ErrOut, InfoOut, WarnOut
from idl_option import GetOption, Option, ParseOptions

GeneratorList = []

Option('release', 'Which release to generate.', default='')
Option('range', 'Which ranges in the form of MIN,MAX.', default='M13,M16')


#
# Generator
#
# Base class for generators.  This class provides a mechanism for
# adding new generator objects to the IDL driver.  To use this class
# override the GenerateRelease and GenerateRange members, and
# instantiate one copy of the class in the same module which defines it to
# register the generator.  After the AST is generated, call the static Run
# member which will check every registered generator to see which ones have
# been enabled through command-line options.  To enable a generator use the
# switches:
#  --<sname> : To enable with defaults
#  --<sname>_opt=<XXX,YYY=y> : To enable with generator specific options.
#
# NOTE:  Generators still have access to global options

class Generator(object):
  def __init__(self, name, sname, desc):
    self.name = name
    self.run_switch = Option(sname, desc)
    self.opt_switch = Option(sname + '_opt', 'Options for %s.' % sname,
                             default='')
    GeneratorList.append(self)
    self.errors = 0

  def Error(self, msg):
    ErrOut.Log('Error %s : %s' % (self.name, msg))
    self.errors += 1

  def GetRunOptions(self):
    options = {}
    option_list = self.opt_switch.Get()
    if option_list:
      option_list = option_list.split(',')
      for opt in option_list:
        offs = opt.find('=')
        if offs > 0:
          options[opt[:offs]] = opt[offs+1:]
        else:
          options[opt] = True
      return options
    if self.run_switch.Get():
      return options
    return None

  def Generate(self, ast, options):
    self.errors = 0

    rangestr = GetOption('range')
    releasestr = GetOption('release')

    # Check for a range option which over-rides a release option
    if not releasestr and rangestr:
      range_list = rangestr.split(',')
      if len(range_list) != 2:
        self.Error('Failed to generate for %s, incorrect range: "%s"' %
                   (self.name, rangestr))
      else:
        vmin = range_list[0]
        vmax = range_list[1]
        vmin = ast.releases.index(vmin)
        vmax = ast.releases.index(vmax) + 1
        range = ast.releases[vmin:vmax]
        InfoOut.Log('Generate range %s of %s.' % (range, self.name))
        ret = self.GenerateRange(ast, range, options)
        if ret < 0:
          self.Error('Failed to generate range %s : %s.' %(vmin, vmax))
        else:
          InfoOut.Log('%s wrote %d files.' % (self.name, ret))
    # Otherwise this should be a single release generation
    else:
      if releasestr:
        InfoOut.Log('Generate release %s of %s.' % (releasestr, self.name))
        ret = self.GenerateRelease(ast, releasestr, options)
        if ret < 0:
          self.Error('Failed to generate release %s.' % releasestr)
        else:
          InfoOut.Log('%s wrote %d files.' % (self.name, ret))

      else:
        self.Error('No range or release specified for %s.' % releasestr)
    return self.errors

  def GenerateRelease(self, ast, release, options):
    __pychecker__ = 'unusednames=ast,release,options'
    self.Error("Undefined release generator.")
    return 0

  def GenerateRange(self, ast, vmin, vmax, options):
    __pychecker__ = 'unusednames=ast,vmin,vmax,options'
    self.Error("Undefined range generator.")
    return 0

  @staticmethod
  def Run(ast):
    fail_count = 0

    # Check all registered generators if they should run.
    for gen in GeneratorList:
      options = gen.GetRunOptions()
      if options is not None:
        if gen.Generate(ast, options):
          fail_count += 1
    return fail_count


#
# GeneratorByFile
#
# A subclass of Generator for use of generators which have a one to one
# mapping between IDL sources and output files. To use, derive a new class
# which defines:
#
#  GetOutFile - Returns an IDLOutFile based on filenode (name) and options
#  GenerateHead - Writes the first part of the file (includes, etc...)
#  GenerateBody - Writes the body of the file (definitions)
#  GenerateTail - Writes the end of the file (closing include guard, etc...)
#
class GeneratorByFile(Generator):
  def GenerateRelease(self, ast, release, options):
    return self.GenerateRange(ast, [release], options)

  def GenerateRange(self, ast, releases, options):
    # Get list of out files
    outlist = GetOption('out')
    if outlist: outlist = outlist.split(',')

    skipList = []
    cnt = 0
    for filenode in ast.GetListOf('File'):
      # Skip this file if not required
      if outlist and filenode.GetName() not in outlist:
        continue

      # If this file has errors, skip it
      if filenode.GetProperty('ERRORS') > 0:
        skipList.append(filenode)
        continue

      # Create output file
      out = self.GetOutFile(filenode, options)
      self.GenerateHead(out, filenode, releases, options)
      self.GenerateBody(out, filenode, releases, options)
      self.GenerateTail(out, filenode, releases, options)

      if out.Close(): cnt = cnt + 1

    for filenode in skipList:
      errcnt = filenode.GetProperty('ERRORS')
      ErrOut.Log('%s : Skipped because of %d errors.' % (
          filenode.GetName(), errcnt))

    if skipList:
      return -len(skipList)

    if GetOption('diff'):
      return -cnt
    return cnt


check_release = 0
check_range = 0

class GeneratorReleaseTest(Generator):
  def GenerateRelease(self, ast, release, options = {}):
    __pychecker__ = 'unusednames=ast,release,options'
    global check_release
    check_map = {
      'so_long': True,
      'MyOpt': 'XYZ',
      'goodbye': True
    }
    check_release = 1
    for item in check_map:
      check_item = check_map[item]
      option_item = options.get(item, None)
      if check_item != option_item:
        print 'Option %s is %s, expecting %s' % (item, option_item, check_item)
        check_release = 0

    if release != 'M14':
      check_release = 0
    return check_release == 1

  def GenerateRange(self, ast, vmin, vmax, options = {}):
    __pychecker__ = 'unusednames=ast,vmin,vmax,options'
    global check_range
    check_range = 1
    return True

def Test():
  __pychecker__ = 'unusednames=args'
  global check_release
  global check_range

  ParseOptions(['--testgen_opt=so_long,MyOpt=XYZ,goodbye'])
  if Generator.Run('AST') != 0:
    print 'Generate release: Failed.\n'
    return -1

  if check_release != 1 or check_range != 0:
    print 'Gererate release: Failed to run.\n'
    return -1

  check_release = 0
  ParseOptions(['--testgen_opt="HELLO"', '--range=M14,M16'])
  if Generator.Run('AST') != 0:
    print 'Generate range: Failed.\n'
    return -1

  if check_release != 0 or check_range != 1:
    print 'Gererate range: Failed to run.\n'
    return -1

  print 'Generator test: Pass'
  return 0


def Main(args):
  if not args: return Test()

  filenames = ParseOptions(args)
  ast = ParseFiles(filenames)

  return Generator.Run(ast)


if __name__ == '__main__':
  GeneratorReleaseTest('Test Gen', 'testgen', 'Generator Class Test.')
  sys.exit(Main(sys.argv[1:]))

