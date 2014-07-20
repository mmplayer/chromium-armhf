// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_video_decoder.h"

#include <deque>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "media/base/demuxer_stream.h"
#include "media/base/filters.h"
#include "media/base/filter_host.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/video/ffmpeg_video_decode_engine.h"
#include "media/video/video_decode_context.h"
#include "ui/gfx/rect.h"

namespace media {

FFmpegVideoDecoder::FFmpegVideoDecoder(MessageLoop* message_loop,
                                       VideoDecodeContext* decode_context)
    : message_loop_(message_loop),
      state_(kUnInitialized),
      decode_engine_(new FFmpegVideoDecodeEngine()),
      decode_context_(decode_context) {
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() {}

void FFmpegVideoDecoder::Initialize(DemuxerStream* demuxer_stream,
                                    const base::Closure& callback,
                                    const StatisticsCallback& stats_callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&FFmpegVideoDecoder::Initialize, this,
                   make_scoped_refptr(demuxer_stream),
                   callback, stats_callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(!demuxer_stream_);
  DCHECK(initialize_callback_.is_null());

  if (!demuxer_stream) {
    host()->SetError(PIPELINE_ERROR_DECODE);
    callback.Run();
    return;
  }

  demuxer_stream_ = demuxer_stream;
  initialize_callback_ = callback;
  statistics_callback_ = stats_callback;

  AVStream* av_stream = demuxer_stream->GetAVStream();
  if (!av_stream) {
    OnInitializeComplete(false);
    return;
  }

  pts_stream_.Initialize(GetFrameDuration(av_stream));

  gfx::Size coded_size(
      av_stream->codec->coded_width, av_stream->codec->coded_height);
  // TODO(vrk): This assumes decoded frame data starts at (0, 0), which is true
  // for now, but may not always be true forever. Fix this in the future.
  gfx::Rect visible_rect(
      av_stream->codec->width, av_stream->codec->height);

  natural_size_ = GetNaturalSize(av_stream);
  if (natural_size_.width() > Limits::kMaxDimension ||
      natural_size_.height() > Limits::kMaxDimension ||
      natural_size_.GetArea() > Limits::kMaxCanvas) {
    OnInitializeComplete(false);
    return;
  }

  VideoDecoderConfig config(CodecIDToVideoCodec(av_stream->codec->codec_id),
                            PixelFormatToVideoFormat(av_stream->codec->pix_fmt),
                            coded_size, visible_rect,
                            av_stream->r_frame_rate.num,
                            av_stream->r_frame_rate.den,
                            av_stream->codec->extradata,
                            av_stream->codec->extradata_size);

  state_ = kInitializing;
  decode_engine_->Initialize(message_loop_, this, NULL, config);
}

void FFmpegVideoDecoder::OnInitializeComplete(bool success) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(!initialize_callback_.is_null());

  if (success) {
    state_ = kNormal;
  } else {
    host()->SetError(PIPELINE_ERROR_DECODE);
  }
  ResetAndRunCB(&initialize_callback_);
}

void FFmpegVideoDecoder::Stop(const base::Closure& callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FFmpegVideoDecoder::Stop, this, callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(uninitialize_callback_.is_null());

  uninitialize_callback_ = callback;
  if (state_ != kUnInitialized)
    decode_engine_->Uninitialize();
  else
    OnUninitializeComplete();
}

void FFmpegVideoDecoder::OnUninitializeComplete() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(!uninitialize_callback_.is_null());

  state_ = kStopped;

  ResetAndRunCB(&uninitialize_callback_);
}

void FFmpegVideoDecoder::Pause(const base::Closure& callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FFmpegVideoDecoder::Pause, this, callback));
    return;
  }

  state_ = kPausing;
  callback.Run();
}

void FFmpegVideoDecoder::Flush(const base::Closure& callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FFmpegVideoDecoder::Flush, this, callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(flush_callback_.is_null());

  state_ = kFlushing;

  FlushBuffers();

  flush_callback_ = callback;

  decode_engine_->Flush();
}

void FFmpegVideoDecoder::OnFlushComplete() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(!flush_callback_.is_null());

  // Everything in the presentation time queue is invalid, clear the queue.
  pts_stream_.Flush();

  // Mark flush operation had been done.
  state_ = kNormal;

  ResetAndRunCB(&flush_callback_);
}

void FFmpegVideoDecoder::Seek(base::TimeDelta time, const FilterStatusCB& cb) {
  if (MessageLoop::current() != message_loop_) {
     message_loop_->PostTask(FROM_HERE,
                             base::Bind(&FFmpegVideoDecoder::Seek, this,
                                               time, cb));
     return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(seek_cb_.is_null());

  pts_stream_.Seek(time);
  seek_cb_ = cb;
  decode_engine_->Seek();
}

void FFmpegVideoDecoder::OnSeekComplete() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(!seek_cb_.is_null());

  ResetAndRunCB(&seek_cb_, PIPELINE_OK);
}

void FFmpegVideoDecoder::OnError() {
  VideoFrameReady(NULL);
}

