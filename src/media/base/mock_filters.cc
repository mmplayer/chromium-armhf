// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_filters.h"

#include "base/logging.h"
#include "media/base/filter_host.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NotNull;

namespace media {

MockDataSource::MockDataSource()
    : total_bytes_(-1),
      buffered_bytes_(-1) {
}

MockDataSource::~MockDataSource() {}

void MockDataSource::set_host(FilterHost* filter_host) {
  Filter::set_host(filter_host);

  if (total_bytes_ > 0)
    host()->SetTotalBytes(total_bytes_);

  if (buffered_bytes_ > 0)
    host()->SetBufferedBytes(buffered_bytes_);
}

void MockDataSource::SetTotalAndBufferedBytes(int64 total_bytes,
                                              int64 buffered_bytes) {
  total_bytes_ = total_bytes;
  buffered_bytes_ = buffered_bytes;
}

MockDemuxerFactory::MockDemuxerFactory(MockDemuxer* demuxer)
    : demuxer_(demuxer), status_(PIPELINE_OK) {
}

MockDemuxerFactory::~MockDemuxerFactory() {}

void MockDemuxerFactory::SetError(PipelineStatus error) {
  DCHECK_NE(error, PIPELINE_OK);
  status_ = error;
}

void MockDemuxerFactory::RunBuildCallback(const std::string& url,
                                          const BuildCallback& callback) {
  if (!demuxer_.get()) {
    callback.Run(PIPELINE_ERROR_REQUIRED_FILTER_MISSING, NULL);
    return;
  }

  scoped_refptr<MockDemuxer> demuxer = demuxer_;
  demuxer_ = NULL;

  if (status_ == PIPELINE_OK) {
    callback.Run(PIPELINE_OK, demuxer.get());
    return;
  }

  callback.Run(status_, NULL);
}

DemuxerFactory* MockDemuxerFactory::Clone() const {
  return new MockDemuxerFactory(demuxer_.get());
}

MockDemuxer::MockDemuxer()
  : total_bytes_(-1), buffered_bytes_(-1), duration_() {}

MockDemuxer::~MockDemuxer() {}

void MockDemuxer::set_host(FilterHost* filter_host) {
  Demuxer::set_host(filter_host);

  if (total_bytes_ > 0)
    host()->SetTotalBytes(total_bytes_);

  if (buffered_bytes_ > 0)
    host()->SetBufferedBytes(buffered_bytes_);

  if (duration_.InMilliseconds() > 0)
    host()->SetDuration(duration_);
}

void MockDemuxer::SetTotalAndBufferedBytesAndDuration(
    int64 total_bytes, int64 buffered_bytes, const base::TimeDelta& duration) {
  total_bytes_ = total_bytes;
  buffered_bytes_ = buffered_bytes;
  duration_ = duration;
}

MockDemuxerStream::MockDemuxerStream() {}

MockDemuxerStream::~MockDemuxerStream() {}

MockVideoDecoder::MockVideoDecoder() {}

MockVideoDecoder::~MockVideoDecoder() {}

MockAudioDecoder::MockAudioDecoder() {}

MockAudioDecoder::~MockAudioDecoder() {}

MockVideoRenderer::MockVideoRenderer() {}

MockVideoRenderer::~MockVideoRenderer() {}

MockAudioRenderer::MockAudioRenderer() {}

MockAudioRenderer::~MockAudioRenderer() {}

MockFilterCollection::MockFilterCollection()
    : demuxer_(new MockDemuxer()),
      video_decoder_(new MockVideoDecoder()),
      audio_decoder_(new MockAudioDecoder()),
      video_renderer_(new MockVideoRenderer()),
      audio_renderer_(new MockAudioRenderer()) {
}

MockFilterCollection::~MockFilterCollection() {}

FilterCollection* MockFilterCollection::filter_collection(
    bool include_demuxer,
    bool run_build_callback,
    bool run_build,
    PipelineStatus build_status) const {
  FilterCollection* collection = new FilterCollection();

  MockDemuxerFactory* demuxer_factory =
      new MockDemuxerFactory(include_demuxer ? demuxer_ : NULL);

  if (build_status != PIPELINE_OK)
    demuxer_factory->SetError(build_status);

  if (run_build_callback) {
    ON_CALL(*demuxer_factory, Build(_, _)).WillByDefault(Invoke(
        demuxer_factory, &MockDemuxerFactory::RunBuildCallback));
  }  // else ignore Build calls.

  if (run_build)
    EXPECT_CALL(*demuxer_factory, Build(_, _));

  collection->SetDemuxerFactory(demuxer_factory);
  collection->AddVideoDecoder(video_decoder_);
  collection->AddAudioDecoder(audio_decoder_);
  collection->AddVideoRenderer(video_renderer_);
  collection->AddAudioRenderer(audio_renderer_);
  return collection;
}

void RunFilterCallback(::testing::Unused, const base::Closure& callback) {
  callback.Run();

}

void RunFilterStatusCB(::testing::Unused, const FilterStatusCB& cb) {
  cb.Run(PIPELINE_OK);
}

void RunPipelineStatusCB(PipelineStatus status, const PipelineStatusCB& cb) {
  cb.Run(status);
}

void RunFilterCallback3(::testing::Unused, const base::Closure& callback,
                        ::testing::Unused) {
  callback.Run();

}

void RunStopFilterCallback(const base::Closure& callback) {
  callback.Run();
}

MockFilter::MockFilter() {
}

MockFilter::~MockFilter() {}

MockStatisticsCallback::MockStatisticsCallback() {}

MockStatisticsCallback::~MockStatisticsCallback() {}

}  // namespace media
