// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/platform_thread.h"
#include "media/base/buffers.h"
#include "media/base/filter_host.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/filters/video_renderer_base.h"

namespace media {

// This equates to ~16.67 fps, which is just slow enough to be tolerable when
// our video renderer is ahead of the audio playback.
//
// A higher value will be a slower frame rate, which looks worse but allows the
// audio renderer to catch up faster.  A lower value will be a smoother frame
// rate, but results in the video being out of sync for longer.
static const int64 kMaxSleepMilliseconds = 60;

// The number of milliseconds to idle when we do not have anything to do.
// Nothing special about the value, other than we're being more OS-friendly
// than sleeping for 1 millisecond.
static const int kIdleMilliseconds = 10;

VideoRendererBase::VideoRendererBase()
    : frame_available_(&lock_),
      state_(kUninitialized),
      thread_(base::kNullThreadHandle),
      pending_reads_(0),
      pending_paint_(false),
      pending_paint_with_last_available_(false),
      playback_rate_(0) {
}

VideoRendererBase::~VideoRendererBase() {
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == kUninitialized || state_ == kStopped) << state_;
}

void VideoRendererBase::Play(const base::Closure& callback) {
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(kPrerolled, state_);
  state_ = kPlaying;
  callback.Run();
}

void VideoRendererBase::Pause(const base::Closure& callback) {
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ != kUninitialized || state_ == kError);
  state_ = kPaused;
  callback.Run();
}

void VideoRendererBase::Flush(const base::Closure& callback) {
  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kPaused);
  flush_callback_ = callback;
  state_ = kFlushing;

  if (!pending_paint_)
    FlushBuffers_Locked();
}

void VideoRendererBase::Stop(const base::Closure& callback) {
  base::PlatformThreadHandle old_thread_handle = base::kNullThreadHandle;
  {
    base::AutoLock auto_lock(lock_);
    state_ = kStopped;

    if (!pending_paint_ && !pending_paint_with_last_available_)
      DoStopOrErrorFlush_Locked();

    // Clean up our thread if present.
    if (thread_) {
      // Signal the thread since it's possible to get stopped with the video
      // thread waiting for a read to complete.
      frame_available_.Signal();
      old_thread_handle = thread_;
      thread_ = base::kNullThreadHandle;
    }
  }
  if (old_thread_handle)
    base::PlatformThread::Join(old_thread_handle);

  // Signal the subclass we're stopping.
  OnStop(callback);
}

void VideoRendererBase::SetPlaybackRate(float playback_rate) {
  base::AutoLock auto_lock(lock_);
  playback_rate_ = playback_rate;
}

void VideoRendererBase::Seek(base::TimeDelta time, const FilterStatusCB& cb) {
  bool run_callback = false;

  {
    base::AutoLock auto_lock(lock_);
    // There is a race condition between filters to receive SeekTask().
    // It turns out we could receive buffer from decoder before seek()
    // is called on us. so we do the following:
    // kFlushed => ( Receive first buffer or Seek() ) => kSeeking and
    // kSeeking => ( Receive enough buffers) => kPrerolled. )
    DCHECK(state_ == kPrerolled || state_ == kFlushed || state_ == kSeeking);
    DCHECK(!cb.is_null());
    DCHECK(seek_cb_.is_null());

    if (state_ == kPrerolled) {
      // Already get enough buffers from decoder.
      run_callback = true;
    } else {
      // Otherwise we are either kFlushed or kSeeking, but without enough
      // buffers we should save the callback function and call it later.
      state_ = kSeeking;
      seek_cb_ = cb;
    }

    seek_timestamp_ = time;
    ScheduleRead_Locked();
  }

  if (run_callback)
    cb.Run(PIPELINE_OK);
}

