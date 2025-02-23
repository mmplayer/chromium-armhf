/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <stdlib.h>
#include <string.h>

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"

PPB_GetInterface g_get_browser_interface = NULL;

const struct PPB_Core* g_core_interface;
const struct PPB_Graphics2D* g_graphics_2d_interface;
const struct PPB_ImageData* g_image_data_interface;
const struct PPB_Instance* g_instance_interface;

/* PPP_Instance implementation -----------------------------------------------*/

struct InstanceInfo {
  PP_Instance pp_instance;
  struct PP_Size last_size;

  struct InstanceInfo* next;
};

/** Linked list of all live instances. */
struct InstanceInfo* all_instances = NULL;

/** Returns a refed resource corresponding to the created graphics 2d. */
PP_Resource MakeAndBindGraphics2D(PP_Instance instance,
                                  const struct PP_Size* size) {
  PP_Resource graphics;

  graphics = g_graphics_2d_interface->Create(instance, size, PP_FALSE);
  if (!graphics)
    return 0;

  if (!g_instance_interface->BindGraphics(instance, graphics)) {
    g_core_interface->ReleaseResource(graphics);
    return 0;
  }
  return graphics;
}

void FlushCompletionCallback(void* user_data, int32_t result) {
  /* Don't need to do anything here. */
}

void Repaint(struct InstanceInfo* instance, const struct PP_Size* size) {
  PP_Resource image, graphics;
  struct PP_ImageDataDesc image_desc;
  uint32_t* image_data;
  int num_words, i;

  /* Create image data to paint into. */
  image = g_image_data_interface->Create(
      instance->pp_instance, PP_IMAGEDATAFORMAT_BGRA_PREMUL, size, PP_TRUE);
  if (!image)
    return;
  g_image_data_interface->Describe(image, &image_desc);

  /* Fill the image with blue. */
  image_data = (uint32_t*)g_image_data_interface->Map(image);
  if (!image_data) {
    g_core_interface->ReleaseResource(image);
    return;
  }
  num_words = image_desc.stride * size->height / 4;
  for (i = 0; i < num_words; i++)
    image_data[i] = 0xFF0000FF;

  /* Create the graphics 2d and paint the image to it. */
  graphics = MakeAndBindGraphics2D(instance->pp_instance, size);
  if (!graphics) {
    g_core_interface->ReleaseResource(image);
    return;
  }

  g_graphics_2d_interface->ReplaceContents(graphics, image);
  g_graphics_2d_interface->Flush(graphics,
      PP_MakeCompletionCallback(&FlushCompletionCallback, NULL));

  g_core_interface->ReleaseResource(graphics);
  g_core_interface->ReleaseResource(image);
}

/** Returns the info for the given instance, or NULL if it's not found. */
struct InstanceInfo* FindInstance(PP_Instance instance) {
  struct InstanceInfo* cur = all_instances;
  while (cur) {
    if (cur->pp_instance == instance)
      return cur;
  }
  return NULL;
}

PP_Bool Instance_DidCreate(PP_Instance instance,
                           uint32_t argc,
                           const char* argn[],
                           const char* argv[]) {
  struct InstanceInfo* info =
      (struct InstanceInfo*)malloc(sizeof(struct InstanceInfo));
  info->pp_instance = instance;
  info->last_size.width = 0;
  info->last_size.height = 0;

  /* Insert into linked list of live instances. */
  info->next = all_instances;
  all_instances = info;
  return PP_TRUE;
}

void Instance_DidDestroy(PP_Instance instance) {
  /* Find the matching item in the linked list, delete it, and patch the
   * links.
   */
  struct InstanceInfo** prev_ptr = &all_instances;
  struct InstanceInfo* cur = all_instances;
  while (cur) {
    if (instance == cur->pp_instance) {
      *prev_ptr = cur->next;
      free(cur);
      return;
    }
    prev_ptr = &cur->next;
  }
}

void Instance_DidChangeView(PP_Instance pp_instance,
                            const struct PP_Rect* position,
                            const struct PP_Rect* clip) {
  struct InstanceInfo* info = FindInstance(pp_instance);
  if (!info)
    return;

  if (info->last_size.width != position->size.width ||
      info->last_size.height != position->size.height) {
    /* Got a resize, repaint the plugin. */
    Repaint(info, &position->size);
    info->last_size.width = position->size.width;
    info->last_size.height = position->size.height;
  }
}

void Instance_DidChangeFocus(PP_Instance pp_instance, PP_Bool has_focus) {
}

PP_Bool Instance_HandleDocumentLoad(PP_Instance pp_instance,
                                    PP_Resource pp_url_loader) {
  return PP_FALSE;
}

static struct PPP_Instance instance_interface = {
  &Instance_DidCreate,
  &Instance_DidDestroy,
  &Instance_DidChangeView,
  &Instance_DidChangeFocus,
  &Instance_HandleDocumentLoad
};


/* Global entrypoints --------------------------------------------------------*/

PP_EXPORT int32_t PPP_InitializeModule(PP_Module module,
                                       PPB_GetInterface get_browser_interface) {
  g_get_browser_interface = get_browser_interface;

  g_core_interface = (const struct PPB_Core*)
      get_browser_interface(PPB_CORE_INTERFACE);
  g_instance_interface = (const struct PPB_Instance*)
      get_browser_interface(PPB_INSTANCE_INTERFACE);
  g_image_data_interface = (const struct PPB_ImageData*)
      get_browser_interface(PPB_IMAGEDATA_INTERFACE);
  g_graphics_2d_interface = (const struct PPB_Graphics2D*)
      get_browser_interface(PPB_GRAPHICS_2D_INTERFACE);
  if (!g_core_interface || !g_instance_interface || !g_image_data_interface ||
      !g_graphics_2d_interface)
    return -1;

  return PP_OK;
}

PP_EXPORT void PPP_ShutdownModule() {
}

PP_EXPORT const void* PPP_GetInterface(const char* interface_name) {
  if (strcmp(interface_name, PPP_INSTANCE_INTERFACE) == 0)
    return &instance_interface;
  return NULL;
}

