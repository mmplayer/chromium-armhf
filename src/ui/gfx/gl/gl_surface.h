// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GL_GL_SURFACE_H_
#define UI_GFX_GL_GL_SURFACE_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "ui/gfx/gl/gl_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

namespace gfx {

class GLContext;

// Encapsulates a surface that can be rendered to with GL, hiding platform
// specific management.
class GL_EXPORT GLSurface : public base::RefCounted<GLSurface> {
 public:
  GLSurface();

  // (Re)create the surface. TODO(apatrick): This is an ugly hack to allow the
  // EGL surface associated to be recreated without destroying the associated
  // context. The implementation of this function for other GLSurface derived
  // classes is in a pending changelist.
  virtual bool Initialize();

  // Destroys the surface.
  virtual void Destroy() = 0;

  // Returns true if this surface is offscreen.
  virtual bool IsOffscreen() = 0;

  // Swaps front and back buffers. This has no effect for off-screen
  // contexts.
  virtual bool SwapBuffers() = 0;

  // Get the size of the surface.
  virtual gfx::Size GetSize() = 0;

  // Get the underlying platform specific surface "handle".
  virtual void* GetHandle() = 0;

  // Returns the internal frame buffer object name if the surface is backed by
  // FBO. Otherwise returns 0.
  virtual unsigned int GetBackingFrameBufferObject();

  static bool InitializeOneOff();

  // Called after a context is made current with this surface. Returns false
  // on error.
  virtual bool OnMakeCurrent(GLContext* context);

  virtual void SetVisible(bool visible);

  // Create a GL surface that renders directly to a view.
  static scoped_refptr<GLSurface> CreateViewGLSurface(
      bool software,
      gfx::PluginWindowHandle window);

  // Create a GL surface used for offscreen rendering.
  static scoped_refptr<GLSurface> CreateOffscreenGLSurface(
      bool software,
      const gfx::Size& size);

  static GLSurface* GetCurrent();

 protected:
  virtual ~GLSurface();
  static void SetCurrent(GLSurface* surface);

 private:
  friend class base::RefCounted<GLSurface>;
  friend class GLContext;
  DISALLOW_COPY_AND_ASSIGN(GLSurface);
};

}  // namespace gfx

#endif  // UI_GFX_GL_GL_SURFACE_H_
