#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate a C++ header from ibus_input_methods.txt.

This program generates a C++ header file containing the information on
available ibus engines.  It parses ibus_input_methods.txt, and then
generates a static array definition from the information extracted. The
input and output file names are specified on the command line.

Run it like:
  gen_ibus_input_methods.py ibus_input_methods.txt ibus_input_methods.h

It will produce output that looks like:

// This file is automatically generated by gen_engines.py
#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_INPUT_METHODS_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_INPUT_METHODS_H_

namespace chromeos {
namespace input_method {

struct IBusEngineInfo {
  const char* input_method_id;
  const char* language_code;
  const char* xkb_keyboard_id;
  const char* keyboard_overlay_id;
};
const IBusEngineInfo kIBusEngines[] = {
  {"mozc-chewing", "zh-TW", "us", "zh-TW"},
  {"xkb:us::eng", "eng", "us", "en_US"},
  {"xkb:us:dvorak:eng", "eng", "us(dvorak)", "en_US_dvorak"},
  {"xkb:be::fra", "fra", "be", "fr"},
  {"xkb:br::por", "por", "br", "pt_BR"},
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_INPUT_METHODS_H_

"""

import fileinput
import re
import sys

OUTPUT_HEADER = """// Automatically generated by gen_ibus_input_methods.py
#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_INPUT_METHODS_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_INPUT_METHODS_H_

namespace chromeos {
namespace input_method {

struct IBusEngineInfo {
  const char* input_method_id;
  const char* language_code;
  const char* xkb_layout_id;
};
const IBusEngineInfo kIBusEngines[] = {
"""

CPP_FORMAT = '#if %s\n'
ENGINE_FORMAT = ('  {"%(input_method_id)s", "%(language_code)s", ' +
                 '"%(xkb_layout_id)s"},\n')

OUTPUT_FOOTER = """
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_INPUT_METHODS_H_
"""

def CreateEngineHeader(engines):
  """Create the header file from a list of engines.

  Arguments:
    engines: list of ibus engine objects
  Returns:
    The text of a C++ header file containing the engine data.
  """
  output = []
  output.append(OUTPUT_HEADER)
  for engine in engines:
    if engine.has_key('if'):
      output.append(CPP_FORMAT % engine['if'])
    output.append(ENGINE_FORMAT % engine)
    if engine.has_key('if'):
      output.append('#endif\n')
  output.append(OUTPUT_FOOTER)

  return "".join(output)


def main(argv):
  if len(argv) != 3:
    print 'Usage: gen_ibus_input_methods.py [whitelist] [output]'
    sys.exit(1)
  engines = []
  for line in fileinput.input(sys.argv[1]):
    line = line.strip()
    if not line or re.match(r'#', line):
      continue
    columns = line.split()
    assert len(columns) == 3 or len(columns) == 4, "Invalid format: " + line
    engine = {}
    engine['input_method_id'] = columns[0]
    engine['xkb_layout_id'] = columns[1]
    engine['language_code'] = columns[2]
    if len(columns) == 4:
      engine['if'] = columns[3]
    engines.append(engine)

  output = CreateEngineHeader(engines)
  output_file = open(sys.argv[2], 'w')
  output_file.write(output)


if __name__ == '__main__':
  main(sys.argv)
