#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Extract UserMetrics "actions" strings from the Chrome source.

This program generates the list of known actions we expect to see in the
user behavior logs.  It walks the Chrome source, looking for calls to
UserMetrics functions, extracting actions and warning on improper calls,
as well as generating the lists of possible actions in situations where
there are many possible actions.

See also:
  content/browser/user_metrics.h
  http://wiki.corp.google.com/twiki/bin/view/Main/ChromeUserExperienceMetrics

If run with a "--hash" argument, chromeactions.txt will be updated.
"""

__author__ = 'evanm (Evan Martin)'

import hashlib
from HTMLParser import HTMLParser
import os
import re
import sys

sys.path.insert(1, os.path.join(sys.path[0], '..', '..', 'tools', 'python'))
from google import path_utils

# Files that are known to use UserMetrics::RecordComputedAction(), which means
# they require special handling code in this script.
# To add a new file, add it to this list and add the appropriate logic to
# generate the known actions to AddComputedActions() below.
KNOWN_COMPUTED_USERS = (
  'back_forward_menu_model.cc',
  'options_page_view.cc',
  'render_view_host.cc',  # called using webkit identifiers
  'user_metrics.cc',  # method definition
  'new_tab_ui.cc',  # most visited clicks 1-9
  'extension_metrics_module.cc', # extensions hook for user metrics
  'safe_browsing_blocking_page.cc', # various interstitial types and actions
  'language_options_handler_common.cc', # languages and input methods in CrOS
  'cros_language_options_handler.cc', # languages and input methods in CrOS
  'about_flags.cc', # do not generate a warning; see AddAboutFlagsActions()
  'external_metrics.cc',  # see AddChromeOSActions()
  'core_options_handler.cc',  # see AddWebUIActions()
  'browser_render_process_host.cc'  # see AddRendererActions()
)

# Language codes used in Chrome. The list should be updated when a new
# language is added to app/l10n_util.cc, as follows:
#
# % (cat app/l10n_util.cc | \
#    perl -n0e 'print $1 if /kAcceptLanguageList.*?\{(.*?)\}/s' | \
#    perl -nle 'print $1, if /"(.*)"/'; echo 'es-419') | \
#   sort | perl -pe "s/(.*)\n/'\$1', /" | \
#   fold -w75 -s | perl -pe 's/^/  /;s/ $//'; echo
#
# The script extracts language codes from kAcceptLanguageList, but es-419
# (Spanish in Latin America) is an exception.
LANGUAGE_CODES = (
  'af', 'am', 'ar', 'az', 'be', 'bg', 'bh', 'bn', 'br', 'bs', 'ca', 'co',
  'cs', 'cy', 'da', 'de', 'de-AT', 'de-CH', 'de-DE', 'el', 'en', 'en-AU',
  'en-CA', 'en-GB', 'en-NZ', 'en-US', 'en-ZA', 'eo', 'es', 'es-419', 'et',
  'eu', 'fa', 'fi', 'fil', 'fo', 'fr', 'fr-CA', 'fr-CH', 'fr-FR', 'fy',
  'ga', 'gd', 'gl', 'gn', 'gu', 'ha', 'haw', 'he', 'hi', 'hr', 'hu', 'hy',
  'ia', 'id', 'is', 'it', 'it-CH', 'it-IT', 'ja', 'jw', 'ka', 'kk', 'km',
  'kn', 'ko', 'ku', 'ky', 'la', 'ln', 'lo', 'lt', 'lv', 'mk', 'ml', 'mn',
  'mo', 'mr', 'ms', 'mt', 'nb', 'ne', 'nl', 'nn', 'no', 'oc', 'om', 'or',
  'pa', 'pl', 'ps', 'pt', 'pt-BR', 'pt-PT', 'qu', 'rm', 'ro', 'ru', 'sd',
  'sh', 'si', 'sk', 'sl', 'sn', 'so', 'sq', 'sr', 'st', 'su', 'sv', 'sw',
  'ta', 'te', 'tg', 'th', 'ti', 'tk', 'to', 'tr', 'tt', 'tw', 'ug', 'uk',
  'ur', 'uz', 'vi', 'xh', 'yi', 'yo', 'zh', 'zh-CN', 'zh-TW', 'zu',
)

# Input method IDs used in Chrome OS. The list should be updated when a
# new input method is added to platform/assets/input_methods/whitelist.txt
# in the Chrome OS tree, as follows:
#
# % sort chromeos/src/platform/assets/input_methods/whitelist.txt | \
#   perl -ne "print \"'\$1', \" if /^([^#]+?)\s/" | \
#   fold -w75 -s | perl -pe 's/^/  /;s/ $//'; echo
#
# The script extracts input method IDs from whitelist.txt.
INPUT_METHOD_IDS = (
  'chewing', 'hangul', 'm17n:ar:kbd', 'm17n:fa:isiri', 'm17n:hi:itrans',
  'm17n:th:kesmanee', 'm17n:th:pattachote', 'm17n:th:tis820',
  'm17n:vi:tcvn', 'm17n:vi:telex', 'm17n:vi:viqr', 'm17n:vi:vni',
  'm17n:zh:cangjie', 'm17n:zh:quick', 'mozc', 'mozc-dv', 'mozc-jp',
  'pinyin', 'xkb:be::fra', 'xkb:be::ger', 'xkb:be::nld', 'xkb:bg::bul',
  'xkb:bg:phonetic:bul', 'xkb:br::por', 'xkb:ca::fra', 'xkb:ca:eng:eng',
  'xkb:ch::ger', 'xkb:ch:fr:fra', 'xkb:cz::cze', 'xkb:de::ger',
  'xkb:dk::dan', 'xkb:ee::est', 'xkb:es::spa', 'xkb:es:cat:cat',
  'xkb:fi::fin', 'xkb:fr::fra', 'xkb:gb:extd:eng', 'xkb:gr::gre',
  'xkb:hr::scr', 'xkb:hu::hun', 'xkb:il::heb', 'xkb:it::ita', 'xkb:jp::jpn',
  'xkb:kr:kr104:kor', 'xkb:lt::lit', 'xkb:lv::lav', 'xkb:nl::nld',
  'xkb:no::nor', 'xkb:pl::pol', 'xkb:pt::por', 'xkb:ro::rum', 'xkb:rs::srp',
  'xkb:ru::rus', 'xkb:ru:phonetic:rus', 'xkb:se::swe', 'xkb:si::slv',
  'xkb:sk::slo', 'xkb:tr::tur', 'xkb:ua::ukr', 'xkb:us::eng',
  'xkb:us:altgr-intl:eng', 'xkb:us:dvorak:eng',
)

number_of_files_total = 0


def AddComputedActions(actions):
  """Add computed actions to the actions list.

  Arguments:
    actions: set of actions to add to.
  """

  # Actions for back_forward_menu_model.cc.
  for dir in ('BackMenu_', 'ForwardMenu_'):
    actions.add(dir + 'ShowFullHistory')
    actions.add(dir + 'Popup')
    for i in range(1, 20):
      actions.add(dir + 'HistoryClick' + str(i))
      actions.add(dir + 'ChapterClick' + str(i))

  # Actions for new_tab_ui.cc.
  for i in range(1, 10):
    actions.add('MostVisited%d' % i)

  # Actions for safe_browsing_blocking_page.cc.
  for interstitial in ('Phishing', 'Malware', 'Multiple'):
    for action in ('Show', 'Proceed', 'DontProceed'):
      actions.add('SBInterstitial%s%s' % (interstitial, action))

  # Actions for language_options_handler.cc (Chrome OS specific).
  for input_method_id in INPUT_METHOD_IDS:
    actions.add('LanguageOptions_DisableInputMethod_%s' % input_method_id)
    actions.add('LanguageOptions_EnableInputMethod_%s' % input_method_id)
    actions.add('InputMethodOptions_Open_%s' % input_method_id)
  for language_code in LANGUAGE_CODES:
    actions.add('LanguageOptions_UiLanguageChange_%s' % language_code)
    actions.add('LanguageOptions_SpellCheckLanguageChange_%s' % language_code)

def AddWebKitEditorActions(actions):
  """Add editor actions from editor_client_impl.cc.

  Arguments:
    actions: set of actions to add to.
  """
  action_re = re.compile(r'''\{ [\w']+, +\w+, +"(.*)" +\},''')

  editor_file = os.path.join(path_utils.ScriptDir(), '..', '..', 'webkit',
                             'api', 'src','EditorClientImpl.cc')
  for line in open(editor_file):
    match = action_re.search(line)
    if match:  # Plain call to RecordAction
      actions.add(match.group(1))

def AddClosedSourceActions(actions):
  """Add actions that are in code which is not checked out by default

  Arguments
    actions: set of actions to add to.
  """
  actions.add('PDF.PrintPage')
  actions.add('PDF.FitToHeightButton')
  actions.add('PDF.FitToWidthButton')
  actions.add('PDF.LoadFailure')
  actions.add('PDF.LoadSuccess')
  actions.add('PDF.PreviewDocumentLoadFailure')
  actions.add('PDF.ZoomFromBrowser')
  actions.add('PDF.ZoomOutButton')
  actions.add('PDF.ZoomInButton')
  actions.add('PDF_Unsupported_Rights_Management')
  actions.add('PDF_Unsupported_XFA')
  actions.add('PDF_Unsupported_3D')
  actions.add('PDF_Unsupported_Movie')
  actions.add('PDF_Unsupported_Sound')
  actions.add('PDF_Unsupported_Screen')
  actions.add('PDF_Unsupported_Portfolios_Packages')
  actions.add('PDF_Unsupported_Attachment')
  actions.add('PDF_Unsupported_Digital_Signature')
  actions.add('PDF_Unsupported_Shared_Review')
  actions.add('PDF_Unsupported_Shared_Form')
  actions.add('PDF_Unsupported_Bookmarks')

def AddAboutFlagsActions(actions):
  """This parses the experimental feature flags for UMA actions.

  Arguments:
    actions: set of actions to add to.
  """
  about_flags = os.path.join(path_utils.ScriptDir(), '..', 'browser',
                             'about_flags.cc')
  flag_name_re = re.compile(r'\s*"([0-9a-zA-Z\-_]+)",\s*// FLAGS:RECORD_UMA')
  for line in open(about_flags):
    match = flag_name_re.search(line)
    if match:
      actions.add("AboutFlags_" + match.group(1))
    # If the line contains the marker but was not matched by the regex, put up
    # an error if the line is not a comment.
    elif 'FLAGS:RECORD_UMA' in line and line[0:2] != '//':
      print >>sys.stderr, 'WARNING: This line is marked for recording ' + \
          'about:flags metrics, but is not in the proper format:\n' + line

def AddChromeOSActions(actions):
  """Add actions reported by non-Chrome processes in Chrome OS.

  Arguments:
    actions: set of actions to add to.
  """
  # Actions sent by the Chrome OS window manager.
  actions.add('Accel_NextWindow_Tab')
  actions.add('Accel_PrevWindow_Tab')
  actions.add('Accel_NextWindow_F5')
  actions.add('Accel_PrevWindow_F5')

  # Actions sent by the Chrome OS power manager.
  actions.add('Accel_BrightnessDown_F6')
  actions.add('Accel_BrightnessUp_F7')

  # Actions sent by Chrome OS update engine.
  actions.add('Updater.ServerCertificateChanged')
  actions.add('Updater.ServerCertificateFailed')

  # Actions sent by Chrome OS cryptohome.
  actions.add('Cryptohome.PKCS11InitFail')

def GrepForActions(path, actions):
  """Grep a source file for calls to UserMetrics functions.

  Arguments:
    path: path to the file
    actions: set of actions to add to
  """
  global number_of_files_total
  number_of_files_total = number_of_files_total + 1
  # we look for the UserMetricsAction structure constructor
  # this should be on one line
  action_re = re.compile(r'[^a-zA-Z]UserMetricsAction\("([^"]*)')
  malformed_action_re = re.compile(r'[^a-zA-Z]UserMetricsAction\([^"]')
  computed_action_re = re.compile(r'UserMetrics::RecordComputedAction')
  line_number = 0
  for line in open(path):
    line_number = line_number + 1
    match = action_re.search(line)
    if match:  # Plain call to RecordAction
      actions.add(match.group(1))
    elif malformed_action_re.search(line):
      # Warn if this line is using RecordAction incorrectly.
      print >>sys.stderr, ('WARNING: %s has malformed call to RecordAction'
                           ' at %d' % (path, line_number))
    elif computed_action_re.search(line):
      # Warn if this file shouldn't be calling RecordComputedAction.
      if os.path.basename(path) not in KNOWN_COMPUTED_USERS:
        print >>sys.stderr, ('WARNING: %s has RecordComputedAction at %d' %
                             (path, line_number))

class WebUIActionsParser(HTMLParser):
  """Parses an HTML file, looking for all tags with a 'metric' attribute.
  Adds user actions corresponding to any metrics found.

  Arguments:
    actions: set of actions to add to
  """
  def __init__(self, actions):
    HTMLParser.__init__(self)
    self.actions = actions

  def handle_starttag(self, tag, attrs):
    # We only care to examine tags that have a 'metric' attribute.
    attrs = dict(attrs)
    if not 'metric' in attrs:
      return

    # Boolean metrics have two corresponding actions.  All other metrics have
    # just one corresponding action.  By default, we check the 'dataType'
    # attribute.
    is_boolean = ('dataType' in attrs and attrs['dataType'] == 'boolean')
    if 'type' in attrs and attrs['type'] in ('checkbox', 'radio'):
      if attrs['type'] == 'checkbox':
        # Checkboxes are boolean by default.  However, their 'value-type' can
        # instead be set to 'integer'.
        if 'value-type' not in attrs or attrs['value-type'] in ['', 'boolean']:
          is_boolean = True
      else:
        # Radio buttons are boolean if and only if their values are 'true' or
        # 'false'.
        assert(attrs['type'] == 'radio')
        if 'value' in attrs and attrs['value'] in ['true', 'false']:
          is_boolean = True

    if is_boolean:
      self.actions.add(attrs['metric'] + '_Enable')
      self.actions.add(attrs['metric'] + '_Disable')
    else:
      self.actions.add(attrs['metric'])

def GrepForWebUIActions(path, actions):
  """Grep a WebUI source file for elements with associated metrics.

  Arguments:
    path: path to the file
    actions: set of actions to add to
  """
  parser = WebUIActionsParser(actions)
  parser.feed(open(path).read())
  parser.close()

def WalkDirectory(root_path, actions, extensions, callback):
  for path, dirs, files in os.walk(root_path):
    if '.svn' in dirs:
      dirs.remove('.svn')
    for file in files:
      ext = os.path.splitext(file)[1]
      if ext in extensions:
        callback(os.path.join(path, file), actions)

def GrepForRendererActions(path, actions):
  """Grep a source file for calls to RenderThread::RecordUserMetrics.

  Arguments:
    path: path to the file
    actions: set of actions to add to
  """
  # We look for the ViewHostMsg_UserMetricsRecordAction constructor.
  # This should be on one line.
  action_re = re.compile(
      r'[^a-zA-Z]RenderThread::RecordUserMetrics\("([^"]*)')
  line_number = 0
  for line in open(path):
    match = action_re.search(line)
    if match:  # Plain call to RecordAction
      actions.add(match.group(1))

def AddLiteralActions(actions):
  """Add literal actions specified via calls to UserMetrics functions.

  Arguments:
    actions: set of actions to add to.
  """
  EXTENSIONS = ('.cc', '.mm', '.c', '.m')

  # Walk the source tree to process all .cc files.
  chrome_root = os.path.normpath(os.path.join(path_utils.ScriptDir(), '..'))
  WalkDirectory(chrome_root, actions, EXTENSIONS, GrepForActions)
  content_root = os.path.normpath(os.path.join(path_utils.ScriptDir(),
                                               '..', '..', 'content'))
  WalkDirectory(content_root, actions, EXTENSIONS, GrepForActions)
  webkit_root = os.path.normpath(os.path.join(path_utils.ScriptDir(),
                                              '..', '..', 'webkit'))
  WalkDirectory(os.path.join(webkit_root, 'glue'), actions, EXTENSIONS,
                GrepForActions)
  WalkDirectory(os.path.join(webkit_root, 'port'), actions, EXTENSIONS,
                GrepForActions)

def AddWebUIActions(actions):
  """Add user actions defined in WebUI files.

  Arguments:
    actions: set of actions to add to.
  """
  resources_root = os.path.join(path_utils.ScriptDir(), '..', 'browser',
                                'resources')
  WalkDirectory(resources_root, actions, ('.html'), GrepForWebUIActions)

def AddRendererActions(actions):
  """Add user actions sent via calls to RenderThread::RecordUserMetrics.

  Arguments:
    actions: set of actions to add to.
  """
  EXTENSIONS = ('.cc', '.mm', '.c', '.m')

  chrome_renderer_root = os.path.join(path_utils.ScriptDir(), '..', 'renderer')
  content_renderer_root = os.path.join(path_utils.ScriptDir(), '..', '..',
      'content', 'renderer')
  WalkDirectory(chrome_renderer_root, actions, EXTENSIONS,
                GrepForRendererActions)
  WalkDirectory(content_renderer_root, actions, EXTENSIONS,
                GrepForRendererActions)

def main(argv):
  if '--hash' in argv:
    hash_output = True
  else:
    hash_output = False
    print >>sys.stderr, "WARNING: If you added new UMA tags, you must" + \
           " use the --hash option to update chromeactions.txt."
  # if we do a hash output, we want to only append NEW actions, and we know
  # the file we want to work on
  actions = set()

  chromeactions_path = os.path.join(path_utils.ScriptDir(), "chromeactions.txt")

  if hash_output:
    f = open(chromeactions_path)
    for line in f:
      part = line.rpartition("\t")
      part = part[2].strip()
      actions.add(part)
    f.close()


  AddComputedActions(actions)
  # TODO(fmantek): bring back webkit editor actions.
  # AddWebKitEditorActions(actions)
  AddAboutFlagsActions(actions)
  AddWebUIActions(actions)
  AddRendererActions(actions)

  AddLiteralActions(actions)

  # print "Scanned {0} number of files".format(number_of_files_total)
  # print "Found {0} entries".format(len(actions))

  AddClosedSourceActions(actions)
  AddChromeOSActions(actions)

  if hash_output:
    f = open(chromeactions_path, "w")


  # Print out the actions as a sorted list.
  for action in sorted(actions):
    if hash_output:
      hash = hashlib.md5()
      hash.update(action)
      print >>f, '0x%s\t%s' % (hash.hexdigest()[:16], action)
    else:
      print action

  if hash_output:
    print "Done. Do not forget to add chromeactions.txt to your changelist"

if '__main__' == __name__:
  main(sys.argv)
