// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filter_host.h"
#include "media/base/test_data_util.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/chunk_demuxer_client.h"
#include "media/webm/cluster_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::NiceMock;
using ::testing::_;

namespace media {

static const uint8 kTracksHeader[] = {
  0x16, 0x54, 0xAE, 0x6B, // Tracks ID
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // tracks(size = 0)
};

static const int kTracksHeaderSize = sizeof(kTracksHeader);
static const int kTracksSizeOffset = 4;

static const int kVideoTrackNum = 1;
static const int kAudioTrackNum = 2;

class MockChunkDemuxerClient : public ChunkDemuxerClient {
 public:
  MockChunkDemuxerClient() {}
  virtual ~MockChunkDemuxerClient() {}

  MOCK_METHOD1(DemuxerOpened, void(ChunkDemuxer* demuxer));
  MOCK_METHOD0(DemuxerClosed, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockChunkDemuxerClient);
};

class ChunkDemuxerTest : public testing::Test{
 protected:
  enum CodecsIndex {
    AUDIO,
    VIDEO,
    MAX_CODECS_INDEX
  };

  ChunkDemuxerTest()
      : client_(new MockChunkDemuxerClient()),
        demuxer_(new ChunkDemuxer(client_.get())) {
  }

  virtual ~ChunkDemuxerTest() {
    ShutdownDemuxer();
  }

  void CreateInfoTracks(bool has_audio, bool has_video,
                        scoped_array<uint8>* buffer, int* size) {
    scoped_array<uint8> info;
    int info_size = 0;
    scoped_array<uint8> audio_track_entry;
    int audio_track_entry_size = 0;
    scoped_array<uint8> video_track_entry;
    int video_track_entry_size = 0;

    ReadTestDataFile("webm_info_element", &info, &info_size);
    ReadTestDataFile("webm_vorbis_track_entry", &audio_track_entry,
                     &audio_track_entry_size);
    ReadTestDataFile("webm_vp8_track_entry", &video_track_entry,
                     &video_track_entry_size);

    int tracks_element_size = 0;

    if (has_audio)
      tracks_element_size += audio_track_entry_size;

    if (has_video)
      tracks_element_size += video_track_entry_size;

    *size = info_size + kTracksHeaderSize + tracks_element_size;

    buffer->reset(new uint8[*size]);

    uint8* buf = buffer->get();
    memcpy(buf, info.get(), info_size);
    buf += info_size;

    memcpy(buf, kTracksHeader, kTracksHeaderSize);

    int tmp = tracks_element_size;
    for (int i = 7; i > 0; i--) {
      buf[kTracksSizeOffset + i] = tmp & 0xff;
      tmp >>= 8;
    }

    buf += kTracksHeaderSize;

    if (has_audio) {
      memcpy(buf, audio_track_entry.get(), audio_track_entry_size);
      buf += audio_track_entry_size;
    }

    if (has_video) {
      memcpy(buf, video_track_entry.get(), video_track_entry_size);
      buf += video_track_entry_size;
    }
  }

  void AppendData(const uint8* data, size_t length) {
    EXPECT_CALL(mock_filter_host_, SetBufferedBytes(_)).Times(AnyNumber());
    EXPECT_CALL(mock_filter_host_, SetBufferedTime(_)).Times(AnyNumber());
    EXPECT_CALL(mock_filter_host_, SetNetworkActivity(true)).Times(AnyNumber());
    EXPECT_TRUE(demuxer_->AppendData(data, length));
  }

  void AppendDataInPieces(const uint8* data, size_t length) {
    const uint8* start = data;
    const uint8* end = data + length;
    size_t default_size = 7;
    while (start < end) {
      size_t append_size = std::min(default_size,
                                    static_cast<size_t>(end - start));
      AppendData(start, append_size);
      start += append_size;
    }
  }

  void AppendInfoTracks(bool has_audio, bool has_video) {
    scoped_array<uint8> info_tracks;
    int info_tracks_size = 0;
    CreateInfoTracks(has_audio, has_video, &info_tracks, &info_tracks_size);
    AppendData(info_tracks.get(), info_tracks_size);
  }

  static void InitDoneCalled(bool* was_called, PipelineStatus expectedStatus,
                             PipelineStatus status) {
    EXPECT_EQ(status, expectedStatus);
    *was_called = true;
  }