void VideoRendererBase::Initialize(VideoDecoder* decoder,
                                   const base::Closure& callback,
                                   const StatisticsCallback& stats_callback) {
  base::AutoLock auto_lock(lock_);
  DCHECK(decoder);
  DCHECK(!callback.is_null());
  DCHECK(!stats_callback.is_null());
  DCHECK_EQ(kUninitialized, state_);
  decoder_ = decoder;

  statistics_callback_ = stats_callback;

  decoder_->set_consume_video_frame_callback(
      base::Bind(&VideoRendererBase::ConsumeVideoFrame,
                 base::Unretained(this)));

  // Notify the pipeline of the video dimensions.
  host()->SetNaturalVideoSize(decoder_->natural_size());

  // Initialize the subclass.
  // TODO(scherkus): do we trust subclasses not to do something silly while
  // we're holding the lock?
  if (!OnInitialize(decoder)) {
    EnterErrorState_Locked(PIPELINE_ERROR_INITIALIZATION_FAILED);
    callback.Run();
    return;
  }

  // We're all good!  Consider ourselves flushed. (ThreadMain() should never
  // see us in the kUninitialized state).
  // Since we had an initial Seek, we consider ourself flushed, because we
  // have not populated any buffers yet.
  state_ = kFlushed;

  // Create our video thread.
  if (!base::PlatformThread::Create(0, this, &thread_)) {
    NOTREACHED() << "Video thread creation failed";
    EnterErrorState_Locked(PIPELINE_ERROR_INITIALIZATION_FAILED);
    callback.Run();
    return;
  }

#if defined(OS_WIN)
  // Bump up our priority so our sleeping is more accurate.
  // TODO(scherkus): find out if this is necessary, but it seems to help.
  ::SetThreadPriority(thread_, THREAD_PRIORITY_ABOVE_NORMAL);
#endif  // defined(OS_WIN)
  callback.Run();
}

bool VideoRendererBase::HasEnded() {
  base::AutoLock auto_lock(lock_);
  return state_ == kEnded;
}

// PlatformThread::Delegate implementation.
void VideoRendererBase::ThreadMain() {
  base::PlatformThread::SetName("CrVideoRenderer");
  base::TimeDelta remaining_time;

  uint32 frames_dropped = 0;

  for (;;) {
    if (frames_dropped > 0) {
      PipelineStatistics statistics;
      statistics.video_frames_dropped = frames_dropped;
      statistics_callback_.Run(statistics);

      frames_dropped = 0;
    }

    base::AutoLock auto_lock(lock_);

    const base::TimeDelta kIdleTimeDelta =
        base::TimeDelta::FromMilliseconds(kIdleMilliseconds);

    if (state_ == kStopped)
      return;

    if (state_ != kPlaying || playback_rate_ == 0) {
      remaining_time = kIdleTimeDelta;
    } else if (frames_queue_ready_.empty() ||
               frames_queue_ready_.front()->IsEndOfStream()) {
      if (current_frame_.get())
        remaining_time = CalculateSleepDuration(NULL, playback_rate_);
      else
        remaining_time = kIdleTimeDelta;
    } else {
      // Calculate how long until we should advance the frame, which is
      // typically negative but for playback rates < 1.0f may be long enough
      // that it makes more sense to idle and check again.
      scoped_refptr<VideoFrame> next_frame = frames_queue_ready_.front();
      remaining_time = CalculateSleepDuration(next_frame, playback_rate_);
    }

    // TODO(jiesun): I do not think we should wake up every 10ms.
    // We should only wait up when following is true:
    // 1. frame arrival (use event);
    // 2. state_ change (use event);
    // 3. playback_rate_ change (use event);
    // 4. next frame's pts (use timeout);
    if (remaining_time > kIdleTimeDelta)
      remaining_time = kIdleTimeDelta;

    // We can not do anything about this until next frame arrival.
    // We do not want to spin in this case though.
    if (remaining_time.InMicroseconds() < 0 && frames_queue_ready_.empty())
      remaining_time = kIdleTimeDelta;

    if (remaining_time.InMicroseconds() > 0)
      frame_available_.TimedWait(remaining_time);

    if (state_ != kPlaying || playback_rate_ == 0)
      continue;

    // Otherwise we're playing, so advance the frame and keep reading from the
    // decoder when following condition is satisfied:
    // 1. We had at least one backup frame.
    // 2. We had not reached end of stream.
    // 3. Current frame is out-dated.
    if (frames_queue_ready_.empty())
      continue;

    scoped_refptr<VideoFrame> next_frame = frames_queue_ready_.front();
    if (next_frame->IsEndOfStream()) {
      state_ = kEnded;
      DVLOG(1) << "Video render gets EOS";
      host()->NotifyEnded();
      continue;
    }

    // Normally we're ready to loop again at this point, but there are
    // exceptions that cause us to drop a frame and/or consider painting a
    // "next" frame.
    if (next_frame->GetTimestamp() > host()->GetTime() + kIdleTimeDelta &&
        current_frame_ &&
        current_frame_->GetTimestamp() <= host()->GetDuration()) {
      continue;
    }

    // If we got here then:
    // 1. next frame's timestamp is already current; or
    // 2. we do not have any current frame yet anyway; or
    // 3. a special case when the stream is badly formatted and
    //    we got a frame with timestamp greater than overall duration.
    //    In this case we should proceed anyway and try to obtain the
    //    end-of-stream packet.
    scoped_refptr<VideoFrame> timeout_frame;
    bool new_frame_available = false;
    if (!pending_paint_) {
      // In this case, current frame could be recycled.
      timeout_frame = current_frame_;
      current_frame_ = frames_queue_ready_.front();
      frames_queue_ready_.pop_front();
      new_frame_available = true;
    } else if (pending_paint_ && frames_queue_ready_.size() >= 2 &&
               !frames_queue_ready_[1]->IsEndOfStream()) {
      // Painter could be really slow, therefore we had to skip frames if
      // 1. We had not reached end of stream.
      // 2. The next frame of current frame is also out-dated.
      base::TimeDelta next_remaining_time =
          frames_queue_ready_[1]->GetTimestamp() - host()->GetTime();
      if (next_remaining_time.InMicroseconds() <= 0) {
        // Since the current frame is still hold by painter/compositor, and
        // the next frame is already timed-out, we should skip the next frame
        // which is the first frame in the queue.
        timeout_frame = frames_queue_ready_.front();
        frames_queue_ready_.pop_front();
        ++frames_dropped;
      }
    }
    if (timeout_frame.get()) {
      frames_queue_done_.push_back(timeout_frame);
      ScheduleRead_Locked();
    }
    if (new_frame_available) {
      base::AutoUnlock auto_unlock(lock_);
      OnFrameAvailable();
    }
  }
}

