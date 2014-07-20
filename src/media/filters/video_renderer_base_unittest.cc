// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "media/base/data_buffer.h"
#include "media/base/limits.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "media/base/video_frame.h"
#include "media/filters/video_renderer_base.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;

namespace media {
ACTION(OnStop) {
  arg0.Run();
}

// Mocked subclass of VideoRendererBase for testing purposes.
class MockVideoRendererBase : public VideoRendererBase {
 public:
  MockVideoRendererBase() {}
  virtual ~MockVideoRendererBase() {}

  // VideoRendererBase implementation.
  MOCK_METHOD1(OnInitialize, bool(VideoDecoder* decoder));
  MOCK_METHOD1(OnStop, void(const base::Closure& callback));
  MOCK_METHOD0(OnFrameAvailable, void());

  // Used for verifying check points during tests.
  MOCK_METHOD1(CheckPoint, void(int id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoRendererBase);
};

class VideoRendererBaseTest : public ::testing::Test {
 public:
  VideoRendererBaseTest()
      : renderer_(new MockVideoRendererBase()),
        decoder_(new MockVideoDecoder()),
        seeking_(false) {
    renderer_->set_host(&host_);

    // Queue all reads from the decoder.
    EXPECT_CALL(*decoder_, ProduceVideoFrame(_))
        .WillRepeatedly(Invoke(this, &VideoRendererBaseTest::EnqueueCallback));

    EXPECT_CALL(*decoder_, natural_size()).WillRepeatedly(Return(kNaturalSize));

    EXPECT_CALL(stats_callback_object_, OnStatistics(_))
        .Times(AnyNumber());
  }

  virtual ~VideoRendererBaseTest() {
    read_queue_.clear();

    if (renderer_.get()) {
      // Expect a call into the subclass.
      EXPECT_CALL(*renderer_, OnStop(_))
          .WillOnce(DoAll(OnStop(), Return()))
          .RetiresOnSaturation();

      renderer_->Stop(NewExpectedClosure());
    }
  }

  void Initialize() {
    // Who knows how many times ThreadMain() will execute!
    //
    // TODO(scherkus): really, really, really need to inject a thread into
    // VideoRendererBase... it makes mocking much harder.
    EXPECT_CALL(host_, GetTime()).WillRepeatedly(Return(base::TimeDelta()));

    // Expects the video renderer to get duration from the host.
    EXPECT_CALL(host_, GetDuration())
        .WillRepeatedly(Return(base::TimeDelta()));

    InSequence s;

    // We expect the video size to be set.
    EXPECT_CALL(host_, SetNaturalVideoSize(kNaturalSize));

    // Then our subclass will be asked to initialize.
    EXPECT_CALL(*renderer_, OnInitialize(_))
        .WillOnce(Return(true));

    // Initialize, we shouldn't have any reads.
    renderer_->Initialize(decoder_,
                          NewExpectedClosure(), NewStatisticsCallback());
    EXPECT_EQ(0u, read_queue_.size());

    // Now seek to trigger prerolling.
    Seek(0);
  }

  void StartSeeking(int64 timestamp, PipelineStatus expected_status) {
    EXPECT_FALSE(seeking_);

    // Seek to trigger prerolling.
    seeking_ = true;
    renderer_->Seek(base::TimeDelta::FromMicroseconds(timestamp),
                    base::Bind(&VideoRendererBaseTest::OnSeekComplete,
                               base::Unretained(this),
                               expected_status));
  }

  void FinishSeeking() {
    EXPECT_CALL(*renderer_, OnFrameAvailable());
    EXPECT_TRUE(seeking_);

    // Satisfy the read requests.  The callback must be executed in order
    // to exit the loop since VideoRendererBase can read a few extra frames
    // after |timestamp| in order to preroll.
    for (int64 i = 0; seeking_; ++i) {
      CreateFrame(i * kDuration, kDuration);
    }
  }

  void Seek(int64 timestamp) {
    StartSeeking(timestamp, PIPELINE_OK);
    FinishSeeking();
  }

  void Flush() {
    renderer_->Pause(NewExpectedClosure());

    renderer_->Flush(NewExpectedClosure());
  }

  void CreateError() {
    decoder_->VideoFrameReadyForTest(NULL);
  }

  void CreateFrame(int64 timestamp, int64 duration) {
    const base::TimeDelta kZero;
    scoped_refptr<VideoFrame> frame =
        VideoFrame::CreateFrame(VideoFrame::RGB32, kNaturalSize.width(),
                                kNaturalSize.height(),
                                base::TimeDelta::FromMicroseconds(timestamp),
                                base::TimeDelta::FromMicroseconds(duration));
    decoder_->VideoFrameReadyForTest(frame);
  }

  void ExpectCurrentTimestamp(int64 timestamp) {
    scoped_refptr<VideoFrame> frame;
    renderer_->GetCurrentFrame(&frame);
    EXPECT_EQ(timestamp, frame->GetTimestamp().InMicroseconds());
    renderer_->PutCurrentFrame(frame);
  }

 protected:
  static const gfx::Size kNaturalSize;
  static const int64 kDuration;

