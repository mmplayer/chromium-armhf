// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/platform/nacl_check.h"

#include "ppapi/c/dev/ppb_context_3d_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/c/dev/ppb_scrollbar_dev.h"
#include "ppapi/c/dev/ppb_surface_3d_dev.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppb_widget_dev.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/c/ppb_fullscreen.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/c/ppb_var.h"

#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/internal_utils.h"

const void* GetBrowserInterface(const char* interface_name) {
  return (*ppb_get_interface())(interface_name);
}

const void* GetBrowserInterfaceSafe(const char* interface_name) {
  const void* ppb_interface = (*ppb_get_interface())(interface_name);
  CHECK(ppb_interface != NULL);
  return ppb_interface;
}

// Stable interfaces.

const PPB_Core* PPBCore() {
  return reinterpret_cast<const PPB_Core*>(
      GetBrowserInterfaceSafe(PPB_CORE_INTERFACE));
}

const PPB_FileIO* PPBFileIO() {
  return reinterpret_cast<const PPB_FileIO*>(
      GetBrowserInterface(PPB_FILEIO_INTERFACE));
}

const PPB_FileRef* PPBFileRef() {
  return reinterpret_cast<const PPB_FileRef*>(
      GetBrowserInterface(PPB_FILEREF_INTERFACE));
}

const PPB_FileSystem* PPBFileSystem() {
  return reinterpret_cast<const PPB_FileSystem*>(
      GetBrowserInterface(PPB_FILESYSTEM_INTERFACE));
}

const PPB_Fullscreen* PPBFullscreen() {
  return reinterpret_cast<const PPB_Fullscreen*>(
      GetBrowserInterfaceSafe(PPB_FULLSCREEN_INTERFACE));
}

const PPB_Graphics2D* PPBGraphics2D() {
  return reinterpret_cast<const PPB_Graphics2D*>(
      GetBrowserInterfaceSafe(PPB_GRAPHICS_2D_INTERFACE));
}

const PPB_Graphics3D* PPBGraphics3D() {
  return reinterpret_cast<const PPB_Graphics3D*>(
      GetBrowserInterface(PPB_GRAPHICS_3D_INTERFACE));
}

const PPB_ImageData* PPBImageData() {
  return reinterpret_cast<const PPB_ImageData*>(
      GetBrowserInterfaceSafe(PPB_IMAGEDATA_INTERFACE));
}

const PPB_InputEvent* PPBInputEvent() {
  return reinterpret_cast<const PPB_InputEvent*>(
      GetBrowserInterfaceSafe(PPB_INPUT_EVENT_INTERFACE));
}

const PPB_Instance* PPBInstance() {
  return reinterpret_cast<const PPB_Instance*>(
      GetBrowserInterfaceSafe(PPB_INSTANCE_INTERFACE));
}

const PPB_KeyboardInputEvent* PPBKeyboardInputEvent() {
  return reinterpret_cast<const PPB_KeyboardInputEvent*>(
      GetBrowserInterfaceSafe(PPB_KEYBOARD_INPUT_EVENT_INTERFACE));
}

const PPB_Messaging* PPBMessaging() {
  return reinterpret_cast<const PPB_Messaging*>(
      GetBrowserInterfaceSafe(PPB_MESSAGING_INTERFACE));
}

const PPB_MouseInputEvent* PPBMouseInputEvent() {
  return reinterpret_cast<const PPB_MouseInputEvent*>(
      GetBrowserInterfaceSafe(PPB_MOUSE_INPUT_EVENT_INTERFACE));
}

const PPB_OpenGLES2* PPBOpenGLES2() {
  return reinterpret_cast<const PPB_OpenGLES2*>(
      GetBrowserInterface(PPB_OPENGLES2_INTERFACE));
}

const PPB_URLLoader* PPBURLLoader() {
  return reinterpret_cast<const PPB_URLLoader*>(
      GetBrowserInterfaceSafe(PPB_URLLOADER_INTERFACE));
}

const PPB_URLRequestInfo* PPBURLRequestInfo() {
  return reinterpret_cast<const PPB_URLRequestInfo*>(
      GetBrowserInterfaceSafe(PPB_URLREQUESTINFO_INTERFACE));
}

const PPB_URLResponseInfo* PPBURLResponseInfo() {
  return reinterpret_cast<const PPB_URLResponseInfo*>(
      GetBrowserInterfaceSafe(PPB_URLRESPONSEINFO_INTERFACE));
}

const PPB_Var* PPBVar() {
  return reinterpret_cast<const PPB_Var*>(
      GetBrowserInterfaceSafe(PPB_VAR_INTERFACE));
}

const PPB_WheelInputEvent* PPBWheelInputEvent() {
  return reinterpret_cast<const PPB_WheelInputEvent*>(
      GetBrowserInterfaceSafe(PPB_WHEEL_INPUT_EVENT_INTERFACE));
}


// Dev interfaces.

const PPB_Context3D_Dev* PPBContext3DDev() {
  return reinterpret_cast<const PPB_Context3D_Dev*>(
      GetBrowserInterface(PPB_CONTEXT_3D_DEV_INTERFACE));
}

const PPB_CursorControl_Dev* PPBCursorControlDev() {
  return reinterpret_cast<const PPB_CursorControl_Dev*>(
      GetBrowserInterface(PPB_CURSOR_CONTROL_DEV_INTERFACE));
}

const PPB_Font_Dev* PPBFontDev() {
  return reinterpret_cast<const PPB_Font_Dev*>(
      GetBrowserInterface(PPB_FONT_DEV_INTERFACE));
}

const PPB_Memory_Dev* PPBMemoryDev() {
  return reinterpret_cast<const PPB_Memory_Dev*>(
      GetBrowserInterface(PPB_MEMORY_DEV_INTERFACE));
}

const PPB_Scrollbar_Dev* PPBScrollbarDev() {
  return  reinterpret_cast<const PPB_Scrollbar_Dev*>(
      GetBrowserInterface(PPB_SCROLLBAR_DEV_INTERFACE));
}

const PPB_Surface3D_Dev* PPBSurface3DDev() {
  return  reinterpret_cast<const PPB_Surface3D_Dev*>(
      GetBrowserInterface(PPB_SURFACE_3D_DEV_INTERFACE));
}

const PPB_Testing_Dev* PPBTestingDev() {
  return  reinterpret_cast<const PPB_Testing_Dev*>(
      GetBrowserInterface(PPB_TESTING_DEV_INTERFACE));
}

const PPB_Widget_Dev* PPBWidgetDev() {
  return  reinterpret_cast<const PPB_Widget_Dev*>(
      GetBrowserInterface(PPB_WIDGET_DEV_INTERFACE));
}
