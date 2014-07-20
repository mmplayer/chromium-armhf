// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GL_GL_SURFACE_EGL_H_
#define UI_GFX_GL_GL_SURFACE_EGL_H_
#pragma once

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "ui/gfx/gl/gl_surface.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

typedef void* EGLConfig;
typedef void* EGLDisplay;
typedef void* EGLSurface;

#if defined(OS_WIN)
typedef HDC EGLNativeDisplayType;
#elif defined(USE_WAYLAND)
typedef struct wl_display* EGLNativeDisplayType;
#else
typedef struct _XDisplay* EGLNativeDisplayType;
#endif

namespace gfx {

// Interface for EGL surface.
class GL_EXPORT GLSurfaceEGL : public GLSurface {
 public:
  GLSurfaceEGL();
  virtual ~GLSurfaceEGL();

  static bool InitializeOneOff();
  EGLDisplay GetDisplay();
  EGLConfig GetConfig();
  static EGLDisplay GetHardwareDisplay();
  static EGLDisplay GetSoftwareDisplay();
  static EGLNativeDisplayType GetNativeDisplay();

protected:
  bool software_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GLSurfaceEGL);
};

// Encapsulates an EGL surface bound to a view.
class NativeViewGLSurfaceEGL : public GLSurfaceEGL {
 public:
  NativeViewGLSurfaceEGL(bool software, gfx::PluginWindowHandle window);
  virtual ~NativeViewGLSurfaceEGL();

  // Implement GLSurface.
  virtual bool Initialize();
  virtual void Destroy();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual EGLSurface GetHandle();

 private:
  gfx::PluginWindowHandle window_;
  EGLSurface surface_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewGLSurfaceEGL);
};

// Encapsulates a pbuffer EGL surface.
class GL_EXPORT PbufferGLSurfaceEGL : public GLSurfaceEGL {
 public:
  PbufferGLSurfaceEGL(bool software, const gfx::Size& size);
  virtual ~PbufferGLSurfaceEGL();

  // Implement GLSurface.
  virtual bool Initialize();
  virtual void Destroy();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual EGLSurface GetHandle();

 private:
  gfx::Size size_;
  EGLSurface surface_;

  DISALLOW_COPY_AND_ASSIGN(PbufferGLSurfaceEGL);
};

}  // namespace gfx

#endif  // UI_GFX_GL_GL_SURFACE_EGL_H_
