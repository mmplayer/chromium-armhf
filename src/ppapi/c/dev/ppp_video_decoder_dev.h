/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppp_video_decoder_dev.idl modified Wed Oct  5 15:59:17 2011. */

#ifndef PPAPI_C_DEV_PPP_VIDEO_DECODER_DEV_H_
#define PPAPI_C_DEV_PPP_VIDEO_DECODER_DEV_H_

#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

#define PPP_VIDEODECODER_DEV_INTERFACE_0_9 "PPP_VideoDecoder(Dev);0.9"
#define PPP_VIDEODECODER_DEV_INTERFACE PPP_VIDEODECODER_DEV_INTERFACE_0_9

/**
 * @file
 * This file defines the <code>PPP_VideoDecoder_Dev</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * PPP_VideoDecoder_Dev structure contains the function pointers that the
 * plugin MUST implement to provide services needed by the video decoder
 * implementation.
 *
 * See PPB_VideoDecoder_Dev for general usage tips.
 */
struct PPP_VideoDecoder_Dev {
  /**
   * Callback function to provide buffers for the decoded output pictures. If
   * succeeds plugin must provide buffers through AssignPictureBuffers function
   * to the API. If |req_num_of_bufs| matching exactly the specification
   * given in the parameters cannot be allocated decoder should be destroyed.
   *
   * Decoding will not proceed until buffers have been provided.
   *
   * Parameters:
   *  |instance| the plugin instance to which the callback is responding.
   *  |decoder| the PPB_VideoDecoder_Dev resource.
   *  |req_num_of_bufs| tells how many buffers are needed by the decoder.
   *  |dimensions| tells the dimensions of the buffer to allocate.
   */
  void (*ProvidePictureBuffers)(PP_Instance instance,
                                PP_Resource decoder,
                                uint32_t req_num_of_bufs,
                                const struct PP_Size* dimensions);
  /**
   * Callback function for decoder to deliver unneeded picture buffers back to
   * the plugin.
   *
   * Parameters:
   *  |instance| the plugin instance to which the callback is responding.
   *  |decoder| the PPB_VideoDecoder_Dev resource.
   *  |picture_buffer| points to the picture buffer that is no longer needed.
   */
  void (*DismissPictureBuffer)(PP_Instance instance,
                               PP_Resource decoder,
                               int32_t picture_buffer_id);
  /**
   * Callback function for decoder to deliver decoded pictures ready to be
   * displayed. Decoder expects the plugin to return the buffer back to the
   * decoder through ReusePictureBuffer function in PPB Video Decoder API.
   *
   * Parameters:
   *  |instance| the plugin instance to which the callback is responding.
   *  |decoder| the PPB_VideoDecoder_Dev resource.
   *  |picture| is the picture that is ready.
   */
  void (*PictureReady)(PP_Instance instance,
                       PP_Resource decoder,
                       const struct PP_Picture_Dev* picture);
  /**
   * Callback function to tell the plugin that decoder has decoded end of stream
   * marker and output all the pictures that should be displayed from the
   * stream.
   *
   * Parameters:
   *  |instance| the plugin instance to which the callback is responding.
   *  |decoder| the PPB_VideoDecoder_Dev resource.
   */
  void (*EndOfStream)(PP_Instance instance, PP_Resource decoder);
  /**
   * Error handler callback for decoder to deliver information about detected
   * errors to the plugin.
   *
   * Parameters:
   *  |instance| the plugin instance to which the callback is responding.
   *  |decoder| the PPB_VideoDecoder_Dev resource.
   *  |error| error is the enumeration specifying the error.
   */
  void (*NotifyError)(PP_Instance instance,
                      PP_Resource decoder,
                      PP_VideoDecodeError_Dev error);
};
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPP_VIDEO_DECODER_DEV_H_ */

