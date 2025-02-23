// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_OPENBSD_AUDIO_MANAGER_OPENBSD_H_
#define MEDIA_AUDIO_OPENBSD_AUDIO_MANAGER_OPENBSD_H_

#include "base/compiler_specific.h"
#include "media/audio/audio_io.h"

class AudioManagerOpenBSD : public AudioManagerBase {
 public:
  AudioManagerOpenBSD();

  // Call before using a newly created AudioManagerOpenBSD instance.
  virtual void Init();

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices() OVERRIDE;
  virtual bool HasAudioInputDevices() OVERRIDE;
  virtual AudioOutputStream* MakeAudioOutputStream(
      const AudioParameters& params) OVERRIDE;
  virtual AudioInputStream* MakeAudioInputStream(
      const AudioParameters& params) OVERRIDE;
  virtual bool IsRecordingInProgress() OVERRIDE;
  virtual void MuteAll() OVERRIDE;
  virtual void UnMuteAll() OVERRIDE;

 protected:
  virtual ~AudioManagerOpenBSD();

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioManagerOpenBSD);
};

#endif  // MEDIA_AUDIO_OPENBSD_AUDIO_MANAGER_OPENBSD_H_
