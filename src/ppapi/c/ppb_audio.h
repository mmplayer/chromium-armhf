/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_audio.idl modified Mon Aug 29 10:11:34 2011. */

#ifndef PPAPI_C_PPB_AUDIO_H_
#define PPAPI_C_PPB_AUDIO_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_AUDIO_INTERFACE_1_0 "PPB_Audio;1.0"
#define PPB_AUDIO_INTERFACE PPB_AUDIO_INTERFACE_1_0

/**
 * @file
 * This file defines the <code>PPB_Audio</code> interface, which provides
 * realtime stereo audio streaming capabilities.
 */


/**
 * @addtogroup Typedefs
 * @{
 */
/**
 * <code>PPB_Audio_Callback</code> defines the type of an audio callback
 * function used to fill the audio buffer with data. Please see the
 * Create() function in the <code>PPB_Audio</code> interface for
 * more details on this callback.
 */
typedef void (*PPB_Audio_Callback)(void* sample_buffer,
                                   uint32_t buffer_size_in_bytes,
                                   void* user_data);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_Audio</code> interface contains pointers to several functions
 * for handling audio resources. Please refer to the
 * <a href="/chrome/nativeclient/docs/audio.html">Pepper
 * Audio API</a> for information on using this interface.
 * Please see descriptions for each <code>PPB_Audio</code> and
 * <code>PPB_AudioConfig</code> function for more details. A C example using
 * <code>PPB_Audio</code> and <code>PPB_AudioConfig</code> follows.
 *
 * <strong>Example: </strong>
 *
 * <code>
 * void audio_callback(void* sample_buffer,
 *                     uint32_t buffer_size_in_bytes,
 *                     void* user_data) {
 *   ... quickly fill in the buffer with samples and return to caller ...
 *  }
 *
 * ...Assume the application has cached the audio configuration interface in
 * <code>audio_config_interface</code> and the audio interface in
 * <code>audio_interface</code>...
 *
 * uint32_t count = audio_config_interface->RecommendSampleFrameCount(
 *     PP_AUDIOSAMPLERATE_44100, 4096);
 * PP_Resource pp_audio_config = audio_config_interface->CreateStereo16Bit(
 *     pp_instance, PP_AUDIOSAMPLERATE_44100, count);
 * PP_Resource pp_audio = audio_interface->Create(pp_instance, pp_audio_config,
 *     audio_callback, NULL);
 * audio_interface->StartPlayback(pp_audio);
 *
 * ...audio_callback() will now be periodically invoked on a separate thread...
 * </code>
 */
struct PPB_Audio {
  /**
   * Create() creates an audio resource. No sound will be heard until
   * StartPlayback() is called. The callback is called with the buffer address
   * and given user data whenever the buffer needs to be filled. From within the
   * callback, you should not call <code>PPB_Audio</code> functions. The
   * callback will be called on a different thread than the one which created
   * the interface. For performance-critical applications (i.e. low-latency
   * audio), the callback should avoid blocking or calling functions that can
   * obtain locks, such as malloc. The layout and the size of the buffer passed
   * to the audio callback will be determined by the device configuration and is
   * specified in the <code>AudioConfig</code> documentation.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   * @param[in] config A <code>PP_Resource</code> corresponding to an audio
   * config resource.
   * @param[in] audio_callback A <code>PPB_Audio_Callback</code> callback
   * function that the browser calls when it needs more samples to play.
   * @param[in] user_data A pointer to user data used in the callback function.
   *
   * @return A <code>PP_Resource</code> containing the audio resource if
   * successful or 0 if the configuration cannot be honored or the callback is
   * null.
   */
  PP_Resource (*Create)(PP_Instance instance,
                        PP_Resource config,
                        PPB_Audio_Callback audio_callback,
                        void* user_data);
  /**
   * IsAudio() determines if the provided resource is an audio resource.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a generic
   * resource.
   *
   * @return A <code>PP_Bool</code> containing containing <code>PP_TRUE</code>
   * if the given resource is an Audio resource, otherwise
   * <code>PP_FALSE</code>.
   */
  PP_Bool (*IsAudio)(PP_Resource resource);
  /**
   * GetCurrrentConfig() returns an audio config resource for the given audio
   * resource.
   *
   * @param[in] config A <code>PP_Resource</code> corresponding to an audio
   * resource.
   *
   * @return A <code>PP_Resource</code> containing the audio config resource if
   * successful.
   */
  PP_Resource (*GetCurrentConfig)(PP_Resource audio);
  /**
   * StartPlayback() starts the playback of the audio resource and begins
   * periodically calling the callback.
   *
   * @param[in] config A <code>PP_Resource</code> corresponding to an audio
   * resource.
   *
   * @return A <code>PP_Bool</code> containing <code>PP_TRUE</code> if
   * successful, otherwise <code>PP_FALSE</code>. Also returns
   * <code>PP_TRUE</code> (and be a no-op) if called while playback is already
   * in progress.
   */
  PP_Bool (*StartPlayback)(PP_Resource audio);
  /**
   * StopPlayback() stops the playback of the audio resource.
   *
   * @param[in] config A <code>PP_Resource</code> corresponding to an audio
   * resource.
   *
   * @return A <code>PP_Bool</code> containing <code>PP_TRUE</code> if
   * successful, otherwise <code>PP_FALSE</code>. Also returns
   * <code>PP_TRUE</code> (and is a no-op) if called while playback is already
   * stopped. If a callback is in progress, StopPlayback() will block until the
   * callback completes.
   */
  PP_Bool (*StopPlayback)(PP_Resource audio);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_AUDIO_H_ */