  void InitDemuxer(bool has_audio, bool has_video) {
    bool init_done_called = false;
    PipelineStatus expectedStatus =
        (has_audio || has_video) ? PIPELINE_OK : DEMUXER_ERROR_COULD_NOT_OPEN;

    EXPECT_CALL(*client_, DemuxerOpened(_));
    demuxer_->Init(base::Bind(&ChunkDemuxerTest::InitDoneCalled,
                              &init_done_called,
                              expectedStatus));

    EXPECT_FALSE(init_done_called);

    AppendInfoTracks(has_audio, has_video);

    EXPECT_TRUE(init_done_called);
    EXPECT_CALL(mock_filter_host_, SetDuration(_));
    EXPECT_CALL(mock_filter_host_, SetCurrentReadPosition(_));
    demuxer_->set_host(&mock_filter_host_);
  }

  void ShutdownDemuxer() {
    if (demuxer_) {
      EXPECT_CALL(*client_, DemuxerClosed());
      demuxer_->Shutdown();
    }
  }

  void AddSimpleBlock(ClusterBuilder* cb, int track_num, int64 timecode) {
    uint8 data[] = { 0x00 };
    cb->AddSimpleBlock(track_num, timecode, 0, data, sizeof(data));
  }

  void AddSimpleBlock(ClusterBuilder* cb, int track_num, int64 timecode,
                      int size) {
    scoped_array<uint8> data(new uint8[size]);
    cb->AddSimpleBlock(track_num, timecode, 0, data.get(), size);
  }

  MOCK_METHOD1(Checkpoint, void(int id));

  MockFilterHost mock_filter_host_;

  scoped_ptr<MockChunkDemuxerClient> client_;
  scoped_refptr<ChunkDemuxer> demuxer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChunkDemuxerTest);
};

TEST_F(ChunkDemuxerTest, TestInit) {
  // Test no streams, audio-only, video-only, and audio & video scenarios.
  for (int i = 0; i < 4; i++) {
    bool has_audio = (i & 0x1) != 0;
    bool has_video = (i & 0x2) != 0;

    client_.reset(new MockChunkDemuxerClient());
    demuxer_ = new ChunkDemuxer(client_.get());
    InitDemuxer(has_audio, has_video);

    scoped_refptr<DemuxerStream> audio_stream =
        demuxer_->GetStream(DemuxerStream::AUDIO);
    if (has_audio) {
      EXPECT_TRUE(audio_stream);

      const AudioDecoderConfig& config = audio_stream->audio_decoder_config();
      EXPECT_EQ(kCodecVorbis, config.codec());
      EXPECT_EQ(16, config.bits_per_channel());
      EXPECT_EQ(CHANNEL_LAYOUT_STEREO, config.channel_layout());
      EXPECT_EQ(44100, config.samples_per_second());
      EXPECT_TRUE(config.extra_data());
      EXPECT_GT(config.extra_data_size(), 0u);
    } else {
      EXPECT_FALSE(audio_stream);
    }

    scoped_refptr<DemuxerStream> video_stream =
        demuxer_->GetStream(DemuxerStream::VIDEO);
    if (has_video) {
      EXPECT_TRUE(video_stream);
    } else {
      EXPECT_FALSE(video_stream);
    }

    ShutdownDemuxer();
    demuxer_ = NULL;
  }
}

// Makes sure that Seek() reports an error if Shutdown()
// is called before the first cluster is passed to the demuxer.
TEST_F(ChunkDemuxerTest, TestShutdownBeforeFirstSeekCompletes) {
  InitDemuxer(true, true);

  demuxer_->Seek(base::TimeDelta::FromSeconds(0),
                 NewExpectedStatusCB(PIPELINE_ERROR_ABORT));
}

// Test that Seek() completes successfully when the first cluster
// arrives.
TEST_F(ChunkDemuxerTest, TestAppendDataAfterSeek) {
  InitDemuxer(true, true);

  InSequence s;

  EXPECT_CALL(*this, Checkpoint(1));

  demuxer_->Seek(base::TimeDelta::FromSeconds(0),
                 NewExpectedStatusCB(PIPELINE_OK));

  EXPECT_CALL(*this, Checkpoint(2));

  ClusterBuilder cb;
  cb.SetClusterTimecode(0);
  AddSimpleBlock(&cb, kVideoTrackNum, 0);
  scoped_ptr<Cluster> cluster(cb.Finish());

  Checkpoint(1);

  AppendData(cluster->data(), cluster->size());

  Checkpoint(2);
}

