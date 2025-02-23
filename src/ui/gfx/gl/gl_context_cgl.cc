// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_context_cgl.h"

#include <vector>

#include "base/logging.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_surface_cgl.h"

namespace gfx {

GLContextCGL::GLContextCGL(GLShareGroup* share_group)
  : GLContext(share_group),
    context_(NULL),
    gpu_preference_(PreferIntegratedGpu) {
}

GLContextCGL::~GLContextCGL() {
  Destroy();
}

bool GLContextCGL::Initialize(
    GLSurface* compatible_surface, GpuPreference gpu_preference) {
  DCHECK(compatible_surface);

  GLContextCGL* share_context = share_group() ?
      static_cast<GLContextCGL*>(share_group()->GetContext()) : NULL;
  if (SupportsDualGpus()) {
    // Ensure the GPU preference is compatible with contexts already in the
    // share group.
    if (share_context && gpu_preference != share_context->GetGpuPreference())
      return false;
  }

  std::vector<CGLPixelFormatAttribute> attribs;
  attribs.push_back(kCGLPFAPBuffer);
  bool using_offline_renderer =
      SupportsDualGpus() && gpu_preference == PreferIntegratedGpu;
  if (using_offline_renderer) {
    attribs.push_back(kCGLPFAAllowOfflineRenderers);
  }
  attribs.push_back((CGLPixelFormatAttribute) 0);

  CGLPixelFormatObj format;
  GLint num_pixel_formats;
  if (CGLChoosePixelFormat(&attribs.front(),
                           &format,
                           &num_pixel_formats) != kCGLNoError) {
    LOG(ERROR) << "Error choosing pixel format.";
    return false;
  }
  if (!format) {
    LOG(ERROR) << "format == 0.";
    return false;
  }
  DCHECK_NE(num_pixel_formats, 0);

  CGLError res = CGLCreateContext(
      format,
      share_context ?
          static_cast<CGLContextObj>(share_context->GetHandle()) : NULL,
      reinterpret_cast<CGLContextObj*>(&context_));
  CGLReleasePixelFormat(format);
  if (res != kCGLNoError) {
    LOG(ERROR) << "Error creating context.";
    Destroy();
    return false;
  }

  gpu_preference_ = gpu_preference;
  return true;
}

void GLContextCGL::Destroy() {
  if (context_) {
    CGLDestroyContext(static_cast<CGLContextObj>(context_));
    context_ = NULL;
  }
}

bool GLContextCGL::MakeCurrent(GLSurface* surface) {
  DCHECK(context_);
  if (IsCurrent(surface))
    return true;

  if (CGLSetPBuffer(static_cast<CGLContextObj>(context_),
                    static_cast<CGLPBufferObj>(surface->GetHandle()),
                    0,
                    0,
                    0) != kCGLNoError) {
    LOG(ERROR) << "Error attaching pbuffer to context.";
    Destroy();
    return false;
  }

  if (CGLSetCurrentContext(
      static_cast<CGLContextObj>(context_)) != kCGLNoError) {
    LOG(ERROR) << "Unable to make gl context current.";
    return false;
  }

  SetCurrent(this, surface);
  if (!surface->OnMakeCurrent(this)) {
    LOG(ERROR) << "Unable to make gl context current.";
    return false;
  }

  return true;
}

void GLContextCGL::ReleaseCurrent(GLSurface* surface) {
  if (!IsCurrent(surface))
    return;

  SetCurrent(NULL, NULL);
  CGLSetCurrentContext(NULL);
  CGLSetPBuffer(static_cast<CGLContextObj>(context_), NULL, 0, 0, 0);
}

bool GLContextCGL::IsCurrent(GLSurface* surface) {
  bool native_context_is_current = CGLGetCurrentContext() == context_;

  // If our context is current then our notion of which GLContext is
  // current must be correct. On the other hand, third-party code
  // using OpenGL might change the current context.
  DCHECK(!native_context_is_current || (GetCurrent() == this));

  if (!native_context_is_current)
    return false;

  if (surface) {
    CGLPBufferObj current_surface = NULL;
    GLenum face;
    GLint level;
    GLint screen;
    CGLGetPBuffer(static_cast<CGLContextObj>(context_),
                  &current_surface,
                  &face,
                  &level,
                  &screen);
    if (current_surface != surface->GetHandle())
      return false;
  }

  return true;
}

void* GLContextCGL::GetHandle() {
  return context_;
}

void GLContextCGL::SetSwapInterval(int interval) {
  DCHECK(IsCurrent(NULL));
  LOG(WARNING) << "GLContex: GLContextCGL::SetSwapInterval is ignored.";
}

GpuPreference GLContextCGL::GetGpuPreference() {
  return gpu_preference_;
}

}  // namespace gfx
