// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_MAIN_PLATFORM_DELEGATE_H_
#define CHROME_RENDERER_RENDERER_MAIN_PLATFORM_DELEGATE_H_
#pragma once

#include "content/common/content_export.h"
#include "content/common/main_function_params.h"

#ifdef __OBJC__
@class NSBundle;
#else
class NSBundle;
#endif  // __OBJC__

class CONTENT_EXPORT RendererMainPlatformDelegate {
 public:
  explicit RendererMainPlatformDelegate(const MainFunctionParams& parameters);
  ~RendererMainPlatformDelegate();

  // Called first thing and last thing in the process' lifecycle, i.e. before
  // the sandbox is enabled.
  void PlatformInitialize();
  void PlatformUninitialize();

  // Gives us an opportunity to initialize state used for tests before enabling
  // the sandbox.
  bool InitSandboxTests(bool no_sandbox);

  // Initiate Lockdown, returns true on success.
  bool EnableSandbox();

  // Runs Sandbox tests.
  void RunSandboxTests();

 private:
  const MainFunctionParams& parameters_;
#if defined(OS_WIN)
  HMODULE sandbox_test_module_;
#elif defined(OS_MACOSX)
  NSBundle* sandbox_tests_bundle_;
#endif

  DISALLOW_COPY_AND_ASSIGN(RendererMainPlatformDelegate);
};

#endif  // CHROME_RENDERER_RENDERER_MAIN_PLATFORM_DELEGATE_H_
