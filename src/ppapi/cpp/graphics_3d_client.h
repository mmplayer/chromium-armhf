// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_GRAPHICS_3D_CLIENT_H_
#define PPAPI_CPP_GRAPHICS_3D_CLIENT_H_

#include "ppapi/c/pp_stdint.h"

namespace pp {

class Instance;
class Rect;
class Scrollbar_Dev;
class Widget_Dev;

// This class provides a C++ interface for callbacks related to 3D. You
// would normally use multiple inheritance to derive from this class in your
// instance.
class Graphics3DClient {
 public:
  Graphics3DClient(Instance* instance);
  virtual ~Graphics3DClient();

  /**
   * Notification that the context was lost for the 3D devices.
   */
  virtual void Graphics3DContextLost() = 0;

 private:
  Instance* associated_instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_GRAPHICS_3D_CLIENT_H_
