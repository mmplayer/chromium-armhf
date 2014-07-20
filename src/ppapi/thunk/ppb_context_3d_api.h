// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_CONTEXT_3D_API_H_
#define PPAPI_THUNK_PPB_CONTEXT_3D_API_H_

#include "ppapi/c/dev/ppb_context_3d_dev.h"
#include "ppapi/c/dev/ppb_context_3d_trusted_dev.h"
#include "ppapi/c/dev/ppb_gles_chromium_texture_mapping_dev.h"

namespace gpu {
namespace gles2 {
class GLES2Implementation;
}  // namespace gles2
}  // namespace gpu

namespace ppapi {
namespace thunk {

class PPB_Context3D_API {
 public:
  virtual ~PPB_Context3D_API() {}

  // Context3D.
  virtual int32_t GetAttrib(int32_t attribute, int32_t* value) = 0;
  virtual int32_t BindSurfaces(PP_Resource draw, PP_Resource read) = 0;
  virtual int32_t GetBoundSurfaces(PP_Resource* draw, PP_Resource* read) = 0;

  // Context3DTrusted.
  virtual PP_Bool InitializeTrusted(int32_t size) = 0;
  virtual PP_Bool GetRingBuffer(int* shm_handle,
                                uint32_t* shm_size) = 0;
  virtual PP_Context3DTrustedState GetState() = 0;
  virtual PP_Bool Flush(int32_t put_offset) = 0;
  virtual PP_Context3DTrustedState FlushSync(int32_t put_offset) = 0;
  virtual int32_t CreateTransferBuffer(uint32_t size) = 0;
  virtual PP_Bool DestroyTransferBuffer(int32_t id) = 0;
  virtual PP_Bool GetTransferBuffer(int32_t id,
                                    int* shm_handle,
                                    uint32_t* shm_size) = 0;
  virtual PP_Context3DTrustedState FlushSyncFast(int32_t put_offset,
                                                 int32_t last_known_get) = 0;

  // GLESChromiumTextureMapping.
  virtual void* MapTexSubImage2DCHROMIUM(GLenum target,
                                         GLint level,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLsizei width,
                                         GLsizei height,
                                         GLenum format,
                                         GLenum type,
                                         GLenum access) = 0;
  virtual void UnmapTexSubImage2DCHROMIUM(const void* mem) = 0;

  // For binding with OpenGLES interface.
  virtual gpu::gles2::GLES2Implementation* GetGLES2Impl() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_CONTEXT_3D_API_H_
