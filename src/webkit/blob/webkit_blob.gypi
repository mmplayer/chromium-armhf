# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'blob',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/net/net.gyp:net',
      ],
      'sources': [
        'blob_data.cc',
        'blob_data.h',
        'blob_storage_controller.cc',
        'blob_storage_controller.h',
        'blob_url_request_job.cc',
        'blob_url_request_job.h',
        'blob_url_request_job_factory.cc',
        'blob_url_request_job_factory.h',
        'deletable_file_reference.cc',
        'deletable_file_reference.h',
        'view_blob_internals_job.cc',
        'view_blob_internals_job.h',
      ],
      'conditions': [
        ['inside_chromium_build==0', {
          'dependencies': [
            '<(DEPTH)/webkit/support/setup_third_party.gyp:third_party_headers',
          ],
        }],
      ],
    },
  ],
}