void VideoRendererBase::GetCurrentFrame(scoped_refptr<VideoFrame>* frame_out) {
  base::AutoLock auto_lock(lock_);
  DCHECK(!pending_paint_ && !pending_paint_with_last_available_);

  if ((!current_frame_.get() || current_frame_->IsEndOfStream()) &&
      (!last_available_frame_.get() ||
       last_available_frame_->IsEndOfStream())) {
    *frame_out = NULL;
    return;
  }

  // We should have initialized and have the current frame.
  DCHECK(state_ != kUninitialized && state_ != kStopped && state_ != kError);

  if (current_frame_) {
    *frame_out = current_frame_;
    last_available_frame_ = current_frame_;
    pending_paint_ = true;
  } else {
    DCHECK(last_available_frame_.get() != NULL);
    *frame_out = last_available_frame_;
    pending_paint_with_last_available_ = true;
  }
}

void VideoRendererBase::PutCurrentFrame(scoped_refptr<VideoFrame> frame) {
  base::AutoLock auto_lock(lock_);

  // Note that we do not claim |pending_paint_| when we return NULL frame, in
  // that case, |current_frame_| could be changed before PutCurrentFrame.
  if (pending_paint_) {
    DCHECK(current_frame_.get() == frame.get());
    DCHECK(!pending_paint_with_last_available_);
    pending_paint_ = false;
  } else if (pending_paint_with_last_available_) {
    DCHECK(last_available_frame_.get() == frame.get());
    DCHECK(!pending_paint_);
    pending_paint_with_last_available_ = false;
  } else {
    DCHECK(frame.get() == NULL);
  }

  // We had cleared the |pending_paint_| flag, there are chances that current
  // frame is timed-out. We will wake up our main thread to advance the current
  // frame when this is true.
  frame_available_.Signal();
  if (state_ == kFlushing) {
    FlushBuffers_Locked();
  } else if (state_ == kError || state_ == kStopped) {
    DoStopOrErrorFlush_Locked();
  }
}