// Test the case where AppendData() is called before Init().
TEST_F(ChunkDemuxerTest, TestAppendDataBeforeInit) {
  scoped_array<uint8> info_tracks;
  int info_tracks_size = 0;
  CreateInfoTracks(true, true, &info_tracks, &info_tracks_size);

  EXPECT_FALSE(demuxer_->AppendData(info_tracks.get(), info_tracks_size));
}

static void OnReadDone(const base::TimeDelta& expected_time,
                       bool* called,
                       Buffer* buffer) {
  EXPECT_EQ(expected_time, buffer->GetTimestamp());
  *called = true;
}

// Make sure Read() callbacks are dispatched with the proper data.
TEST_F(ChunkDemuxerTest, TestRead) {
  InitDemuxer(true, true);

  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);

  bool audio_read_done = false;
  bool video_read_done = false;
  audio->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(32),
                         &audio_read_done));

  video->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(123),
                         &video_read_done));

  ClusterBuilder cb;
  cb.SetClusterTimecode(0);
  AddSimpleBlock(&cb, kAudioTrackNum, 32);
  AddSimpleBlock(&cb, kVideoTrackNum, 123);
  scoped_ptr<Cluster> cluster(cb.Finish());

  AppendData(cluster->data(), cluster->size());

  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);
}

TEST_F(ChunkDemuxerTest, TestOutOfOrderClusters) {
  InitDemuxer(true, true);

  ClusterBuilder cb;

  cb.SetClusterTimecode(10);
  AddSimpleBlock(&cb, kAudioTrackNum, 10);
  AddSimpleBlock(&cb, kVideoTrackNum, 10);
  AddSimpleBlock(&cb, kAudioTrackNum, 33);
  AddSimpleBlock(&cb, kVideoTrackNum, 43);
  scoped_ptr<Cluster> clusterA(cb.Finish());

  AppendData(clusterA->data(), clusterA->size());

  // Cluster B starts before clusterA and has data
  // that overlaps.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 7);
  AddSimpleBlock(&cb, kAudioTrackNum, 28);
  AddSimpleBlock(&cb, kVideoTrackNum, 40);
  scoped_ptr<Cluster> clusterB(cb.Finish());

  // Make sure that AppendData() fails because this cluster data
  // is before previous data.
  EXPECT_CALL(mock_filter_host_, SetError(PIPELINE_ERROR_DECODE));
  AppendData(clusterB->data(), clusterB->size());

  // Verify that AppendData() doesn't accept more data now.
  cb.SetClusterTimecode(45);
  AddSimpleBlock(&cb, kAudioTrackNum, 45);
  AddSimpleBlock(&cb, kVideoTrackNum, 45);
  scoped_ptr<Cluster> clusterC(cb.Finish());
  EXPECT_FALSE(demuxer_->AppendData(clusterC->data(), clusterC->size()));
}

TEST_F(ChunkDemuxerTest, TestNonMonotonicButAboveClusterTimecode) {
  InitDemuxer(true, true);

  ClusterBuilder cb;

  // Test the case where block timecodes are not monotonically
  // increasing but stay above the cluster timecode.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 10);
  AddSimpleBlock(&cb, kAudioTrackNum, 7);
  AddSimpleBlock(&cb, kVideoTrackNum, 15);
  scoped_ptr<Cluster> clusterA(cb.Finish());

  EXPECT_CALL(mock_filter_host_, SetError(PIPELINE_ERROR_DECODE));
  AppendData(clusterA->data(), clusterA->size());

  // Verify that AppendData() doesn't accept more data now.
  cb.SetClusterTimecode(20);
  AddSimpleBlock(&cb, kAudioTrackNum, 20);
  AddSimpleBlock(&cb, kVideoTrackNum, 20);
  scoped_ptr<Cluster> clusterB(cb.Finish());
  EXPECT_FALSE(demuxer_->AppendData(clusterB->data(), clusterB->size()));
}

