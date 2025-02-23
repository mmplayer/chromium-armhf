#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main functions for the Layout Test Analyzer module."""

import csv
from datetime import datetime
import optparse
import os
import sys
import time

import layouttests
import layouttest_analyzer_helpers
from test_expectations import TestExpectations
from trend_graph import TrendGraph

# Predefined result directory.
DEFAULT_RESULT_DIR = 'result'
DEFAULT_ANNO_FILE = os.path.join('anno', 'anno.csv')
DEFAULT_GRAPH_FILE = os.path.join('graph', 'graph.html')
# Predefined result files for debug.
CUR_TIME_FOR_DEBUG = '2011-09-11-19'
CURRENT_RESULT_FILE_FOR_DEBUG = os.path.join(DEFAULT_RESULT_DIR,
                                             CUR_TIME_FOR_DEBUG)
PREV_TIME_FOR_DEBUG = '2011-09-11-18'


def ParseOption():
  """Parse command-line options using OptionParser.

  Returns:
      an object containing all command-line option information.
  """
  option_parser = optparse.OptionParser()

  option_parser.add_option('-r', '--receiver-email-address',
                           dest='receiver_email_address',
                           help=('receiver\'s email address. '
                                 'Result email is not sent if this is not '
                                 'specified.'))
  option_parser.add_option('-g', '--debug-mode', dest='debug',
                           help=('Debug mode is used when you want to debug '
                                 'the analyzer by using local file rather '
                                 'than getting data from SVN. This shortens '
                                 'the debugging time (off by default).'),
                           action='store_true', default=False)
  option_parser.add_option('-t', '--trend-graph-location',
                           dest='trend_graph_location',
                           help=('Location of the bug trend file; '
                                 'file is expected to be in Google '
                                 'Visualization API trend-line format '
                                 '(defaults to %default).'),
                           default=DEFAULT_GRAPH_FILE)
  option_parser.add_option('-a', '--bug-anno-file-location',
                           dest='bug_annotation_file_location',
                           help=('Location of the bug annotation file; '
                                 'file is expected to be in CSV format '
                                 '(default to %default).'),
                           default=DEFAULT_ANNO_FILE)
  option_parser.add_option('-n', '--test-group-file-location',
                           dest='test_group_file_location',
                           help=('Location of the test group file; '
                                 'file is expected to be in CSV format '
                                 'and lists all test name patterns. '
                                 'When this option is not specified, '
                                 'the value of --test-group-name is used '
                                 'for a test name pattern.'),
                           default=None)
  option_parser.add_option('-x', '--test-group-name',
                           dest='test_group_name',
                           help='A name of test group. Either '
                                '--test_group_file_location or this option '
                                'needs to be specified.')
  option_parser.add_option('-d', '--result-directory-location',
                           dest='result_directory_location',
                           help=('Name of result directory location '
                                 '(default to %default).'),
                           default=DEFAULT_RESULT_DIR)
  option_parser.add_option('-b', '--email-appended-text-file-location',
                           dest='email_appended_text_file_location',
                           help=('File location of the email appended text. '
                                 'The text is appended in the status email. '
                                 '(default to %default and no text is '
                                 'appended in that case).'),
                           default=None)
  option_parser.add_option('-c', '--email-only-change-mode',
                           dest='email_only_change_mode',
                           help=('With this mode, email is sent out '
                                 'only when there is a change in the '
                                 'analyzer result compared to the previous '
                                 'result (off by default)'),
                           action='store_true', default=False)
  option_parser.add_option('-q', '--dashboard-file-location',
                           dest='dashboard_file_location',
                           help=('Location of dashboard file. The results are '
                                 'not reported to the dashboard if this '
                                 'option is not specified.'))
  return option_parser.parse_args()[0]


