// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_PROCESS_LAUNCH_CAUSES_H_
#define CONTENT_COMMON_GPU_GPU_PROCESS_LAUNCH_CAUSES_H_
#pragma once

namespace content {

// If you change this enum you also need to update
// tools/histograms/histograms.xml.
enum CauseForGpuLaunch {
  CAUSE_FOR_GPU_LAUNCH_ABOUT_GPUCRASH,
  CAUSE_FOR_GPU_LAUNCH_ABOUT_GPUHANG,
  CAUSE_FOR_GPU_LAUNCH_GPUDATAMANAGER_REQUESTCOMPLETEGPUINFOIFNEEDED,
  CAUSE_FOR_GPU_LAUNCH_RENDERWIDGETFULLSCREENPEPPER_CREATECONTEXT,
  CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE,
  CAUSE_FOR_GPU_LAUNCH_NO_LAUNCH,
  CAUSE_FOR_GPU_LAUNCH_VIDEODECODEACCELERATOR_INITIALIZE,
  CAUSE_FOR_GPU_LAUNCH_PEPPERPLATFORMCONTEXT3DIMPL_INITIALIZE,

  // All new values should be inserted above this point so that
  // existing values continue to match up with those in histograms.xml.
  CAUSE_FOR_GPU_LAUNCH_MAX_ENUM
};

}  // namespace content


#endif  // CONTENT_COMMON_GPU_GPU_PROCESS_LAUNCH_CAUSES_H_