void VideoRendererBase::ConsumeVideoFrame(scoped_refptr<VideoFrame> frame) {
  if (frame) {
    PipelineStatistics statistics;
    statistics.video_frames_decoded = 1;
    statistics_callback_.Run(statistics);
  }

  base::AutoLock auto_lock(lock_);

  if (!frame) {
    EnterErrorState_Locked(PIPELINE_ERROR_DECODE);
    return;
  }

  // Decoder could reach seek state before our Seek() get called.
  // We will enter kSeeking
  if (state_ == kFlushed)
    state_ = kSeeking;

  // Synchronous flush between filters should prevent this from happening.
  DCHECK_NE(state_, kStopped);
  if (frame.get() && !frame->IsEndOfStream())
    --pending_reads_;

  DCHECK(state_ != kUninitialized && state_ != kStopped && state_ != kError);

  if (state_ == kPaused || state_ == kFlushing) {
    // Decoder are flushing rubbish video frame, we will not display them.
    if (frame.get() && !frame->IsEndOfStream())
      frames_queue_done_.push_back(frame);
    DCHECK_LE(frames_queue_done_.size(),
              static_cast<size_t>(Limits::kMaxVideoFrames));

    // Excluding kPause here, because in pause state, we will never
    // transfer out-bounding buffer. We do not flush buffer when Compositor
    // hold reference to our video frame either.
    if (state_ == kFlushing && pending_paint_ == false)
      FlushBuffers_Locked();

    return;
  }

  // Discard frames until we reach our desired seek timestamp.
  if (state_ == kSeeking && !frame->IsEndOfStream() &&
      (frame->GetTimestamp() + frame->GetDuration()) <= seek_timestamp_) {
    frames_queue_done_.push_back(frame);
    ScheduleRead_Locked();
  } else {
    frames_queue_ready_.push_back(frame);
    DCHECK_LE(frames_queue_ready_.size(),
              static_cast<size_t>(Limits::kMaxVideoFrames));
    frame_available_.Signal();
  }

  // Check for our preroll complete condition.
  bool new_frame_available = false;
  if (state_ == kSeeking) {
    if (frames_queue_ready_.size() == Limits::kMaxVideoFrames ||
        frame->IsEndOfStream()) {
      // We're paused, so make sure we update |current_frame_| to represent
      // our new location.
      state_ = kPrerolled;

      // Because we might remain paused (i.e., we were not playing before we
      // received a seek), we can't rely on ThreadMain() to notify the subclass
      // the frame has been updated.
      scoped_refptr<VideoFrame> first_frame;
      first_frame = frames_queue_ready_.front();
      if (!first_frame->IsEndOfStream()) {
        frames_queue_ready_.pop_front();
        current_frame_ = first_frame;
      }
      new_frame_available = true;

      // If we reach prerolled state before Seek() is called by pipeline,
      // |seek_callback_| is not set, we will return immediately during
      // when Seek() is eventually called.
      if (!seek_cb_.is_null()) {
        ResetAndRunCB(&seek_cb_, PIPELINE_OK);
      }
    }
  } else if (state_ == kFlushing && pending_reads_ == 0 && !pending_paint_) {
    OnFlushDone_Locked();
  }

  if (new_frame_available) {
    base::AutoUnlock auto_unlock(lock_);
    OnFrameAvailable();
  }
}

void VideoRendererBase::ReadInput(scoped_refptr<VideoFrame> frame) {
  // We should never return empty frames or EOS frame.
  DCHECK(frame.get() && !frame->IsEndOfStream());

  decoder_->ProduceVideoFrame(frame);
  ++pending_reads_;
}