def GetCurrentAndPreviousResults(debug, test_group_file_location,
                                 test_group_name, result_directory_location):
  """Get current and the latest previous analyzer results.

  In debug mode, they are read from predefined files. In non-debug mode,
  current analyzer results are dynamically obtained from Webkit SVN and
  the latest previous result is read from the corresponding file.

  Args:
    debug: please refer to |options|.
    test_group_file_location: please refer to |options|.
    test_group_name: please refer to |options|.
    result_directory_location: please refer to |options|.

  Returns:
    a tuple of the following:
       prev_time: the previous time string that is compared against.
       prev_analyzer_result_map: previous analyzer result map. Please refer to
          layouttest_analyzer_helpers.AnalyzerResultMap.
       analyzer_result_map: current analyzer result map. Please refer to
          layouttest_analyzer_helpers.AnalyzerResultMap.
  """
  if not debug:
    if not test_group_file_location and not test_group_name:
      print ('Either --test-group-name or --test_group_file_location must be '
             'specified. Exiting this program.')
      sys.exit()
    filter_names = []
    if test_group_file_location and os.path.exists(test_group_file_location):
      filter_names = layouttests.LayoutTests.GetLayoutTestNamesFromCSV(
          test_group_file_location)
      parent_location_list = (
          layouttests.LayoutTests.GetParentDirectoryList(filter_names))
      recursion = False
    else:
      # When test group CSV file is not specified, test group name
      # (e.g., 'media') is used for getting layout tests.
      # The tests are in
      #   http://svn.webkit.org/repository/webkit/trunk/LayoutTests/media
      # Filtering is not set so all HTML files are considered as valid tests.
      # Also, we look for the tests recursively.
      if not test_group_file_location or (
          not os.path.exists(test_group_file_location)):
        print ('Warning: CSV file (%s) does not exist. So it is ignored and '
               '%s is used for obtaining test names') % (
                   test_group_file_location, test_group_name)
      if not test_group_name.endswith('/'):
        test_group_name += '/'
      parent_location_list = [test_group_name]
      filter_names = None
      recursion = True
    layouttests_object = layouttests.LayoutTests(
        parent_location_list=parent_location_list, recursion=recursion,
        filter_names=filter_names)
    analyzer_result_map = layouttest_analyzer_helpers.AnalyzerResultMap(
        layouttests_object.JoinWithTestExpectation(TestExpectations()))
    result = layouttest_analyzer_helpers.FindLatestResult(
                 result_directory_location)
    if result:
      (prev_time, prev_analyzer_result_map) = result
    else:
      prev_time = None
      prev_analyzer_result_map = None
  else:
    analyzer_result_map = layouttest_analyzer_helpers.AnalyzerResultMap.Load(
        CURRENT_RESULT_FILE_FOR_DEBUG)
    prev_time = PREV_TIME_FOR_DEBUG
    prev_analyzer_result_map = (
        layouttest_analyzer_helpers.AnalyzerResultMap.Load(
            os.path.join(DEFAULT_RESULT_DIR, prev_time)))
  return (prev_time, prev_analyzer_result_map, analyzer_result_map)


def ReadEmailInformation(bug_annotation_file_location,
                         email_appended_text_file_location):
  """Read bug annotations and generate an annotation map used for email.

  Args:
    bug_annotation_file_location: please refer to |options|.
    email_appended_text_file_location: please refer to |options|.

  Returns:
    a tuple of the following:
      anno_map: a dictionary that maps bug names to their annotations.
      appended_text_to_email: the text string to append to the status email.
  """
  anno_map = {}
  try:
    file_object = open(bug_annotation_file_location)
  except IOError:
    print 'cannot open annotation file %s. Skipping.' % (
        bug_annotation_file_location)
  else:
    data = csv.reader(file_object)
    for row in data:
      anno_map[row[0]] = row[1]
    file_object.close()

  appended_text_to_email = ''
  if email_appended_text_file_location:
    try:
      file_object = open(email_appended_text_file_location)
    except IOError:
      print 'cannot open email appended text file %s. Skipping.' % (
          email_appended_text_file_location)
    else:
      appended_text_to_email = ''.join(file_object.readlines())
      file_object.close()
  return (anno_map, appended_text_to_email)


