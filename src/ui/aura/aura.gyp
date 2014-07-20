# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'aura',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../skia/skia.gyp:skia',
        '../gfx/compositor/compositor.gyp:compositor',
        '../ui.gyp:gfx_resources',
        '../ui.gyp:ui',
        '../ui.gyp:ui_resources',
      ],
      'defines': [
        'AURA_IMPLEMENTATION',
      ],
      'sources': [
        'cursor.h',
        'desktop_host.h',
        'desktop_host_linux.cc',
        'desktop_host_win.cc',
        'desktop_host_win.h',
        'desktop.cc',
        'desktop.h',
        'desktop_delegate.h',
        'event.cc',
        'event.h',
        'event_filter.cc',
        'event_filter.h',
        'focus_manager.h',
        'hit_test.h',
        'layout_manager.h',
        'root_window.cc',
        'root_window.h',
        'screen_aura.cc',
        'screen_aura.h',
        'toplevel_window_container.cc',
        'toplevel_window_container.h',
        'toplevel_window_event_filter.cc',
        'toplevel_window_event_filter.h',
        'window.cc',
        'window.h',
        'window_delegate.h',
        'window_observer.h',
        'window_types.h',
      ],
    },
    {
      'target_name': 'aura_demo',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../skia/skia.gyp:skia',
        '../../third_party/icu/icu.gyp:icui18n',
        '../../third_party/icu/icu.gyp:icuuc',
        '../gfx/compositor/compositor.gyp:compositor',
        '../ui.gyp:gfx_resources',
        '../ui.gyp:ui',
        '../ui.gyp:ui_resources',
        'aura',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'demo/demo_main.cc',
        '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.rc',
      ],
    },
    {
      'target_name': 'aura_unittests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:test_support_base',
        '../../skia/skia.gyp:skia',
        '../../testing/gtest.gyp:gtest',
        '../gfx/compositor/compositor.gyp:compositor',
        '../gfx/gl/gl.gyp:gl',
        '../ui.gyp:gfx_resources',
        '../ui.gyp:ui',
        '../ui.gyp:ui_resources',
        'aura',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/aura_test_base.cc',
        'test/aura_test_base.h',
        'test/event_generator.cc',
        'test/event_generator.h',
        'test/run_all_unittests.cc',
        'test/test_desktop_delegate.cc',
        'test/test_desktop_delegate.h',
        'test/test_suite.cc',
        'test/test_suite.h',
        'test/test_window_delegate.cc',
        'test/test_window_delegate.h',
        'toplevel_window_event_filter_unittest.cc',
        'window_unittest.cc',
        '../gfx/compositor/test_compositor.cc',
        '../gfx/compositor/test_compositor.h',
        '../gfx/compositor/test_texture.cc',
        '../gfx/compositor/test_texture.h',
        '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.rc',
        '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.rc',
      ],
      # osmesa GL implementation is used on linux.
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '<(DEPTH)/third_party/mesa/mesa.gyp:osmesa',
          ],
        }],
        ['OS!="mac"', {
          'dependencies': [
            '../../chrome/chrome.gyp:packed_resources',
           ],
        }],
      ],
    },
  ],
}
