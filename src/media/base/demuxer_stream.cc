// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/demuxer_stream.h"

namespace media {

DemuxerStream::~DemuxerStream() {}

AVStream* DemuxerStream::GetAVStream() {
  return NULL;
}

}  // namespace media
