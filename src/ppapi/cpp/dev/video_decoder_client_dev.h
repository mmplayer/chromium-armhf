// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_VIDEO_DECODER_CLIENT_DEV_H_
#define PPAPI_CPP_DEV_VIDEO_DECODER_CLIENT_DEV_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/dev/pp_video_dev.h"

namespace pp {

class Instance;
class VideoDecoder_Dev;

// This class provides a C++ interface for callbacks related to video decoding.
// It is the C++ counterpart to PPP_VideoDecoder_Dev.
// You would normally use multiple inheritance to derive from this class in your
// instance.
class VideoDecoderClient_Dev {
 public:
  VideoDecoderClient_Dev(Instance* instance);
  virtual ~VideoDecoderClient_Dev();

  // Callback to provide buffers for the decoded output pictures.
  virtual void ProvidePictureBuffers(PP_Resource decoder,
                                     uint32_t req_num_of_bufs,
                                     const PP_Size& dimensions) = 0;

  // Callback for decoder to deliver unneeded picture buffers back to the
  // plugin.
  virtual void DismissPictureBuffer(PP_Resource decoder,
                                    int32_t picture_buffer_id) = 0;

  // Callback to deliver decoded pictures ready to be displayed.
  virtual void PictureReady(PP_Resource decoder,
                            const PP_Picture_Dev& picture) = 0;

  // Callback to notify that decoder has decoded end of stream marker and has
  // outputted all displayable pictures.
  virtual void EndOfStream(PP_Resource decoder) = 0;

  // Callback to notify about decoding errors.
  virtual void NotifyError(PP_Resource decoder,
                           PP_VideoDecodeError_Dev error) = 0;

 private:
  Instance* associated_instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_VIDEO_DECODER_CLIENT_DEV_H_