void FFmpegVideoDecoder::OnReadComplete(Buffer* buffer_in) {
  scoped_refptr<Buffer> buffer(buffer_in);
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &FFmpegVideoDecoder::OnReadCompleteTask, this, buffer));
}

void FFmpegVideoDecoder::OnReadCompleteTask(scoped_refptr<Buffer> buffer) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK_NE(state_, kStopped);  // because of Flush() before Stop().

  // During decode, because reads are issued asynchronously, it is possible to
  // receive multiple end of stream buffers since each read is acked. When the
  // first end of stream buffer is read, FFmpeg may still have frames queued
  // up in the decoder so we need to go through the decode loop until it stops
  // giving sensible data.  After that, the decoder should output empty
  // frames.  There are three states the decoder can be in:
  //
  //   kNormal: This is the starting state. Buffers are decoded. Decode errors
  //            are discarded.
  //   kFlushCodec: There isn't any more input data. Call avcodec_decode_video2
  //                until no more data is returned to flush out remaining
  //                frames. The input buffer is ignored at this point.
  //   kDecodeFinished: All calls return empty frames.
  //
  // These are the possible state transitions.
  //
  // kNormal -> kFlushCodec:
  //     When buffer->IsEndOfStream() is first true.
  // kNormal -> kDecodeFinished:
  //     A catastrophic failure occurs, and decoding needs to stop.
  // kFlushCodec -> kDecodeFinished:
  //     When avcodec_decode_video2() returns 0 data or errors out.
  // (any state) -> kNormal:
  //     Any time buffer->IsDiscontinuous() is true.

  // Transition to kFlushCodec on the first end of stream buffer.
  if (state_ == kNormal && buffer->IsEndOfStream()) {
    state_ = kFlushCodec;
  }

  // Push all incoming timestamps into the priority queue as long as we have
  // not yet received an end of stream buffer.  It is important that this line
  // stay below the state transition into kFlushCodec done above.
  if (state_ == kNormal) {
    pts_stream_.EnqueuePts(buffer.get());
  }

  // Otherwise, attempt to decode a single frame.
  decode_engine_->ConsumeVideoSample(buffer);
}

void FFmpegVideoDecoder::ProduceVideoFrame(
    scoped_refptr<VideoFrame> video_frame) {
  if (MessageLoop::current() != message_loop_) {
    if (state_ != kStopped) {
      message_loop_->PostTask(FROM_HERE, base::Bind(
          &FFmpegVideoDecoder::ProduceVideoFrame, this, video_frame));
    }
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);

  // Synchronized flushing before stop should prevent this.
  DCHECK_NE(state_, kStopped);

  // If the decoding is finished, we just always return empty frames.
  if (state_ == kDecodeFinished) {
    // Signal VideoRenderer the end of the stream event.
    VideoFrameReady(VideoFrame::CreateEmptyFrame());

    // Fall through, because we still need to keep record of this frame.
  }

  // Notify decode engine the available of new frame.
  decode_engine_->ProduceVideoFrame(video_frame);
}

void FFmpegVideoDecoder::ConsumeVideoFrame(
    scoped_refptr<VideoFrame> video_frame,
    const PipelineStatistics& statistics) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK_NE(state_, kStopped);

  statistics_callback_.Run(statistics);

  if (video_frame.get()) {
    if (kPausing == state_ || kFlushing == state_) {
      frame_queue_flushed_.push_back(video_frame);
      if (kFlushing == state_)
        FlushBuffers();
      return;
    }

    // If we actually got data back, enqueue a frame.
    pts_stream_.UpdatePtsAndDuration(video_frame.get());

    video_frame->SetTimestamp(pts_stream_.current_pts());
    video_frame->SetDuration(pts_stream_.current_duration());

    VideoFrameReady(video_frame);
  } else {
    // When in kFlushCodec, any errored decode, or a 0-lengthed frame,
    // is taken as a signal to stop decoding.
    if (state_ == kFlushCodec) {
      state_ = kDecodeFinished;

      // Signal VideoRenderer the end of the stream event.
      VideoFrameReady(VideoFrame::CreateEmptyFrame());
    }
  }
}

void FFmpegVideoDecoder::ProduceVideoSample(
    scoped_refptr<Buffer> buffer) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK_NE(state_, kStopped);

  demuxer_stream_->Read(base::Bind(&FFmpegVideoDecoder::OnReadComplete,
                                   this));
}

gfx::Size FFmpegVideoDecoder::natural_size() {
  return natural_size_;
}

void FFmpegVideoDecoder::FlushBuffers() {
  while (!frame_queue_flushed_.empty()) {
    scoped_refptr<VideoFrame> video_frame;
    video_frame = frame_queue_flushed_.front();
    frame_queue_flushed_.pop_front();

    // Return frames back to the decode engine.
    decode_engine_->ProduceVideoFrame(video_frame);
  }
}

void FFmpegVideoDecoder::SetVideoDecodeEngineForTest(
    VideoDecodeEngine* engine) {
  decode_engine_.reset(engine);
}

}  // namespace media
