# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../base/base.gyp:base',
  ],
  'sources': [
    'public/utility/content_utility_client.h',
    'public/utility/utility_thread.cc',
    'public/utility/utility_thread.h',
    'utility/utility_main.cc',
    'utility/utility_thread_impl.cc',
    'utility/utility_thread_impl.h',
  ],
  'include_dirs': [
    '..',
  ],
  'conditions': [
    ['OS=="mac"', {
      'link_settings': {
        'mac_bundle_resources': [
          'utility/utility.sb',
        ],
      },
    }],
  ],
}
