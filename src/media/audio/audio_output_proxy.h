// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_STREAM_PROXY_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_STREAM_PROXY_H_

#include "base/basictypes.h"
#include "base/task.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

class AudioOutputDispatcher;

// AudioOutputProxy is an audio otput stream that uses resources more
// efficiently than a regular audio output stream: it opens audio
// device only when sound is playing, i.e. between Start() and Stop()
// (there is still one physical stream per each audio output proxy in
// playing state).
//
// AudioOutputProxy uses AudioOutputDispatcher to open and close
// physical output streams.
class MEDIA_EXPORT AudioOutputProxy : public AudioOutputStream {
 public:
  // Caller keeps ownership of |dispatcher|.
  AudioOutputProxy(AudioOutputDispatcher* dispatcher);

  // AudioOutputStream interface.
  virtual bool Open();
  virtual void Start(AudioSourceCallback* callback);
  virtual void Stop();
  virtual void SetVolume(double volume);
  virtual void GetVolume(double* volume);
  virtual void Close();

 private:
  // Needs to access destructor.
  friend class DeleteTask<AudioOutputProxy>;

  enum State {
    kCreated,
    kOpened,
    kPlaying,
    kClosed,
    kError,
  };

  virtual ~AudioOutputProxy();

  scoped_refptr<AudioOutputDispatcher> dispatcher_;
  State state_;

  // The actual audio stream. Must be set to NULL in any state other
  // than kPlaying.
  AudioOutputStream* physical_stream_;

  // Need to save volume here, so that we can restore it in case the stream
  // is stopped, and then started again.
  double volume_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputProxy);
};

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_STREAM_PROXY_H_
