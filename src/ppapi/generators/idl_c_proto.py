#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Generator for C style prototypes and definitions """

import glob
import os
import sys

from idl_log import ErrOut, InfoOut, WarnOut
from idl_node import IDLNode
from idl_ast import IDLAst
from idl_option import GetOption, Option, ParseOptions
from idl_parser import ParseFiles

Option('cgen_debug', 'Debug generate.')

class CGenError(Exception):
  def __init__(self, msg):
    self.value = value

  def __str__(self):
    return repr(self.value)


def CommentLines(lines, tabs=0):
  # Generate a C style comment block by prepending the block with '<tab>/*'
  # and adding a '<tab> *' per line.
  tab = '  ' * tabs

  out = '%s/*' % tab + ('\n%s *' % tab).join(lines)

  # Add a terminating ' */' unless the last line is blank which would mean it
  # already has ' *'
  if not lines[-1]:
    out += '/\n'
  else:
    out += ' */\n'
  return out

def Comment(node, prefix=None, tabs=0):
  # Generate a comment block from the provided Comment node.
  comment = node.GetName()
  lines = comment.split('\n')

  # If an option prefix is provided, then prepend that to the comment
  # for this node.
  if prefix:
    prefix_lines = prefix.split('\n')
    # If both the prefix and comment start with a blank line ('*') remove
    # the extra one.
    if prefix_lines[0] == '*' and lines[0] == '*':
      lines = prefix_lines + lines[1:]
    else:
      lines = prefix_lines + lines;
  return CommentLines(lines, tabs)

def GetNodeComments(node, prefix=None, tabs=0):
  # Generate a comment block joining all comment nodes which are children of
  # the provided node.
  comment_txt = ''
  for doc in node.GetListOf('Comment'):
    comment_txt += Comment(doc, tabs=tabs)
  return comment_txt


