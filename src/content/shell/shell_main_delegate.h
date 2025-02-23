// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_MAIN_DELEGATE_H_
#define CONTENT_SHELL_SHELL_MAIN_DELEGATE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/app/content_main_delegate.h"
#include "content/shell/shell_content_client.h"

namespace content {
class ShellContentBrowserClient;
class ShellContentRendererClient;
class ShellContentPluginClient;
class ShellContentUtilityClient;
}  // namespace content

class ShellMainDelegate : public content::ContentMainDelegate {
 public:
  ShellMainDelegate();
  virtual ~ShellMainDelegate();

  virtual void PreSandboxStartup() OVERRIDE;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  virtual void ZygoteForked() OVERRIDE;
#endif

 private:
  void InitializeShellContentClient(const std::string& process_type);

  scoped_ptr<content::ShellContentBrowserClient> browser_client_;
  scoped_ptr<content::ShellContentRendererClient> renderer_client_;
  scoped_ptr<content::ShellContentPluginClient> plugin_client_;
  scoped_ptr<content::ShellContentUtilityClient> utility_client_;
  content::ShellContentClient content_client_;

  DISALLOW_COPY_AND_ASSIGN(ShellMainDelegate);
};

#endif  // CONTENT_SHELL_SHELL_MAIN_DELEGATE_H_
