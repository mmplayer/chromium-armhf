# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../base/base.gyp:base',
    '../skia/skia.gyp:skia',
    '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
  ],
  'sources': [
    'worker/shared_worker_devtools_agent.cc',
    'worker/shared_worker_devtools_agent.h',
    'worker/websharedworker_stub.cc',
    'worker/websharedworker_stub.h',
    'worker/webworker_stub_base.cc',
    'worker/webworker_stub_base.h',
    'worker/webworker_stub.cc',
    'worker/webworker_stub.h',
    'worker/webworkerclient_proxy.cc',
    'worker/webworkerclient_proxy.h',
    'worker/worker_main.cc',
    'worker/worker_thread.cc',
    'worker/worker_thread.h',
    'worker/worker_webapplicationcachehost_impl.cc',
    'worker/worker_webapplicationcachehost_impl.h',
    'worker/worker_webkitplatformsupport_impl.cc',
    'worker/worker_webkitplatformsupport_impl.h',
  ],
  'include_dirs': [
    '..',
  ],
}