class CGen(object):
  # TypeMap
  #
  # TypeMap modifies how an object is stored or passed, for example pointers
  # are passed as 'const' if they are 'in' parameters, and structures are
  # preceeded by the keyword 'struct' as well as using a pointer.
  #
  TypeMap = {
    'Array': {
      'in': 'const %s',
      'inout': '%s*',
      'out': '%s*',
      'store': '%s',
      'return': '%s'
    },
    'Callspec': {
      'in': '%s',
      'inout': '%s',
      'out': '%s',
      'store': '%s',
      'return': '%s'
    },
    'Enum': {
      'in': '%s',
      'inout': '%s*',
      'out': '%s*',
      'store': '%s',
      'return': '%s'
    },
    'Struct': {
      'in': 'const %s*',
      'inout': '%s*',
      'out': '%s*',
      'return': ' %s*',
      'store': '%s'
    },
    'blob_t': {
      'in': 'const %s',
      'inout': '%s',
      'out': '%s',
      'return': '%s',
      'store': '%s'
    },
    'mem_t': {
      'in': 'const %s',
      'inout': '%s',
      'out': '%s',
      'return': '%s',
      'store': '%s'
    },
    'str_t': {
      'in': 'const %s',
      'inout': '%s',
      'out': '%s',
      'return': 'const %s',
      'store': '%s'
    },
    'TypeValue': {
      'in': '%s',
      'inout': '%s*',
      'out': '%s*',
      'return': '%s',
      'store': '%s'
    },
  }


  #
  # RemapName
  #
  # A diction array of PPAPI types that are converted to language specific
  # types before being returned by by the C generator
  #
  RemapName = {
  'blob_t': 'void**',
  'float_t': 'float',
  'double_t': 'double',
  'handle_t': 'int',
  'mem_t': 'void*',
  'str_t': 'char*',
  'interface_t' : 'const void*'
  }

  def __init__(self):
    self.dbg_depth = 0

  #
  # Debug Logging functions
  #
  def Log(self, txt):
    if not GetOption('cgen_debug'): return
    tabs = ''
    for tab in range(self.dbg_depth): tabs += '  '
    print '%s%s' % (tabs, txt)

  def LogEnter(self, txt):
    if txt: self.Log(txt)
    self.dbg_depth += 1

  def LogExit(self, txt):
    self.dbg_depth -= 1
    if txt: self.Log(txt)

  #
  # Return the array specification of the object.
  #
  def GetArraySpec(self, node):
    assert(node.cls == 'Array')
    out = ''
    fixed = node.GetProperty('FIXED')
    if fixed:
      return '[%s]' % fixed
    else:
      return '[]'

  #
  # GetTypeName
  #
  # For any valid 'typed' object such as Member or Typedef
  # the typenode object contains the typename
  #
  # For a given node return the type name by passing mode.
  #
  def GetTypeName(self, node, release, prefix=''):
    self.LogEnter('GetTypeName of %s rel=%s' % (node, release))

    # For Members, Params, and Typedefs get the type it refers to otherwise
    # the node in question is it's own type (struct, union etc...)
    if node.IsA('Member', 'Param', 'Typedef'):
      typeref = node.GetType(release)
    else:
      typeref = node

    if typeref is None:
      raise CGenError('No type for %s' % node)

    # If the type is a (BuiltIn) Type then return it's name
    # remapping as needed
    if typeref.IsA('Type'):
      name = CGen.RemapName.get(typeref.GetName(), None)
      if name is None: name = typeref.GetName()
      name = '%s%s' % (prefix, name)

    # For structures, preceed with 'struct' or 'union' as appropriate
    elif typeref.IsA('Interface', 'Struct'):
      if typeref.GetProperty('union'):
        name = 'union %s%s' % (prefix, typeref.GetName())
      else:
        name = 'struct %s%s' % (prefix, typeref.GetName())

    # If it's an enum, or typedef then return the Enum's name
    elif typeref.IsA('Enum', 'Typedef'):
      name = '%s%s' % (prefix, typeref.GetName())

    else:
      raise RuntimeError('Getting name of non-type %s.' % node)
    self.LogExit('GetTypeName %s is %s' % (node, name))
    return name


  #
  # GetRootType
  #
  # For a given node return basic type of that object.  This is
  # either a 'Type', 'Callspec', or 'Array'
  #
  def GetRootTypeMode(self, node, release, mode):
    self.LogEnter('GetRootType of %s' % node)
    # If it has an array spec, then treat it as an array regardless of type
    if node.GetOneOf('Array'):
      rootType = 'Array'
    # Or if it has a callspec, treat it as a function
    elif node.GetOneOf('Callspec'):
      rootType, mode = self.GetRootTypeMode(node.GetType(release), release,
                                            'return')

    # If it's a plain typedef, try that object's root type
    elif node.IsA('Member', 'Param', 'Typedef'):
      rootType, mode = self.GetRootTypeMode(node.GetType(release),
                                            release, mode)

    # If it's an Enum, then it's normal passing rules
    elif node.IsA('Enum'):
      rootType = node.cls

    # If it's an Interface or Struct, we may be passing by value
    elif node.IsA('Interface', 'Struct'):
      if mode == 'return':
        if node.GetProperty('returnByValue'):
          rootType = 'TypeValue'
        else:
          rootType = node.cls
      else:
        if node.GetProperty('passByValue'):
          rootType = 'TypeValue'
        else:
          rootType = node.cls

    # If it's an Basic Type, check if it's a special type
    elif node.IsA('Type'):
      if node.GetName() in CGen.TypeMap:
        rootType = node.GetName()
      else:
        rootType = 'TypeValue'
    else:
      raise RuntimeError('Getting root type of non-type %s.' % node)
    self.LogExit('RootType is "%s"' % rootType)
    return rootType, mode


  def GetTypeByMode(self, node, release, mode):
    self.LogEnter('GetTypeByMode of %s mode=%s release=%s' %
                  (node, mode, release))
    name = self.GetTypeName(node, release)
    ntype, mode = self.GetRootTypeMode(node, release, mode)
    out = CGen.TypeMap[ntype][mode] % name
    self.LogExit('GetTypeByMode %s = %s' % (node, out))
    return out


  # Get the passing mode of the object (in, out, inout).
  def GetParamMode(self, node):
    self.Log('GetParamMode for %s' % node)
    if node.GetProperty('in'): return 'in'
    if node.GetProperty('out'): return 'out'
    if node.GetProperty('inout'): return 'inout'
    return 'return'

  #
  # GetComponents
  #
  # Returns the signature components of an object as a tuple of
  # (rtype, name, arrays, callspec) where:
  #   rtype - The store or return type of the object.
  #   name - The name of the object.
  #   arrays - A list of array dimensions as [] or [<fixed_num>].
  #   args -  None of not a function, otherwise  a list of parameters.
  #
  def GetComponents(self, node, release, mode):
    self.LogEnter('GetComponents mode %s for %s %s' % (mode, node, release))

    # Generate passing type by modifying root type
    rtype = self.GetTypeByMode(node, release, mode)
    if node.IsA('Enum', 'Interface', 'Struct'):
      rname = node.GetName()
    else:
      rname = node.GetType(release).GetName()

    if rname in CGen.RemapName:
      rname = CGen.RemapName[rname]
    if '%' in rtype:
      rtype = rtype % rname
    name = node.GetName()
    arrayspec = [self.GetArraySpec(array) for array in node.GetListOf('Array')]
    callnode = node.GetOneOf('Callspec')
    if callnode:
      callspec = []
      for param in callnode.GetListOf('Param'):
        mode = self.GetParamMode(param)
        ptype, pname, parray, pspec = self.GetComponents(param, release, mode)
        callspec.append((ptype, pname, parray, pspec))
    else:
      callspec = None

    self.LogExit('GetComponents: %s, %s, %s, %s' %
                 (rtype, name, arrayspec, callspec))
    return (rtype, name, arrayspec, callspec)


  def Compose(self, rtype, name, arrayspec, callspec, prefix, func_as_ptr):
    self.LogEnter('Compose: %s %s' % (rtype, name))
    arrayspec = ''.join(arrayspec)
    name = '%s%s%s' % (prefix, name, arrayspec)
    if callspec is None:
      out = '%s %s' % (rtype, name)
    else:
      params = []
      for ptype, pname, parray, pspec in callspec:
        params.append(self.Compose(ptype, pname, parray, pspec, '', True))
      if func_as_ptr: name = '(*%s)' % name
      out = '%s %s(%s)' % (rtype, name, ', '.join(params))
    self.LogExit('Exit Compose: %s' % out)
    return out

  #
  # GetSignature
  #
  # Returns the 'C' style signature of the object
  #  prefix - A prefix for the object's name
  #  func_as_ptr - Formats a function as a function pointer
  #
  def GetSignature(self, node, release, mode, prefix='', func_as_ptr=True):
    self.LogEnter('GetSignature %s %s as func=%s' % (node, mode, func_as_ptr))
    rtype, name, arrayspec, callspec = self.GetComponents(node, release, mode)
    out = self.Compose(rtype, name, arrayspec, callspec, prefix, func_as_ptr)
    self.LogExit('Exit GetSignature: %s' % out)
    return out

  # Define a Typedef.
  def DefineTypedef(self, node, releases, prefix='', comment=False):
    release = releases[0]
    out = 'typedef %s;\n' % self.GetSignature(node, release, 'return',
                                              prefix, True)
    self.Log('DefineTypedef: %s' % out)
    return out

  # Define an Enum.
  def DefineEnum(self, node, releases, prefix='', comment=False):
    self.LogEnter('DefineEnum %s' % node)
    unnamed =  node.GetProperty('unnamed')
    if unnamed:
      out = 'enum {'
    else:
      out = 'typedef enum {'
    name = '%s%s' % (prefix, node.GetName())
    enumlist = []
    for child in node.GetListOf('EnumItem'):
      value = child.GetProperty('VALUE')
      comment_txt = GetNodeComments(child, tabs=1)
      if value:
        item_txt = '%s%s = %s' % (prefix, child.GetName(), value)
      else:
        item_txt = '%s%s' % (prefix, child.GetName())
      enumlist.append('%s  %s' % (comment_txt, item_txt))
    self.LogExit('Exit DefineEnum')

    if unnamed:
      out = '%s\n%s\n};\n' % (out, ',\n'.join(enumlist))
    else:
      out = '%s\n%s\n} %s;\n' % (out, ',\n'.join(enumlist), name)
    return out

  def DefineMember(self, node, releases, prefix='', comment=False):
    release = releases[0]
    self.LogEnter('DefineMember %s' % node)
    out = '%s;' % self.GetSignature(node, release, 'store', '', True)
    self.LogExit('Exit DefineMember')
    return out

  def DefineStructInternals(self, node, release, suffix='', comment=True):
    out = ''
    if node.GetProperty('union'):
      out += 'union %s%s {\n' % (node.GetName(), suffix)
    else:
      out += 'struct %s%s {\n' % (node.GetName(), suffix)

    # Generate Member Functions
    members = []
    for child in node.GetListOf('Member'):
      member = self.Define(child, [release], tabs=1, comment=comment)
      if not member:
        continue
      members.append(member)
    out += '%s\n};\n' % '\n'.join(members)
    return out


  def DefineStruct(self, node, releases, prefix='', comment=False):
    self.LogEnter('DefineStruct %s' % node)
    out = ''
    build_list = node.GetUniqueReleases(releases)

    # Build the most recent one with comments
    out = self.DefineStructInternals(node, build_list[-1], comment=True)

    # Build the rest without comments and with the version number appended
    for rel in build_list[0:-1]:
      ver_num = node.GetVersion(rel)
      ver = ("_%s" % ver_num).replace('.', '_')
      out += '\n' + self.DefineStructInternals(node, rel, suffix=ver,
                                               comment=False)

    self.LogExit('Exit DefineStruct')
    return out


  #
  # Copyright and Comment
  #
  # Generate a comment or copyright block
  #
  def Copyright(self, node, tabs=0):
    lines = node.GetName().split('\n')
    return CommentLines(lines, tabs)


  # Define a top level object.
  def Define(self, node, releases, tabs=0, prefix='', comment=False):
    if not node.InReleases(releases):
      return ''

    self.LogEnter('Define %s tab=%d prefix="%s"' % (node,tabs,prefix))
    declmap = {
      'Enum' : CGen.DefineEnum,
      'Function' : CGen.DefineMember,
      'Interface' : CGen.DefineStruct,
      'Member' : CGen.DefineMember,
      'Struct' : CGen.DefineStruct,
      'Typedef' : CGen.DefineTypedef,
    }

    out = ''
    func = declmap.get(node.cls)
    if not func:
      ErrOut.Log('Failed to define %s named %s' % (node.cls, node.GetName()))
    define_txt = func(self, node, releases, prefix=prefix, comment=comment)

    comment_txt = GetNodeComments(node, tabs=0)
    if comment_txt and comment:
      out += comment_txt
    out += define_txt

    tab = '  ' * tabs
    lines = []
    for line in out.split('\n'):
      # Add indentation
      line = tab + line
      if len(line) > 80:
        left = line.rfind('(') + 1
        args = line[left:].split(',')
        line_max = 0
        for arg in args:
          if len(arg) > line_max: line_max = len(arg)

        if left + line_max >= 80:
          space = '%s    ' % tab
          args =  (',\n%s' % space).join([arg.strip() for arg in args])
          lines.append('%s\n%s%s' % (line[:left], space, args))
        else:
          space = ' ' * (left - 1)
          args =  (',\n%s' % space).join(args)
          lines.append('%s%s' % (line[:left], args))
      else:
        lines.append(line.rstrip())
    self.LogExit('Exit Define')
    return '\n'.join(lines)

