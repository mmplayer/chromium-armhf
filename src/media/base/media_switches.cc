// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_switches.h"

namespace switches {

#if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_SOLARIS)
// The Alsa device to use when opening an audio stream.
const char kAlsaOutputDevice[] = "alsa-output-device";
// The Alsa device to use when opening an audio input stream.
const char kAlsaInputDevice[] = "alsa-input-device";
#endif

// Use PulseAudio instead of ALSA on Linux.
const char kUsePulseAudio[] = "use-pulseaudio";

// Set number of threads to use for video decoding.
const char kVideoThreads[] = "video-threads";

}  // namespace switches