  StatisticsCallback NewStatisticsCallback() {
    return base::Bind(&MockStatisticsCallback::OnStatistics,
                      base::Unretained(&stats_callback_object_));
  }

  // Fixture members.
  scoped_refptr<MockVideoRendererBase> renderer_;
  scoped_refptr<MockVideoDecoder> decoder_;
  StrictMock<MockFilterHost> host_;
  MockStatisticsCallback stats_callback_object_;

  // Receives all the buffers that renderer had provided to |decoder_|.
  std::deque<scoped_refptr<VideoFrame> > read_queue_;

 private:
  void EnqueueCallback(scoped_refptr<VideoFrame> frame) {
    read_queue_.push_back(frame);
  }

  void OnSeekComplete(PipelineStatus expected_status, PipelineStatus status) {
    EXPECT_EQ(status, expected_status);
    EXPECT_TRUE(seeking_);
    seeking_ = false;
  }

  bool seeking_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererBaseTest);
};

const gfx::Size VideoRendererBaseTest::kNaturalSize(16u, 16u);
const int64 VideoRendererBaseTest::kDuration = 10;

// Test initialization where the subclass failed for some reason.
TEST_F(VideoRendererBaseTest, Initialize_Failed) {
  InSequence s;

  // We expect the video size to be set.
  EXPECT_CALL(host_, SetNaturalVideoSize(kNaturalSize));

  // Our subclass will fail when asked to initialize.
  EXPECT_CALL(*renderer_, OnInitialize(_))
      .WillOnce(Return(false));

  // We expect to receive an error.
  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_INITIALIZATION_FAILED));

  // Initialize, we expect to have no reads.
  renderer_->Initialize(decoder_,
                        NewExpectedClosure(), NewStatisticsCallback());
  EXPECT_EQ(0u, read_queue_.size());
}

// Test successful initialization and preroll.
TEST_F(VideoRendererBaseTest, Initialize_Successful) {
  Initialize();
  ExpectCurrentTimestamp(0);
  Flush();
}

TEST_F(VideoRendererBaseTest, Play) {
  Initialize();
  renderer_->Play(NewExpectedClosure());
  Flush();
}

TEST_F(VideoRendererBaseTest, Error_Playing) {
  Initialize();
  renderer_->Play(NewExpectedClosure());

  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_DECODE));
  CreateError();
  Flush();
}

TEST_F(VideoRendererBaseTest, Error_Seeking) {
  Initialize();
  Flush();
  StartSeeking(0, PIPELINE_ERROR_DECODE);
  CreateError();
  Flush();
}

TEST_F(VideoRendererBaseTest, Seek_Exact) {
  Initialize();
  Flush();
  Seek(kDuration * 6);
  ExpectCurrentTimestamp(kDuration * 6);
  Flush();
}

TEST_F(VideoRendererBaseTest, Seek_RightBefore) {
  Initialize();
  Flush();
  Seek(kDuration * 6 - 1);
  ExpectCurrentTimestamp(kDuration * 5);
  Flush();
}

TEST_F(VideoRendererBaseTest, Seek_RightAfter) {
  Initialize();
  Flush();
  Seek(kDuration * 6 + 1);
  ExpectCurrentTimestamp(kDuration * 6);
  Flush();
}

// Verify behavior for GetCurrentFrame() calls that happen after a
// decoder error.
TEST_F(VideoRendererBaseTest, GetCurrentFrame_AfterError) {
  Initialize();
  renderer_->Play(NewExpectedClosure());

  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_DECODE));
  CreateError();

  scoped_refptr<VideoFrame> frame;
  renderer_->GetCurrentFrame(&frame);
  EXPECT_TRUE(!frame.get());
  renderer_->PutCurrentFrame(frame);
}

// Verify behavior for GetCurrentFrame() when an error occurs in the middle
// of a paint operation.
TEST_F(VideoRendererBaseTest, Error_DuringPaint) {
  Initialize();
  renderer_->Play(NewExpectedClosure());

  scoped_refptr<VideoFrame> frame;
  renderer_->GetCurrentFrame(&frame);

  EXPECT_TRUE(frame.get());

  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_DECODE));
  CreateError();

  renderer_->PutCurrentFrame(frame);

  renderer_->GetCurrentFrame(&frame);
  EXPECT_TRUE(!frame.get());
  renderer_->PutCurrentFrame(frame);
}


// Verify behavior for GetCurrentFrame() calls that happen after
// the renderer has been stopped.
TEST_F(VideoRendererBaseTest, GetCurrentFrame_AfterStop) {
  Initialize();

  EXPECT_CALL(*renderer_, OnStop(_))
      .WillOnce(DoAll(OnStop(), Return()))
      .RetiresOnSaturation();

  renderer_->Stop(NewExpectedClosure());

  scoped_refptr<VideoFrame> frame;
  renderer_->GetCurrentFrame(&frame);
  EXPECT_TRUE(!frame.get());
  renderer_->PutCurrentFrame(frame);

  renderer_ = NULL;
}

}  // namespace media