# Clean a string representing an object definition and return then string
# as a single space delimited set of tokens.
def CleanString(instr):
  instr = instr.strip()
  instr = instr.split()
  return ' '.join(instr)


# Test a file, by comparing all it's objects, with their comments.
def TestFile(filenode):
  cgen = CGen()

  errors = 0
  for node in filenode.GetChildren()[2:]:
    instr = node.GetOneOf('Comment')
    if not instr: continue
    instr.Dump()
    instr = CleanString(instr.GetName())

    outstr = cgen.Define(node, releases=['M14'])
    if GetOption('verbose'):
      print outstr + '\n'
    outstr = CleanString(outstr)

    if instr != outstr:
      ErrOut.Log('Failed match of\n>>%s<<\n>>%s<<\nto:' % (instr, outstr))
      node.Dump(1, comments=True)
      errors += 1
  return errors


# Build and resolve the AST and compare each file individual.
def TestFiles(filenames):
  if not filenames:
    idldir = os.path.split(sys.argv[0])[0]
    idldir = os.path.join(idldir, 'test_cgen', '*.idl')
    filenames = glob.glob(idldir)

  filenames = sorted(filenames)
  ast = ParseFiles(filenames)

  total_errs = 0
  for filenode in ast.GetListOf('File'):
    errs = TestFile(filenode)
    if errs:
      ErrOut.Log('%s test failed with %d error(s).' %
                 (filenode.GetName(), errs))
      total_errs += errs

  if total_errs:
    ErrOut.Log('Failed generator test.')
  else:
    InfoOut.Log('Passed generator test.')
  return total_errs

def Main(args):
  filenames = ParseOptions(args)
  if GetOption('test'):
    return TestFiles(filenames)
  ast = ParseFiles(filenames)
  for f in ast.GetListOf('File'):
    if f.GetProperty('ERRORS') > 0:
      print 'Skipping %s' % f.GetName()
      continue
    print DefineDepends(node)
    for node in f.GetChildren()[2:]:
      print Define(node, comment=True, prefix='tst_')


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
