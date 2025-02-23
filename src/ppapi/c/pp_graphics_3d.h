/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PPAPI_C_PP_GRAPHICS_3D_H_
#define PPAPI_C_PP_GRAPHICS_3D_H_

#include "ppapi/c/pp_stdint.h"

enum PP_Graphics3DAttrib {
  // Bits of Alpha in the color buffer.
  PP_GRAPHICS3DATTRIB_ALPHA_SIZE = 0x3021,
  // Bits of Blue in the color buffer.
  PP_GRAPHICS3DATTRIB_BLUE_SIZE = 0x3022,
  // Bits of Green in the color buffer.
  PP_GRAPHICS3DATTRIB_GREEN_SIZE = 0x3023,
  // Bits of Red in the color buffer.
  PP_GRAPHICS3DATTRIB_RED_SIZE = 0x3024,
  // Bits of Z in the depth buffer.
  PP_GRAPHICS3DATTRIB_DEPTH_SIZE = 0x3025,
  // Bits of Stencil in the stencil buffer.
  PP_GRAPHICS3DATTRIB_STENCIL_SIZE = 0x3026,
  // Number of samples per pixel.
  PP_GRAPHICS3DATTRIB_SAMPLES = 0x3031,
  // Number of multisample buffers.
  PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS = 0x3032,
  // Attrib list terminator.
  PP_GRAPHICS3DATTRIB_NONE = 0x3038,
  // Height of surface in pixels.
  PP_GRAPHICS3DATTRIB_HEIGHT = 0x3056,
  // Width of surface in pixels.
  PP_GRAPHICS3DATTRIB_WIDTH = 0x3057,

  // Specifies the effect on the color buffer of posting a surface
  // with SwapBuffers. The initial value is chosen by the implementation.
  PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR = 0x3093,
  // Indicates that color buffer contents are unaffected.
  PP_GRAPHICS3DATTRIB_BUFFER_PRESERVED = 0x3094,
  // Indicates that color buffer contents may be destroyed or changed.
  PP_GRAPHICS3DATTRIB_BUFFER_DESTROYED = 0x3095
};

// TODO(alokp): Remove this when PPB_Context3D_Dev and PPB_Surface3D_Dev
// are deprecated.
typedef int32_t PP_Config3D_Dev;

#endif  // PPAPI_C_PP_GRAPHICS_3D_H_
