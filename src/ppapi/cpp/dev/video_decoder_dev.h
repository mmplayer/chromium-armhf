// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_VIDEO_DECODER_DEV_H_
#define PPAPI_CPP_DEV_VIDEO_DECODER_DEV_H_

#include <vector>

#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class Context3D_Dev;
class Graphics3D;
class Instance;

// C++ wrapper for the Pepper Video Decoder interface. For more detailed
// documentation refer to the C interfaces.
//
// C++ version of the PPB_VideoDecoder_Dev interface.
class VideoDecoder_Dev : public Resource {
 public:
  // See PPB_VideoDecoder_Dev::Create.
  VideoDecoder_Dev(const Instance* instance,
                   const Context3D_Dev& context,
                   PP_VideoDecoder_Profile profile);
  VideoDecoder_Dev(const Instance* instance,
                   const Graphics3D& context,
                   PP_VideoDecoder_Profile profile);

  explicit VideoDecoder_Dev(PP_Resource resource);
  virtual ~VideoDecoder_Dev();

  // PPB_VideoDecoder_Dev implementation.
  void AssignPictureBuffers(const std::vector<PP_PictureBuffer_Dev>& buffers);
  int32_t Decode(const PP_VideoBitstreamBuffer_Dev& bitstream_buffer,
                 CompletionCallback callback);
  void ReusePictureBuffer(int32_t picture_buffer_id);
  int32_t Flush(CompletionCallback callback);
  int32_t Reset(CompletionCallback callback);

 private:
  // Disallow copy-construction to ensure Destroy() is called exactly once.
  VideoDecoder_Dev(const VideoDecoder_Dev&);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_VIDEO_DECODER_DEV_H_
