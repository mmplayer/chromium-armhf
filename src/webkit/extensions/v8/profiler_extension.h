// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_EXTENSIONS_V8_PROFILER_EXTENSION_H_
#define WEBKIT_EXTENSIONS_V8_PROFILER_EXTENSION_H_
#pragma once

namespace v8 {
class Extension;
}

namespace extensions_v8 {

// Profiler is an extension to allow javascript access to the API for
// an external profiler program (such as Quantify). The "External" part of the
// name is to distinguish it from the built-in V8 Profiler.
class ProfilerExtension {
 public:
  static v8::Extension* Get();
};

}  // namespace extensions_v8

#endif  // WEBKIT_EXTENSIONS_V8_PROFILER_EXTENSION_H_