def SendEmail(prev_time, prev_analyzer_result_map, analyzer_result_map,
              anno_map, appended_text_to_email, email_only_change_mode, debug,
              receiver_email_address, test_group_name):
  """Send result status email.

  Args:
    prev_time: the previous time string that is compared against.
    prev_analyzer_result_map: previous analyzer result map. Please refer to
        layouttest_analyzer_helpers.AnalyzerResultMap.
    analyzer_result_map: current analyzer result map. Please refer to
        layouttest_analyzer_helpers.AnalyzerResultMap.
    anno_map: a dictionary that maps bug names to their annotations.
    appended_text_to_email: the text string to append to the status email.
    email_only_change_mode: please refer to |options|.
    debug: please refer to |options|.
    receiver_email_address: please refer to |options|.
    test_group_name: please refer to |options|.

  Returns:
    a tuple of the following:
        result_change: a boolean indicating whether there is a change in the
            result compared with the latest past result.
        diff_map: please refer to
            layouttest_analyzer_helpers.SendStatusEmail().
        simple_rev_str: a simple version of revision string that is sent in
            the email.
        rev: the latest revision number for the given test group.
        rev_date: the latest revision date for the given test group.
        email_content:  email content string (without
            |appended_text_to_email|) that will be shown on the dashboard.
  """
  rev = ''
  rev_date = ''
  email_content = ''
  if prev_analyzer_result_map:
    diff_map = analyzer_result_map.CompareToOtherResultMap(
        prev_analyzer_result_map)
    result_change = (any(diff_map['whole']) or any(diff_map['skip']) or
                        any(diff_map['nonskip']))
    # Email only when |email_only_change_mode| is False or there
    # is a change in the result compared to the last result.
    simple_rev_str = ''
    if not email_only_change_mode or result_change:
      prev_time_in_float = datetime.strptime(prev_time, '%Y-%m-%d-%H')
      prev_time_in_float = time.mktime(prev_time_in_float.timetuple())
      if debug:
        cur_time_in_float = datetime.strptime(CUR_TIME_FOR_DEBUG,
                                              '%Y-%m-%d-%H')
        cur_time_in_float = time.mktime(cur_time_in_float.timetuple())
      else:
        cur_time_in_float = time.time()
      (rev_str, simple_rev_str, rev, rev_date) = (
          layouttest_analyzer_helpers.GetRevisionString(prev_time_in_float,
                                                        cur_time_in_float,
                                                        diff_map))
      email_content = analyzer_result_map.ConvertToString(prev_time, diff_map,
                                                          anno_map)
      if receiver_email_address:
        layouttest_analyzer_helpers.SendStatusEmail(
            prev_time, analyzer_result_map, diff_map, anno_map,
            receiver_email_address, test_group_name,
            appended_text_to_email, email_content, rev_str)
    if simple_rev_str:
      simple_rev_str = '\'' + simple_rev_str + '\''
    else:
      simple_rev_str = 'undefined'  # GViz uses undefined for NONE.
  else:
    # Initial result should be written to tread-graph if there are no previous
    # results.
    result_change = True
    diff_map = None
    simple_rev_str = 'undefined'
  return (result_change, diff_map, simple_rev_str, rev, rev_date,
          email_content)


