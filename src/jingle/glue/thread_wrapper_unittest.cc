// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "jingle/glue/thread_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;

namespace jingle_glue {

static const uint32 kTestMessage1 = 1;
static const uint32 kTestMessage2 = 2;

static const int kTestDelayMs1 = 10;
static const int kTestDelayMs2 = 20;
static const int kTestDelayMs3 = 30;
static const int kTestDelayMs4 = 40;
static const int kMaxTestDelay = 40;

namespace {

class MockMessageHandler : public talk_base::MessageHandler {
 public:
  MOCK_METHOD1(OnMessage, void(talk_base::Message* msg));
};

MATCHER_P3(MatchMessage, handler, message_id, data, "") {
  return arg->phandler == handler &&
      arg->message_id == message_id &&
      arg->pdata == data;
}

ACTION(DeleteMessageData) {
  delete arg0->pdata;
}

// Helper class used in the Dispose test.
class DeletableObject {
 public:
  DeletableObject(bool* deleted)
      : deleted_(deleted) {
    *deleted = false;
  }

  ~DeletableObject() {
    *deleted_ = true;
  }

 private:
  bool* deleted_;
};

}  // namespace

class ThreadWrapperTest : public testing::Test {
 public:
  // This method is used by the SendDuringSend test. It sends message to the
  // main thread synchronously using Send().
  void PingMainThread() {
    talk_base::MessageData* data = new talk_base::MessageData();
    MockMessageHandler handler;

    EXPECT_CALL(handler, OnMessage(
        MatchMessage(&handler, kTestMessage2, data)))
        .WillOnce(DeleteMessageData());
    thread_->Send(&handler, kTestMessage2, data);
  }

 protected:
  ThreadWrapperTest()
      : thread_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    JingleThreadWrapper::EnsureForCurrentThread();
    thread_ = talk_base::Thread::Current();
  }

  // ThreadWrapper destroyes itself when |message_loop_| is destroyed.
  MessageLoop message_loop_;
  talk_base::Thread* thread_;
  MockMessageHandler handler1_;
  MockMessageHandler handler2_;
};