TEST_F(ChunkDemuxerTest, TestBackwardsAndBeforeClusterTimecode) {
  InitDemuxer(true, true);

  ClusterBuilder cb;

  // Test timecodes going backwards and including values less than the cluster
  // timecode.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 5);
  AddSimpleBlock(&cb, kAudioTrackNum, 3);
  AddSimpleBlock(&cb, kVideoTrackNum, 3);
  scoped_ptr<Cluster> clusterA(cb.Finish());

  EXPECT_CALL(mock_filter_host_, SetError(PIPELINE_ERROR_DECODE));
  AppendData(clusterA->data(), clusterA->size());

  // Verify that AppendData() doesn't accept more data now.
  cb.SetClusterTimecode(6);
  AddSimpleBlock(&cb, kAudioTrackNum, 6);
  AddSimpleBlock(&cb, kVideoTrackNum, 6);
  scoped_ptr<Cluster> clusterB(cb.Finish());
  EXPECT_FALSE(demuxer_->AppendData(clusterB->data(), clusterB->size()));
}


TEST_F(ChunkDemuxerTest, TestPerStreamMonotonicallyIncreasingTimestamps) {
  InitDemuxer(true, true);

  ClusterBuilder cb;

  // Test strict monotonic increasing timestamps on a per stream
  // basis.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 7);
  scoped_ptr<Cluster> cluster(cb.Finish());

  EXPECT_CALL(mock_filter_host_, SetError(PIPELINE_ERROR_DECODE));
  AppendData(cluster->data(), cluster->size());
}

TEST_F(ChunkDemuxerTest, TestMonotonicallyIncreasingTimestampsAcrossClusters) {
  InitDemuxer(true, true);

  ClusterBuilder cb;

  // Test strict monotonic increasing timestamps on a per stream
  // basis across clusters.
  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 5);
  scoped_ptr<Cluster> clusterA(cb.Finish());

  AppendData(clusterA->data(), clusterA->size());

  cb.SetClusterTimecode(5);
  AddSimpleBlock(&cb, kAudioTrackNum, 5);
  AddSimpleBlock(&cb, kVideoTrackNum, 7);
  scoped_ptr<Cluster> clusterB(cb.Finish());

  EXPECT_CALL(mock_filter_host_, SetError(PIPELINE_ERROR_DECODE));
  AppendData(clusterB->data(), clusterB->size());

  // Verify that AppendData() doesn't accept more data now.
  cb.SetClusterTimecode(10);
  AddSimpleBlock(&cb, kAudioTrackNum, 10);
  AddSimpleBlock(&cb, kVideoTrackNum, 10);
  scoped_ptr<Cluster> clusterC(cb.Finish());
  EXPECT_FALSE(demuxer_->AppendData(clusterC->data(), clusterC->size()));
}

// Test the case where a cluster is passed to AppendData() before
// INFO & TRACKS data.
TEST_F(ChunkDemuxerTest, TestClusterBeforeInfoTracks) {
  EXPECT_CALL(*client_, DemuxerOpened(_));
  demuxer_->Init(NewExpectedStatusCB(DEMUXER_ERROR_COULD_NOT_OPEN));

  ClusterBuilder cb;
  cb.SetClusterTimecode(0);
  AddSimpleBlock(&cb, kVideoTrackNum, 0);
  scoped_ptr<Cluster> cluster(cb.Finish());

  AppendData(cluster->data(), cluster->size());
}

// Test cases where we get an EndOfStream() call during initialization.
TEST_F(ChunkDemuxerTest, TestEOSDuringInit) {
  EXPECT_CALL(*client_, DemuxerOpened(_));
  demuxer_->Init(NewExpectedStatusCB(DEMUXER_ERROR_COULD_NOT_OPEN));
  demuxer_->EndOfStream(PIPELINE_OK);
}