def UpdateTrendGraph(start_time, analyzer_result_map, diff_map, simple_rev_str,
                     trend_graph_location):
  """Update trend graph in GViz.

  Annotate the graph with revision information.

  Args:
    start_time: the script starting time as a float value.
    analyzer_result_map: current analyzer result map. Please refer to
        layouttest_analyzer_helpers.AnalyzerResultMap.
    diff_map: a map that has 'whole', 'skip' and 'nonskip' as keys.
        Please refer to |diff_map| in
        |layouttest_analyzer_helpers.SendStatusEmail()|.
    simple_rev_str: a simple version of revision string that is sent in
        the email.
    trend_graph_location: the location of the trend graph that needs to be
        updated.

  Returns:
     a dictionary that maps result data category ('whole', 'skip', 'nonskip',
         'passingrate') to information tuple (a dictionary that maps test name
         to its description, annotation, simple_rev_string) of the given result
         data category. These tuples are used for trend graph update.
  """
  # Trend graph update (if specified in the command-line argument) when
  # there is change from the last result.
  # Currently, there are two graphs (graph1 is for 'whole', 'skip',
  # 'nonskip' and the graph2 is for 'passingrate'). Please refer to
  # graph/graph.html.
  # Sample JS annotation for graph1:
  #   [new Date(2011,8,12,10,41,32),224,undefined,'',52,undefined,
  #    undefined, 12, 'test1,','<a href="http://t</a>,',],
  # This example lists 'whole' triple and 'skip' triple and
  # 'nonskip' triple. Each triple is (the number of tests that belong to
  # the test group, linked text, a link). The following code generates this
  # automatically based on rev_string etc.
  trend_graph = TrendGraph(trend_graph_location)
  datetime_string = start_time.strftime('%Y,%m,%d,%H,%M,%S')
  data_map = {}
  passingrate_anno = ''
  for test_group in ['whole', 'skip', 'nonskip']:
    anno = 'undefined'
    # Extract test description.
    test_map = {}
    for (test_name, value) in (
        analyzer_result_map.result_map[test_group].iteritems()):
      test_map[test_name] = value['desc']
    test_str = ''
    links = ''
    if diff_map and diff_map[test_group]:
      for i in [0, 1]:
        for (name, _) in diff_map[test_group][i]:
          test_str += name + ','
          # This is link to test HTML in WebKit SVN.
          links += ('<a href="http://trac.webkit.org/browser/trunk/'
                    'LayoutTests/%s">%s</a>,') % (name, name)
      if test_str:
        anno = '\'' + test_str + '\''
        # The annotation of passing rate is a union of all annotations.
        passingrate_anno += anno
    if links:
      links = '\'' + links + '\''
    else:
      links = 'undefined'
    if test_group is 'whole':
      data_map[test_group] = (test_map, anno, links)
    else:
      data_map[test_group] = (test_map, anno, simple_rev_str)
  if not passingrate_anno:
    passingrate_anno = 'undefined'
  data_map['passingrate'] = (
      str(analyzer_result_map.GetPassingRate()), passingrate_anno,
      simple_rev_str)
  trend_graph.Update(datetime_string, data_map)
  return data_map


