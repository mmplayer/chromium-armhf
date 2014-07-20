# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
     {
      'target_name': 'ppapi_example',
      'dependencies': [
        'ppapi.gyp:ppapi_cpp'
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'example/Info.plist',
      },
      'sources': [
        'example/example.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'type': 'shared_library',
          'sources': [
            'example/example.rc',
          ],
          'run_as': {
            'action': [
              '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)chrome<(EXECUTABLE_SUFFIX)',
              '--register-pepper-plugins=$(TargetPath);application/x-ppapi-example',
              'file://$(ProjectDir)/example/example.html',
            ],
          },
        }],
        ['os_posix == 1 and OS != "mac"', {
          'type': 'shared_library',
          'cflags': ['-fvisibility=hidden'],
          # -gstabs, used in the official builds, causes an ICE. Simply remove
          # it.
          'cflags!': ['-gstabs'],
        }],
        ['OS=="mac"', {
          'type': 'loadable_module',
          'mac_bundle': 1,
          'product_name': 'PPAPIExample',
          'product_extension': 'plugin',
          'sources+': [
            'example/Info.plist'
          ],
        }],
      ],
      # See README for instructions on how to run and debug on the Mac.
      #'conditions' : [
      #  ['OS=="mac"', {
      #    'target_name' : 'Chromium',
      #    'type' : 'executable',
      #    'xcode_settings' : {
      #      'ARGUMENTS' : '--renderer-startup-dialog --internal-pepper --no-sandbox file://${SRCROOT}/test_page.html'
      #    },
      #  }],
      #],
    },
    {
      'target_name': 'ppapi_tests',
      'type': 'loadable_module',
      'include_dirs': [
        'lib/gl/include',
      ],
      'sources': [
        # Common test files.
        'tests/test_case.cc',
        'tests/test_case.h',
        'tests/testing_instance.cc',
        'tests/testing_instance.h',

        # Test cases.
        'tests/all_c_includes.h',
        'tests/all_cpp_includes.h',
        'tests/arch_dependent_sizes_32.h',
        'tests/arch_dependent_sizes_64.h',
        'tests/pp_thread.h',
        'tests/test_broker.cc',
        'tests/test_broker.h',
        'tests/test_buffer.cc',
        'tests/test_buffer.h',
        'tests/test_c_includes.c',
        'tests/test_char_set.cc',
        'tests/test_char_set.h',
        'tests/test_core.cc',
        'tests/test_core.h',
        'tests/test_crypto.cc',
        'tests/test_crypto.h',
        'tests/test_cpp_includes.cc',
        'tests/test_cursor_control.cc',
        'tests/test_cursor_control.h',
        'tests/test_directory_reader.cc',
        'tests/test_directory_reader.h',
        'tests/test_fullscreen.cc',
        'tests/test_fullscreen.h',
        'tests/test_flash_fullscreen.cc',
        'tests/test_flash_fullscreen.h',
        'tests/test_file_io.cc',
        'tests/test_file_io.h',
        'tests/test_file_ref.cc',
        'tests/test_file_ref.h',
        'tests/test_file_system.cc',
        'tests/test_file_system.h',
        'tests/test_graphics_2d.cc',
        'tests/test_graphics_2d.h',
        'tests/test_graphics_3d.cc',
        'tests/test_graphics_3d.h',
        'tests/test_image_data.cc',
        'tests/test_image_data.h',
        'tests/test_memory.cc',
        'tests/test_memory.h',
        'tests/test_paint_aggregator.cc',
        'tests/test_paint_aggregator.h',
        'tests/test_post_message.cc',
        'tests/test_post_message.h',
        'tests/test_query_policy.cc',
        'tests/test_query_policy.h',
        'tests/test_scrollbar.cc',
        'tests/test_scrollbar.h',
        'tests/test_struct_sizes.c',
        'tests/test_uma.cc',
        'tests/test_uma.h',
        'tests/test_url_loader.cc',
        'tests/test_url_loader.h',
        'tests/test_url_util.cc',
        'tests/test_url_util.h',
        'tests/test_utils.cc',
        'tests/test_utils.h',
        'tests/test_var.cc',
        'tests/test_var.h',
        'tests/test_video_decoder.cc',
        'tests/test_video_decoder.h',

        # Deprecated test cases.
        'tests/test_instance_deprecated.cc',
        'tests/test_instance_deprecated.h',
        'tests/test_var_deprecated.cc',
        'tests/test_var_deprecated.h',
      ],
      'dependencies': [
        'ppapi.gyp:ppapi_cpp'
      ],
      'run_as': {
        'action': [
          '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)chrome<(EXECUTABLE_SUFFIX)',
          '--enable-pepper-testing',
          '--enable-accelerated-plugins',
          '--register-pepper-plugins=$(TargetPath);application/x-ppapi-tests',
          'file://$(ProjectDir)/tests/test_case.html?testcase=',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'defines': [
            '_CRT_SECURE_NO_DEPRECATE',
            '_CRT_NONSTDC_NO_WARNINGS',
            '_CRT_NONSTDC_NO_DEPRECATE',
            '_SCL_SECURE_NO_DEPRECATE',
          ],
        }],
        ['OS=="mac"', {
          'mac_bundle': 1,
          'product_name': 'ppapi_tests',
          'product_extension': 'plugin',
        }],
        ['p2p_apis==1', {
          'sources': [
            'tests/test_transport.cc',
            'tests/test_transport.h',
          ],
        }],
      ],