TEST_F(ChunkDemuxerTest, TestDecodeErrorEndOfStream) {
  InitDemuxer(true, true);

  ClusterBuilder cb;
  cb.SetClusterTimecode(0);
  AddSimpleBlock(&cb, kAudioTrackNum, 0);
  AddSimpleBlock(&cb, kVideoTrackNum, 0);
  AddSimpleBlock(&cb, kAudioTrackNum, 23);
  AddSimpleBlock(&cb, kVideoTrackNum, 33);
  scoped_ptr<Cluster> cluster(cb.Finish());
  AppendData(cluster->data(), cluster->size());

  EXPECT_CALL(mock_filter_host_, SetError(PIPELINE_ERROR_DECODE));
  demuxer_->EndOfStream(PIPELINE_ERROR_DECODE);
}

TEST_F(ChunkDemuxerTest, TestNetworkErrorEndOfStream) {
  InitDemuxer(true, true);

  ClusterBuilder cb;
  cb.SetClusterTimecode(0);
  AddSimpleBlock(&cb, kAudioTrackNum, 0);
  AddSimpleBlock(&cb, kVideoTrackNum, 0);
  AddSimpleBlock(&cb, kAudioTrackNum, 23);
  AddSimpleBlock(&cb, kVideoTrackNum, 33);
  scoped_ptr<Cluster> cluster(cb.Finish());
  AppendData(cluster->data(), cluster->size());

  EXPECT_CALL(mock_filter_host_, SetError(PIPELINE_ERROR_NETWORK));
  demuxer_->EndOfStream(PIPELINE_ERROR_NETWORK);
}

// Helper class to reduce duplicate code when testing end of stream
// Read() behavior.
class EndOfStreamHelper {
 public:
  EndOfStreamHelper(const scoped_refptr<Demuxer> demuxer)
      : demuxer_(demuxer),
        audio_read_done_(false),
        video_read_done_(false) {
  }

  // Request a read on the audio and video streams.
  void RequestReads() {
    EXPECT_FALSE(audio_read_done_);
    EXPECT_FALSE(video_read_done_);

    scoped_refptr<DemuxerStream> audio =
        demuxer_->GetStream(DemuxerStream::AUDIO);
    scoped_refptr<DemuxerStream> video =
        demuxer_->GetStream(DemuxerStream::VIDEO);

    audio->Read(base::Bind(&OnEndOfStreamReadDone,
                           &audio_read_done_));

    video->Read(base::Bind(&OnEndOfStreamReadDone,
                           &video_read_done_));
  }

  // Check to see if |audio_read_done_| and |video_read_done_| variables
  // match |expected|.
  void CheckIfReadDonesWereCalled(bool expected) {
    EXPECT_EQ(expected, audio_read_done_);
    EXPECT_EQ(expected, video_read_done_);
  }

 private:
  static void OnEndOfStreamReadDone(bool* called, Buffer* buffer) {
    EXPECT_TRUE(buffer->IsEndOfStream());
    *called = true;
  }

  scoped_refptr<Demuxer> demuxer_;
  bool audio_read_done_;
  bool video_read_done_;

  DISALLOW_COPY_AND_ASSIGN(EndOfStreamHelper);
};

// Make sure that all pending reads that we don't have media data for get an
// "end of stream" buffer when EndOfStream() is called.
TEST_F(ChunkDemuxerTest, TestEndOfStreamWithPendingReads) {
  InitDemuxer(true, true);

  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);

  bool audio_read_done_1 = false;
  bool video_read_done_1 = false;
  EndOfStreamHelper end_of_stream_helper_1(demuxer_);
  EndOfStreamHelper end_of_stream_helper_2(demuxer_);

  audio->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(32),
                         &audio_read_done_1));

  video->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(123),
                         &video_read_done_1));

  end_of_stream_helper_1.RequestReads();
  end_of_stream_helper_2.RequestReads();

  ClusterBuilder cb;
  cb.SetClusterTimecode(0);
  AddSimpleBlock(&cb, kAudioTrackNum, 32);
  AddSimpleBlock(&cb, kVideoTrackNum, 123);
  scoped_ptr<Cluster> cluster(cb.Finish());

  AppendData(cluster->data(), cluster->size());

  EXPECT_TRUE(audio_read_done_1);
  EXPECT_TRUE(video_read_done_1);
  end_of_stream_helper_1.CheckIfReadDonesWereCalled(false);
  end_of_stream_helper_2.CheckIfReadDonesWereCalled(false);

  demuxer_->EndOfStream(PIPELINE_OK);

  end_of_stream_helper_1.CheckIfReadDonesWereCalled(true);
  end_of_stream_helper_2.CheckIfReadDonesWereCalled(true);
}