def UpdateDashboard(dashboard_file_location, test_group_name, data_map,
                    layouttest_root_path, rev, rev_date, email,
                    email_content):
  """Update dashboard HTML file.

  Args:
    dashboard_file_location: the file location for the dashboard file.
    test_group_name: please refer to |options|.
    data_map: a dictionary that maps result data category ('whole', 'skip',
        'nonskip', 'passingrate') to information tuple (a dictionary that maps
        test name to its description, annotation, simple_rev_string) of the
        given result data category. These tuples are used for trend graph
        update.
    layouttest_root_path: A location string where Webkit layout tests are
        stored.
    rev: the latest revision number for the given test group.
    rev_date: the latest revision date for the given test group.
    email: email address of the owner for the given test group.
    email_content:  email content string (without |appended_text_to_email|)
        that will be shown on the dashboard.
  """
  # Generate a HTML file that contains all test names for each test group.
  escaped_tg_name = test_group_name.replace('/', '_')
  for tg in ['whole', 'skip', 'nonskip']:
    file_name = os.path.join(
        os.path.dirname(dashboard_file_location),
        escaped_tg_name + '_' + tg + '.html')
    file_object = open(file_name, 'wb')
    file_object.write('<table border="1">')
    sorted_testnames = data_map[tg][0].keys()
    sorted_testnames.sort()
    for testname in sorted_testnames:
      file_object.write(('<tr><td><a href="%s">%s</a></td>'
                         '<td><a href="%s">dashboard</a></td>'
                         '<td>%s</td></tr>') % (
          layouttest_root_path + testname, testname,
          ('http://test-results.appspot.com/dashboards/'
           'flakiness_dashboard.html#tests=%s') % testname,
          data_map[tg][0][testname]))
    file_object.write('</table>')
    file_object.close()
  email_content_with_link = ''
  if email_content:
    file_name = os.path.join(os.path.dirname(dashboard_file_location),
                             escaped_tg_name + '_email.html')
    file_object = open(file_name, 'wb')
    file_object.write(email_content)
    file_object.close()
    email_content_with_link = '<a href="%s_email.html">info</a>' % (
        escaped_tg_name)
  new_str = ('<td><a href="%s">%s</a></td><td><a href="%s">%s</a></td>'
             '<td><a href="%s">%s</a></td><td><a href="%s">%s</a></td>'
             '<td><a href="%s">%s</a></td><td>%d%%</td><td>%s%%</td>'
             '<td><a href="http://trac.webkit.org/changeset/%s">%s</a></td>'
             '<td>%s</td><td><a href="mailto:%s">%s</a></td>'
             '<td>%s</td>\n') % (
                 # Dashboard file and graph must be in the same directory
                 # to make the following link work.
                 layouttest_root_path + '/' + test_group_name,
                 test_group_name, escaped_tg_name + '.html',
                 'graph', escaped_tg_name + '_whole.html',
                 len(data_map['whole'][0]), escaped_tg_name + '_skip.html',
                 len(data_map['skip'][0]), escaped_tg_name + '_nonskip.html',
                 len(data_map['nonskip'][0]),
                 100 - int(data_map['passingrate'][0]),
                 data_map['passingrate'][0], rev, rev, rev_date, email,
                 email, email_content_with_link)
  layouttest_analyzer_helpers.ReplaceLineInFile(
      dashboard_file_location, '<td>' + test_group_name + '</td>', new_str)


def main():
  """A main function for the analyzer."""
  options = ParseOption()
  start_time = datetime.now()

  (prev_time, prev_analyzer_result_map, analyzer_result_map) = (
      GetCurrentAndPreviousResults(options.debug,
                                   options.test_group_file_location,
                                   options.test_group_name,
                                   options.result_directory_location))
  (anno_map, appended_text_to_email) = ReadEmailInformation(
      options.bug_annotation_file_location,
      options.email_appended_text_file_location)
  (result_change, diff_map, simple_rev_str, rev, rev_date, email_content) = (
      SendEmail(prev_time, prev_analyzer_result_map, analyzer_result_map,
                anno_map, appended_text_to_email,
                options.email_only_change_mode, options.debug,
                options.receiver_email_address, options.test_group_name))
  if not options.debug and (result_change or not prev_analyzer_result_map):
    # Save the current result when result is changed or the script is
    # executed for the first time.
    date = start_time.strftime('%Y-%m-%d-%H')
    file_path = os.path.join(options.result_directory_location, date)
    analyzer_result_map.Save(file_path)
  if result_change or not prev_analyzer_result_map:
    data_map = UpdateTrendGraph(start_time, analyzer_result_map, diff_map,
                                simple_rev_str, options.trend_graph_location)
    # Report the result to dashboard.
    if options.dashboard_file_location:
      UpdateDashboard(options.dashboard_file_location, options.test_group_name,
                      data_map, layouttests.DEFAULT_LAYOUTTEST_LOCATION, rev,
                      rev_date, options.receiver_email_address,
                      email_content)


if '__main__' == __name__:
  main()