# TODO(dmichael):  Figure out what is wrong with the script on Windows and add
#                  it as an automated action.
#      'actions': [
#        {
#          'action_name': 'generate_ppapi_include_tests',
#          'inputs': [],
#          'outputs': [
#            'tests/test_c_includes.c',
#            'tests/test_cc_includes.cc',
#          ],
#          'action': [
#            '<!@(python generate_ppapi_include_tests.py)',
#          ],
#        },
#      ],
    },
    {
      'target_name': 'ppapi_unittests',
      'type': 'executable',
      'variables': {
        'chromium_code': 1,
      },
      'dependencies': [
        'ppapi_proxy',
        'ppapi_shared',
        '../base/base.gyp:test_support_base',
        '../gpu/gpu.gyp:gpu_ipc',
        '../ipc/ipc.gyp:ipc',
        '../ipc/ipc.gyp:test_support_ipc',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../ui/gfx/surface/surface.gyp:surface',
      ],
      'sources': [
        'proxy/run_all_unittests.cc',

        'proxy/mock_resource.cc',
        'proxy/mock_resource.h',
        'proxy/plugin_dispatcher_unittest.cc',
        'proxy/plugin_resource_tracker_unittest.cc',
        'proxy/plugin_var_tracker_unittest.cc',
        'proxy/ppapi_proxy_test.cc',
        'proxy/ppapi_proxy_test.h',
        'proxy/ppb_var_unittest.cc',
        'proxy/ppp_instance_private_proxy_unittest.cc',
        'proxy/ppp_instance_proxy_unittest.cc',
        'proxy/ppp_messaging_proxy_unittest.cc',
        'proxy/serialized_var_unittest.cc',
        'shared_impl/resource_tracker_unittest.cc',
      ],
    },
    {
      'target_name': 'ppapi_example_skeleton',
      'suppress_wildcard': 1,
      'type': 'none',
      'direct_dependent_settings': {
        'product_name': '>(_target_name)',
        'conditions': [
          ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris" or OS=="android"', {
            'cflags': ['-fvisibility=hidden'],
            'type': 'shared_library',
            # -gstabs, used in the official builds, causes an ICE. Simply remove
            # it.
            'cflags!': ['-gstabs'],
          }],
          ['OS=="win"', {
            'type': 'shared_library',
          }],
          ['OS=="mac"', {
            'type': 'loadable_module',
          }],
        ],
      },
    },
    {
      'target_name': 'ppapi_example_mouse_lock',
      'dependencies': [
        'ppapi_example_skeleton',
        'ppapi.gyp:ppapi_cpp',
      ],
      'sources': [
        'examples/mouse_lock/mouse_lock.cc',
      ],
    },
  ],

  'conditions': [
    # NOTE: the PPAPI examples fail to build on mac & windows.
    # http://code.google.com/p/chromium/issues/detail?id=54005 tracks mac.
    ['OS!="mac"', {
      'targets': [
        {
          'target_name': 'ppapi_example_c_stub',
          'dependencies': [
            'ppapi_example_skeleton',
            'ppapi.gyp:ppapi_c',
          ],
          'sources': [
            'examples/stub/stub.c',
          ],
        },
        {
          'target_name': 'ppapi_example_cc_stub',
          'dependencies': [
            'ppapi_example_skeleton',
            'ppapi.gyp:ppapi_cpp',
          ],
          'sources': [
            'examples/stub/stub.cc',
          ],
        },
        {
          'target_name': 'ppapi_example_audio',
          'dependencies': [
            'ppapi_example_skeleton',
            'ppapi.gyp:ppapi_cpp',
          ],
          'sources': [
            'examples/audio/audio.cc',
          ],
        },
        {
          'target_name': 'ppapi_example_file_chooser',
          'dependencies': [
            'ppapi_example_skeleton',
            'ppapi.gyp:ppapi_cpp',
          ],
          'sources': [
            'examples/file_chooser/file_chooser.cc',
          ],
        },
        {
          'target_name': 'ppapi_example_graphics_2d',
          'dependencies': [
            'ppapi_example_skeleton',
            'ppapi.gyp:ppapi_c',
          ],
          'sources': [
            'examples/2d/graphics_2d_example.c',
          ],
        },
        {
          'target_name': 'ppapi_example_ime',
          'dependencies': [
            'ppapi_example_skeleton',
            'ppapi.gyp:ppapi_cpp',
          ],
          'sources': [
            'examples/ime/ime.cc',
          ],
        },
        {
          'target_name': 'ppapi_example_paint_manager',
          'dependencies': [
            'ppapi_example_skeleton',
            'ppapi.gyp:ppapi_cpp',
          ],
          'sources': [
            'examples/2d/paint_manager_example.cc',
          ],
        },
        {
          'target_name': 'ppapi_example_post_message',
          'dependencies': [
            'ppapi_example_skeleton',
            'ppapi.gyp:ppapi_cpp',
          ],
          'sources': [
            'examples/scripting/post_message.cc',
          ],
        },
        {
          'target_name': 'ppapi_example_scroll',
          'dependencies': [
            'ppapi_example_skeleton',
            'ppapi.gyp:ppapi_cpp',
          ],
          'sources': [
            'examples/2d/scroll.cc',
          ],
        },
        {
          'target_name': 'ppapi_example_simple_font',
          'dependencies': [
            'ppapi_example_skeleton',
            'ppapi.gyp:ppapi_cpp',
          ],
          'sources': [
            'examples/font/simple_font.cc',
          ],
        },
        {
          'target_name': 'ppapi_example_url_loader',
          'dependencies': [
            'ppapi_example_skeleton',
            'ppapi.gyp:ppapi_cpp',
          ],
          'sources': [
            'examples/url_loader/streaming.cc',
          ],
        },
        {
          'target_name': 'ppapi_example_gles2',
          'dependencies': [
            'ppapi_example_skeleton',
            'ppapi.gyp:ppapi_cpp',
            'ppapi.gyp:ppapi_gles2',
            'ppapi.gyp:ppapi_egl',
          ],
          'include_dirs': [
            'lib/gl/include',
          ],
          'sources': [
            'examples/gles2/gles2.cc',
            'examples/gles2/testdata.h',
          ],
        },
        {
          'target_name': 'ppapi_example_vc',
          'dependencies': [
            'ppapi_example_skeleton',
            'ppapi.gyp:ppapi_cpp',
            'ppapi.gyp:ppapi_gles2',
            'ppapi.gyp:ppapi_egl',
          ],
          'include_dirs': [
            'lib/gl/include',
          ],
          'sources': [
            'examples/video_capture/video_capture.cc',
          ],
        },
      ],
    }]
  ]
}
