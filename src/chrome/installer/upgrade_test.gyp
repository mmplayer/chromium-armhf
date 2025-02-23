# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'version_py': '../../chrome/tools/build/version.py',
    'version_path': '../../chrome/VERSION',
    'lastchange_path': '<(SHARED_INTERMEDIATE_DIR)/build/LASTCHANGE',
    # 'branding_dir' is set in the 'conditions' section at the bottom.
    'msvs_use_common_release': 0,
    'msvs_use_common_linker_extras': 0,
  },
  'includes': [
    '../../build/win_precompile.gypi',
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'alternate_version_generator_lib',
          'type': 'static_library',
          'dependencies': [
            '../chrome.gyp:common_constants',
            '../chrome.gyp:installer_util',
            '../../base/base.gyp:base',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'test/alternate_version_generator.cc',
            'test/alternate_version_generator.h',
            'test/pe_image_resources.cc',
            'test/pe_image_resources.h',
            'test/resource_loader.cc',
            'test/resource_loader.h',
            'test/resource_updater.cc',
            'test/resource_updater.h',
          ],
        },
        {
          'target_name': 'upgrade_test',
          'type': 'executable',
          'dependencies': [
            'alternate_version_generator_lib',
            # This dependency, although correct, results in the mini installer
            # being rebuilt every time upgrade_test is built.  So disable it
            # for now.
            # TODO(grt): fix rules/targets/etc for
            # mini_installer.gyp:mini_installer so that it does no work if
            # nothing has changed, then un-comment this next line:
            # 'mini_installer.gyp:mini_installer',
            '../../base/base.gyp:test_support_base',
            '../../testing/gtest.gyp:gtest',
            '../chrome.gyp:common_constants',
            '../chrome.gyp:installer_util',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'test/run_all_tests.cc',
            'test/upgrade_test.cc',
          ],
        },
        {
          'target_name': 'alternate_version_generator',
          'type': 'executable',
          'dependencies': [
            'alternate_version_generator_lib',
            '../../base/base.gyp:test_support_base',
            '../../testing/gtest.gyp:gtest',
            '../chrome.gyp:common_constants',
            '../chrome.gyp:installer_util',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'test/alternate_version_generator_main.cc',
          ],
        },
      ],
    }],
    [ 'branding == "Chrome"', {
      'variables': {
         'branding_dir': '../app/theme/google_chrome',
      },
    }, { # else branding!="Chrome"
      'variables': {
         'branding_dir': '../app/theme/chromium',
      },
    }],
  ],
}