// Make sure that all Read() calls after we get an EndOfStream()
// call return an "end of stream" buffer.
TEST_F(ChunkDemuxerTest, TestReadsAfterEndOfStream) {
  InitDemuxer(true, true);

  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);

  bool audio_read_done_1 = false;
  bool video_read_done_1 = false;
  EndOfStreamHelper end_of_stream_helper_1(demuxer_);
  EndOfStreamHelper end_of_stream_helper_2(demuxer_);
  EndOfStreamHelper end_of_stream_helper_3(demuxer_);

  audio->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(32),
                         &audio_read_done_1));

  video->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(123),
                         &video_read_done_1));

  end_of_stream_helper_1.RequestReads();

  ClusterBuilder cb;
  cb.SetClusterTimecode(0);
  AddSimpleBlock(&cb, kAudioTrackNum, 32);
  AddSimpleBlock(&cb, kVideoTrackNum, 123);
  scoped_ptr<Cluster> cluster(cb.Finish());

  AppendData(cluster->data(), cluster->size());

  EXPECT_TRUE(audio_read_done_1);
  EXPECT_TRUE(video_read_done_1);
  end_of_stream_helper_1.CheckIfReadDonesWereCalled(false);

  demuxer_->EndOfStream(PIPELINE_OK);

  end_of_stream_helper_1.CheckIfReadDonesWereCalled(true);

  // Request a few more reads and make sure we immediately get
  // end of stream buffers.
  end_of_stream_helper_2.RequestReads();
  end_of_stream_helper_2.CheckIfReadDonesWereCalled(true);

  end_of_stream_helper_3.RequestReads();
  end_of_stream_helper_3.CheckIfReadDonesWereCalled(true);
}

// Make sure AppendData() will accept elements that span multiple calls.
TEST_F(ChunkDemuxerTest, TestAppendingInPieces) {

  EXPECT_CALL(*client_, DemuxerOpened(_));
  demuxer_->Init(NewExpectedStatusCB(PIPELINE_OK));

  EXPECT_CALL(mock_filter_host_, SetDuration(_));
  EXPECT_CALL(mock_filter_host_, SetCurrentReadPosition(_));
  demuxer_->set_host(&mock_filter_host_);

  scoped_array<uint8> info_tracks;
  int info_tracks_size = 0;
  CreateInfoTracks(true, true, &info_tracks, &info_tracks_size);

  ClusterBuilder cb;
  cb.SetClusterTimecode(0);
  AddSimpleBlock(&cb, kAudioTrackNum, 32, 512);
  AddSimpleBlock(&cb, kVideoTrackNum, 123, 1024);
  scoped_ptr<Cluster> clusterA(cb.Finish());

  cb.SetClusterTimecode(125);
  AddSimpleBlock(&cb, kAudioTrackNum, 125, 2048);
  AddSimpleBlock(&cb, kVideoTrackNum, 150, 2048);
  scoped_ptr<Cluster> clusterB(cb.Finish());

  size_t buffer_size = info_tracks_size + clusterA->size() + clusterB->size();
  scoped_array<uint8> buffer(new uint8[buffer_size]);
  uint8* dst = buffer.get();
  memcpy(dst, info_tracks.get(), info_tracks_size);
  dst += info_tracks_size;

  memcpy(dst, clusterA->data(), clusterA->size());
  dst += clusterA->size();

  memcpy(dst, clusterB->data(), clusterB->size());
  dst += clusterB->size();

  AppendDataInPieces(buffer.get(), buffer_size);

  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);

  ASSERT_TRUE(audio);
  ASSERT_TRUE(video);

  bool audio_read_done = false;
  bool video_read_done = false;
  audio->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(32),
                         &audio_read_done));

  video->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(123),
                         &video_read_done));

  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);

  audio_read_done = false;
  video_read_done = false;
  audio->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(125),
                         &audio_read_done));

  video->Read(base::Bind(&OnReadDone,
                         base::TimeDelta::FromMilliseconds(150),
                         &video_read_done));

  EXPECT_TRUE(audio_read_done);
  EXPECT_TRUE(video_read_done);
}

}  // namespace media