void VideoRendererBase::ScheduleRead_Locked() {
  lock_.AssertAcquired();
  DCHECK_NE(kEnded, state_);
  // TODO(jiesun): We use dummy buffer to feed decoder to let decoder to
  // provide buffer pools. In the future, we may want to implement real
  // buffer pool to recycle buffers.
  while (!frames_queue_done_.empty()) {
    scoped_refptr<VideoFrame> video_frame = frames_queue_done_.front();
    frames_queue_done_.pop_front();
    ReadInput(video_frame);
  }
}

void VideoRendererBase::FlushBuffers_Locked() {
  lock_.AssertAcquired();
  DCHECK(!pending_paint_);

  // We should never put EOF frame into "done queue".
  while (!frames_queue_ready_.empty()) {
    scoped_refptr<VideoFrame> video_frame = frames_queue_ready_.front();
    if (!video_frame->IsEndOfStream()) {
      frames_queue_done_.push_back(video_frame);
    }
    frames_queue_ready_.pop_front();
  }
  if (current_frame_.get() && !current_frame_->IsEndOfStream()) {
    frames_queue_done_.push_back(current_frame_);
  }
  current_frame_ = NULL;

  // Flush all buffers out to decoder.
  ScheduleRead_Locked();

  if (pending_reads_ == 0 && state_ == kFlushing)
    OnFlushDone_Locked();
}

void VideoRendererBase::OnFlushDone_Locked() {
  lock_.AssertAcquired();
  // Check all buffers are returned to owners.
  DCHECK_EQ(frames_queue_done_.size(), 0u);
  DCHECK(!current_frame_.get());
  DCHECK(frames_queue_ready_.empty());

  if (!flush_callback_.is_null())  // This ensures callback is invoked once.
    ResetAndRunCB(&flush_callback_);

  state_ = kFlushed;
}

base::TimeDelta VideoRendererBase::CalculateSleepDuration(
    VideoFrame* next_frame, float playback_rate) {
  // Determine the current and next presentation timestamps.
  base::TimeDelta now = host()->GetTime();
  base::TimeDelta this_pts = current_frame_->GetTimestamp();
  base::TimeDelta next_pts;
  if (next_frame) {
    next_pts = next_frame->GetTimestamp();
  } else {
    next_pts = this_pts + current_frame_->GetDuration();
  }

  // Determine our sleep duration based on whether time advanced.
  base::TimeDelta sleep;
  if (now == previous_time_) {
    // Time has not changed, assume we sleep for the frame's duration.
    sleep = next_pts - this_pts;
  } else {
    // Time has changed, figure out real sleep duration.
    sleep = next_pts - now;
    previous_time_ = now;
  }

  // Scale our sleep based on the playback rate.
  // TODO(scherkus): floating point badness and degrade gracefully.
  return base::TimeDelta::FromMicroseconds(
      static_cast<int64>(sleep.InMicroseconds() / playback_rate));
}

void VideoRendererBase::EnterErrorState_Locked(PipelineStatus status) {
  lock_.AssertAcquired();

  base::Closure callback;
  State old_state = state_;
  state_ = kError;

  // Flush frames if we aren't in the middle of a paint. If we
  // are painting then flushing will happen when the paint completes.
  if (!pending_paint_ && !pending_paint_with_last_available_)
    DoStopOrErrorFlush_Locked();

  switch (old_state) {
    case kUninitialized:
    case kPrerolled:
    case kPaused:
    case kFlushed:
    case kEnded:
    case kPlaying:
      break;

    case kFlushing:
      CHECK(!flush_callback_.is_null());
      std::swap(callback, flush_callback_);
      break;

    case kSeeking:
      CHECK(!seek_cb_.is_null());
      ResetAndRunCB(&seek_cb_, status);
      return;
      break;

    case kStopped:
      NOTREACHED() << "Should not error if stopped.";
      return;

    case kError:
      return;
  }

  host()->SetError(status);

  if (!callback.is_null())
    callback.Run();
}

void VideoRendererBase::DoStopOrErrorFlush_Locked() {
  DCHECK(!pending_paint_);
  DCHECK(!pending_paint_with_last_available_);
  lock_.AssertAcquired();
  FlushBuffers_Locked();
  last_available_frame_ = NULL;
  DCHECK_EQ(pending_reads_, 0);
}

}  // namespace media
