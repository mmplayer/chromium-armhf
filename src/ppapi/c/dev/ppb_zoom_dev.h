/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_zoom_dev.idl modified Fri Aug 26 15:06:04 2011. */

#ifndef PPAPI_C_DEV_PPB_ZOOM_DEV_H_
#define PPAPI_C_DEV_PPB_ZOOM_DEV_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_ZOOM_DEV_INTERFACE_0_2 "PPB_Zoom(Dev);0.2"
#define PPB_ZOOM_DEV_INTERFACE PPB_ZOOM_DEV_INTERFACE_0_2

/**
 * @file
 * Implementation of the Zoom interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * Zoom interface should only apply to those full-page "plugin-document".
 */
struct PPB_Zoom_Dev {
  /**
   * Informs the browser about the new zoom factor for the plugin (see
   * ppp_zoom_dev.h for a description of zoom factor). The plugin should only
   * call this function if the zoom change was triggered by the browser, it's
   * only needed in case a plugin can update its own zoom, say because of its
   * own UI.
   */
  void (*ZoomChanged)(PP_Instance instance, double factor);
  /**
   * Sets the mininum and maximium zoom factors.
   */
  void (*ZoomLimitsChanged)(PP_Instance instance,
                            double minimum_factor,
                            double maximium_factor);
};
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_ZOOM_DEV_H_ */

