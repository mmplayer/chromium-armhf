// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ui/gfx/gl/gl_context.h"

namespace gfx {

class GLSurface;

// Encapsulates a GLX OpenGL context.
class GLContextGLX : public GLContext {
 public:
  explicit GLContextGLX(GLShareGroup* share_group);
  virtual ~GLContextGLX();

  // Implement GLContext.
  virtual bool Initialize(
      GLSurface* compatible_surface, GpuPreference gpu_preference);
  virtual void Destroy();
  virtual bool MakeCurrent(GLSurface* surface);
  virtual void ReleaseCurrent(GLSurface* surface);
  virtual bool IsCurrent(GLSurface* surface);
  virtual void* GetHandle();
  virtual void SetSwapInterval(int interval);
  virtual std::string GetExtensions();
  virtual bool WasAllocatedUsingARBRobustness();

 private:
  void* context_;

  DISALLOW_COPY_AND_ASSIGN(GLContextGLX);
};

}  // namespace gfx