TEST_F(ThreadWrapperTest, Post) {
  talk_base::MessageData* data1 = new talk_base::MessageData();
  talk_base::MessageData* data2 = new talk_base::MessageData();
  talk_base::MessageData* data3 = new talk_base::MessageData();
  talk_base::MessageData* data4 = new talk_base::MessageData();

  thread_->Post(&handler1_, kTestMessage1, data1);
  thread_->Post(&handler1_, kTestMessage2, data2);
  thread_->Post(&handler2_, kTestMessage1, data3);
  thread_->Post(&handler2_, kTestMessage1, data4);

  InSequence in_seq;

  EXPECT_CALL(handler1_, OnMessage(
      MatchMessage(&handler1_, kTestMessage1, data1)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler1_, OnMessage(
      MatchMessage(&handler1_, kTestMessage2, data2)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_, OnMessage(
      MatchMessage(&handler2_, kTestMessage1, data3)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_, OnMessage(
      MatchMessage(&handler2_, kTestMessage1, data4)))
      .WillOnce(DeleteMessageData());

  message_loop_.RunAllPending();
}

TEST_F(ThreadWrapperTest, PostDelayed) {
  talk_base::MessageData* data1 = new talk_base::MessageData();
  talk_base::MessageData* data2 = new talk_base::MessageData();
  talk_base::MessageData* data3 = new talk_base::MessageData();
  talk_base::MessageData* data4 = new talk_base::MessageData();

  thread_->PostDelayed(kTestDelayMs1, &handler1_, kTestMessage1, data1);
  thread_->PostDelayed(kTestDelayMs2, &handler1_, kTestMessage2, data2);
  thread_->PostDelayed(kTestDelayMs3, &handler2_, kTestMessage1, data3);
  thread_->PostDelayed(kTestDelayMs4, &handler2_, kTestMessage1, data4);

  InSequence in_seq;

  EXPECT_CALL(handler1_, OnMessage(
      MatchMessage(&handler1_, kTestMessage1, data1)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler1_, OnMessage(
      MatchMessage(&handler1_, kTestMessage2, data2)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_, OnMessage(
      MatchMessage(&handler2_, kTestMessage1, data3)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_, OnMessage(
      MatchMessage(&handler2_, kTestMessage1, data4)))
      .WillOnce(DeleteMessageData());

  message_loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask(),
                                kMaxTestDelay);
  message_loop_.Run();
}

TEST_F(ThreadWrapperTest, Clear) {
  thread_->Post(&handler1_, kTestMessage1, NULL);
  thread_->Post(&handler1_, kTestMessage2, NULL);
  thread_->Post(&handler2_, kTestMessage1, NULL);
  thread_->Post(&handler2_, kTestMessage2, NULL);

  thread_->Clear(&handler1_, kTestMessage2);

  InSequence in_seq;

  talk_base::MessageData* null_data = NULL;
  EXPECT_CALL(handler1_, OnMessage(
      MatchMessage(&handler1_, kTestMessage1, null_data)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_, OnMessage(
      MatchMessage(&handler2_, kTestMessage1, null_data)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_, OnMessage(
      MatchMessage(&handler2_, kTestMessage2, null_data)))
      .WillOnce(DeleteMessageData());

  message_loop_.RunAllPending();
}

TEST_F(ThreadWrapperTest, ClearDelayed) {
  thread_->PostDelayed(kTestDelayMs1, &handler1_, kTestMessage1, NULL);
  thread_->PostDelayed(kTestDelayMs2, &handler1_, kTestMessage2, NULL);
  thread_->PostDelayed(kTestDelayMs3, &handler2_, kTestMessage1, NULL);
  thread_->PostDelayed(kTestDelayMs4, &handler2_, kTestMessage1, NULL);

  thread_->Clear(&handler1_, kTestMessage2);

  InSequence in_seq;

  talk_base::MessageData* null_data = NULL;
  EXPECT_CALL(handler1_, OnMessage(
      MatchMessage(&handler1_, kTestMessage1, null_data)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_, OnMessage(
      MatchMessage(&handler2_, kTestMessage1, null_data)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_, OnMessage(
      MatchMessage(&handler2_, kTestMessage1, null_data)))
      .WillOnce(DeleteMessageData());

  message_loop_.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask(),
                                kMaxTestDelay);
  message_loop_.Run();
}

// Verify that the queue is cleared when a handler is destroyed.
TEST_F(ThreadWrapperTest, ClearDestoroyed) {
  MockMessageHandler* handler_ptr;
  {
    MockMessageHandler handler;
    handler_ptr = &handler;
    thread_->Post(&handler, kTestMessage1, NULL);
  }
  talk_base::MessageList removed;
  thread_->Clear(handler_ptr, talk_base::MQID_ANY, &removed);
  DCHECK_EQ(0U, removed.size());
}

// Verify that Send() calls handler synchronously when called on the
// same thread.
TEST_F(ThreadWrapperTest, SendSameThread) {
  talk_base::MessageData* data = new talk_base::MessageData();

  EXPECT_CALL(handler1_, OnMessage(
      MatchMessage(&handler1_, kTestMessage1, data)))
      .WillOnce(DeleteMessageData());
  thread_->Send(&handler1_, kTestMessage1, data);
}

void InitializeWrapperForNewThread(talk_base::Thread** thread,
                                   base::WaitableEvent* done_event) {
  JingleThreadWrapper::EnsureForCurrentThread();
  JingleThreadWrapper::current()->set_send_allowed(true);
  *thread = JingleThreadWrapper::current();
  done_event->Signal();
}

// Verify that Send() calls handler synchronously when called for a
// different thread.
TEST_F(ThreadWrapperTest, SendToOtherThread) {
  JingleThreadWrapper::current()->set_send_allowed(true);

  base::Thread second_thread("JingleThreadWrapperTest");
  second_thread.Start();

  base::WaitableEvent initialized_event(true, false);
  talk_base::Thread* target;
  second_thread.message_loop()->PostTask(
      FROM_HERE, base::Bind(&InitializeWrapperForNewThread,
                            &target, &initialized_event));
  initialized_event.Wait();

  ASSERT_TRUE(target != NULL);

  talk_base::MessageData* data = new talk_base::MessageData();

  EXPECT_CALL(handler1_, OnMessage(
      MatchMessage(&handler1_, kTestMessage1, data)))
      .WillOnce(DeleteMessageData());
  target->Send(&handler1_, kTestMessage1, data);

  Mock::VerifyAndClearExpectations(&handler1_);
}

// Verify that thread handles Send() while another Send() is
// pending. The test creates second thread and Send()s kTestMessage1
// to that thread. kTestMessage1 handler calls PingMainThread() which
// tries to Send() kTestMessage2 to the main thread.
TEST_F(ThreadWrapperTest, SendDuringSend) {
  JingleThreadWrapper::current()->set_send_allowed(true);

  base::Thread second_thread("JingleThreadWrapperTest");
  second_thread.Start();

  base::WaitableEvent initialized_event(true, false);
  talk_base::Thread* target;
  second_thread.message_loop()->PostTask(
      FROM_HERE, base::Bind(&InitializeWrapperForNewThread,
                            &target, &initialized_event));
  initialized_event.Wait();

  ASSERT_TRUE(target != NULL);

  talk_base::MessageData* data = new talk_base::MessageData();

  EXPECT_CALL(handler1_, OnMessage(
      MatchMessage(&handler1_, kTestMessage1, data)))
      .WillOnce(DoAll(
          InvokeWithoutArgs(
              this, &ThreadWrapperTest::PingMainThread),
          DeleteMessageData()));
  target->Send(&handler1_, kTestMessage1, data);

  Mock::VerifyAndClearExpectations(&handler1_);
}

TEST_F(ThreadWrapperTest, Dispose) {
  bool deleted_;
  thread_->Dispose(new DeletableObject(&deleted_));
  EXPECT_FALSE(deleted_);
  message_loop_.RunAllPending();
  EXPECT_TRUE(deleted_);
}

}  // namespace jingle_glue
