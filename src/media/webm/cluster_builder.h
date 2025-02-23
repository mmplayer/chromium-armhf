// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBM_CLUSTER_BUILDER_H_
#define MEDIA_WEBM_CLUSTER_BUILDER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/buffers.h"

namespace media {

class Cluster {
 public:
  // Takes ownership of |data|
  Cluster(const uint8* data, int size);
  ~Cluster();

  const uint8* data() const { return data_.get(); }
  int size() const { return size_; }

 private:
  scoped_array<const uint8> data_;
  int size_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Cluster);
};

class ClusterBuilder {
 public:
  ClusterBuilder();
  ~ClusterBuilder();

  void SetClusterTimecode(int64 cluster_timecode);
  void AddSimpleBlock(int track_num, int64 timecode, int flags,
                      const uint8* data, int size);

  Cluster* Finish();

 private:
  void Reset();
  void ExtendBuffer(int bytes_needed);
  void UpdateUInt64(int offset, int64 value);

  scoped_array<uint8> buffer_;
  int buffer_size_;
  int bytes_used_;
  int64 cluster_timecode_;

  DISALLOW_COPY_AND_ASSIGN(ClusterBuilder);
};

}  // namespace media

#endif  // MEDIA_WEBM_CLUSTER_BUILDER_H_
