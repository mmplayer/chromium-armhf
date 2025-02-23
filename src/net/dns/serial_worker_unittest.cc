// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/serial_worker.h"

#include "base/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class SerialWorkerTest : public testing::Test {
 public:
  // The class under test
  class TestSerialWorker : public SerialWorker {
   public:
    explicit TestSerialWorker(SerialWorkerTest* t)
      : test_(t) {}
    virtual void DoWork() OVERRIDE {
      ASSERT_TRUE(test_);
      test_->OnWork();
    }
    virtual void OnWorkFinished() OVERRIDE {
      ASSERT_TRUE(test_);
      test_->OnWorkFinished();
    }
   private:
    virtual ~TestSerialWorker() {}
    SerialWorkerTest* test_;
  };

  // Mocks

  void OnWork() {
    { // Check that OnWork is executed serially.
      base::AutoLock lock(work_lock_);
      EXPECT_FALSE(work_running_) << "DoRead is not called serially!";
      work_running_ = true;
    }
    BreakNow("OnWork");
    work_allowed_.Wait();
    // Calling from WorkerPool, but protected by work_allowed_/work_called_.
    output_value_ = input_value_;

    { // This lock might be destroyed after work_called_ is signalled.
      base::AutoLock lock(work_lock_);
      work_running_ = false;
    }
    work_called_.Signal();
  }

  void OnWorkFinished() {
    EXPECT_TRUE(message_loop_ == MessageLoop::current());
    EXPECT_EQ(output_value_, input_value_);
    BreakNow("OnWorkFinished");
  }

 protected:
  friend class BreakTask;
  class BreakTask : public Task {
   public:
    BreakTask(SerialWorkerTest* test, std::string breakpoint)
      : test_(test), breakpoint_(breakpoint) {}
    virtual ~BreakTask() {}
    virtual void Run() OVERRIDE {
      test_->breakpoint_ = breakpoint_;
      MessageLoop::current()->QuitNow();
    }
   private:
    SerialWorkerTest* test_;
    std::string breakpoint_;
  };

  void BreakNow(std::string b) {
    message_loop_->PostTask(FROM_HERE, new BreakTask(this, b));
  }

  void RunUntilBreak(std::string b) {
    message_loop_->Run();
    ASSERT_EQ(breakpoint_, b);
  }

  SerialWorkerTest()
      : input_value_(0),
        output_value_(-1),
        work_allowed_(false, false),
        work_called_(false, false),
        work_running_(false) {
  }

  // Helpers for tests.

  // Lets OnWork run and waits for it to complete. Can only return if OnWork is
  // executed on a concurrent thread.
  void WaitForWork() {
    RunUntilBreak("OnWork");
    work_allowed_.Signal();
    work_called_.Wait();
  }

  // test::Test methods
  virtual void SetUp() OVERRIDE {
    message_loop_ = MessageLoop::current();
    worker_ = new TestSerialWorker(this);
  }

  virtual void TearDown() OVERRIDE {
    // Cancel the worker to catch if it makes a late DoWork call.
    worker_->Cancel();
    // Check if OnWork is stalled.
    EXPECT_FALSE(work_running_) << "OnWork should be done by TearDown";
    // Release it for cleanliness.
    if (work_running_) {
      WaitForWork();
    }
  }

  // Input value read on WorkerPool.
  int input_value_;
  // Output value written on WorkerPool.
  int output_value_;

  // read is called on WorkerPool so we need to synchronize with it.
  base::WaitableEvent work_allowed_;
  base::WaitableEvent work_called_;

  // Protected by read_lock_. Used to verify that read calls are serialized.
  bool work_running_;
  base::Lock work_lock_;

  // Loop for this thread.
  MessageLoop* message_loop_;

  // WatcherDelegate under test.
  scoped_refptr<TestSerialWorker> worker_;

  std::string breakpoint_;
};

TEST_F(SerialWorkerTest, ExecuteAndSerializeReads) {
  for (int i = 0; i < 3; ++i) {
    ++input_value_;
    worker_->WorkNow();
    WaitForWork();
    RunUntilBreak("OnWorkFinished");

    message_loop_->AssertIdle();
  }

  // Schedule two calls. OnWork checks if it is called serially.
  ++input_value_;
  worker_->WorkNow();
  // read is blocked, so this will have to induce re-work
  worker_->WorkNow();
  WaitForWork();
  WaitForWork();
  RunUntilBreak("OnWorkFinished");

  // No more tasks should remain.
  message_loop_->AssertIdle();
}

}  // namespace

}  // namespace net

