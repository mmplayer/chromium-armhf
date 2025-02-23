// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/graphics_3d_client.h"

#include "ppapi/c/ppp_graphics_3d.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

const char kPPPGraphics3DInterface[] = PPP_GRAPHICS_3D_INTERFACE;

void Graphics3D_ContextLost(PP_Instance instance) {
  void* object =
      pp::Instance::GetPerInstanceObject(instance, kPPPGraphics3DInterface);
  if (!object)
    return;
  return static_cast<Graphics3DClient*>(object)->Graphics3DContextLost();
}

static PPP_Graphics3D graphics3d_interface = {
  &Graphics3D_ContextLost,
};

}  // namespace

Graphics3DClient::Graphics3DClient(Instance* instance)
    : associated_instance_(instance) {
  pp::Module::Get()->AddPluginInterface(kPPPGraphics3DInterface,
                                        &graphics3d_interface);
  associated_instance_->AddPerInstanceObject(kPPPGraphics3DInterface, this);
}

Graphics3DClient::~Graphics3DClient() {
  associated_instance_->RemovePerInstanceObject(kPPPGraphics3DInterface, this);
}

}  // namespace pp
