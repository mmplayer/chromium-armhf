// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/thread_test_helper.h"
#include "base/time.h"
#include "chrome/browser/net/sqlite_persistent_cookie_store.h"
#include "chrome/common/chrome_constants.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

class SQLitePersistentCookieStoreTest : public testing::Test {
 public:
  SQLitePersistentCookieStoreTest()
      : ui_thread_(BrowserThread::UI),
        db_thread_(BrowserThread::DB),
        io_thread_(BrowserThread::IO),
        loaded_event_(false, false),
        key_loaded_event_(false, false),
        db_thread_event_(false, false) {
  }

  void OnLoaded(
      const std::vector<net::CookieMonster::CanonicalCookie*>& cookies) {
    cookies_ = cookies;
    loaded_event_.Signal();
  }

  void OnKeyLoaded(
      const std::vector<net::CookieMonster::CanonicalCookie*>& cookies) {
    cookies_ = cookies;
    key_loaded_event_.Signal();
  }

  void Load(std::vector<net::CookieMonster::CanonicalCookie*>* cookies) {
    store_->Load(base::Bind(&SQLitePersistentCookieStoreTest::OnLoaded,
                                base::Unretained(this)));
    loaded_event_.Wait();
    *cookies = cookies_;
  }

  // We have to create this method to wrap WaitableEvent::Wait, since we cannot
  // bind a non-void returning method as a Closure.
  void WaitOnDBEvent() {
    db_thread_event_.Wait();
  }

  virtual void SetUp() {
    ui_thread_.Start();
    db_thread_.Start();
    io_thread_.Start();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    store_ = new SQLitePersistentCookieStore(
        temp_dir_.path().Append(chrome::kCookieFilename));
    std::vector<net::CookieMonster::CanonicalCookie*> cookies;
    Load(&cookies);
    ASSERT_EQ(0u, cookies.size());
    // Make sure the store gets written at least once.
    store_->AddCookie(
        net::CookieMonster::CanonicalCookie(GURL(), "A", "B", "http://foo.bar",
                                            "/", std::string(), std::string(),
                                            base::Time::Now(),
                                            base::Time::Now(),
                                            base::Time::Now(),
                                            false, false, true));
  }

 protected:
  BrowserThread ui_thread_;
  BrowserThread db_thread_;
  BrowserThread io_thread_;
  base::WaitableEvent loaded_event_;
  base::WaitableEvent key_loaded_event_;
  base::WaitableEvent db_thread_event_;
  std::vector<net::CookieMonster::CanonicalCookie*> cookies_;
  ScopedTempDir temp_dir_;
  scoped_refptr<SQLitePersistentCookieStore> store_;
};

TEST_F(SQLitePersistentCookieStoreTest, KeepOnDestruction) {
  store_->SetClearLocalStateOnExit(false);
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  scoped_refptr<base::ThreadTestHelper> helper(
      new base::ThreadTestHelper(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));
  ASSERT_TRUE(helper->Run());

  ASSERT_TRUE(file_util::PathExists(
      temp_dir_.path().Append(chrome::kCookieFilename)));
  ASSERT_TRUE(file_util::Delete(
      temp_dir_.path().Append(chrome::kCookieFilename), false));
}

TEST_F(SQLitePersistentCookieStoreTest, RemoveOnDestruction) {
  store_->SetClearLocalStateOnExit(true);
  // Replace the store effectively destroying the current one and forcing it
  // to write it's data to disk. Then we can see if after loading it again it
  // is still there.
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  scoped_refptr<base::ThreadTestHelper> helper(
      new base::ThreadTestHelper(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));
  ASSERT_TRUE(helper->Run());

  ASSERT_FALSE(file_util::PathExists(
      temp_dir_.path().Append(chrome::kCookieFilename)));
}

// Test if data is stored as expected in the SQLite database.
TEST_F(SQLitePersistentCookieStoreTest, TestPersistance) {
  std::vector<net::CookieMonster::CanonicalCookie*> cookies;
  // Replace the store effectively destroying the current one and forcing it
  // to write it's data to disk. Then we can see if after loading it again it
  // is still there.
  store_ = NULL;
  scoped_refptr<base::ThreadTestHelper> helper(
      new base::ThreadTestHelper(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));
  // Make sure we wait until the destructor has run.
  ASSERT_TRUE(helper->Run());
  store_ = new SQLitePersistentCookieStore(
      temp_dir_.path().Append(chrome::kCookieFilename));

  // Reload and test for persistence
  Load(&cookies);
  ASSERT_EQ(1U, cookies.size());
  ASSERT_STREQ("http://foo.bar", cookies[0]->Domain().c_str());
  ASSERT_STREQ("A", cookies[0]->Name().c_str());
  ASSERT_STREQ("B", cookies[0]->Value().c_str());

  // Now delete the cookie and check persistence again.
  store_->DeleteCookie(*cookies[0]);
  store_ = NULL;
  // Make sure we wait until the destructor has run.
  ASSERT_TRUE(helper->Run());
  STLDeleteContainerPointers(cookies.begin(), cookies.end());
  cookies.clear();
  store_ = new SQLitePersistentCookieStore(
      temp_dir_.path().Append(chrome::kCookieFilename));

  // Reload and check if the cookie has been removed.
  Load(&cookies);
  ASSERT_EQ(0U, cookies.size());
}

// Test that priority load of cookies for a specfic domain key could be
// completed before the entire store is loaded
TEST_F(SQLitePersistentCookieStoreTest, TestLoadCookiesForKey) {
  base::Time t = base::Time::Now() + base::TimeDelta::FromInternalValue(10);
  // A foo.bar cookie was already added in setup.
  store_->AddCookie(
    net::CookieMonster::CanonicalCookie(GURL(), "A", "B",
                                        "www.aaa.com", "/",
                                        std::string(), std::string(),
                                        t, t, t, false, false, true));
  t += base::TimeDelta::FromInternalValue(10);
  store_->AddCookie(
    net::CookieMonster::CanonicalCookie(GURL(), "A", "B",
                                        "travel.aaa.com", "/",
                                        std::string(), std::string(),
                                        t, t, t, false, false, true));
  t += base::TimeDelta::FromInternalValue(10);
  store_->AddCookie(
    net::CookieMonster::CanonicalCookie(GURL(), "A", "B",
                                        "www.bbb.com", "/",
                                        std::string(), std::string(),
                                        t, t, t, false, false, true));
  store_ = NULL;

  scoped_refptr<base::ThreadTestHelper> helper(
      new base::ThreadTestHelper(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));
  // Make sure we wait until the destructor has run.
  ASSERT_TRUE(helper->Run());

  store_ = new SQLitePersistentCookieStore(
    temp_dir_.path().Append(chrome::kCookieFilename));
  // Posting a blocking task to db_thread_ makes sure that the DB thread waits
  // until both Load and LoadCookiesForKey have been posted to its task queue.
  db_thread_.PostTask(BrowserThread::DB, FROM_HERE,
    base::Bind(&SQLitePersistentCookieStoreTest::WaitOnDBEvent,
               base::Unretained(this)));
  store_->Load(base::Bind(&SQLitePersistentCookieStoreTest::OnLoaded,
                          base::Unretained(this)));
  store_->LoadCookiesForKey("aaa.com",
    base::Bind(&SQLitePersistentCookieStoreTest::OnKeyLoaded,
               base::Unretained(this)));
  db_thread_.PostTask(BrowserThread::DB, FROM_HERE,
    base::Bind(&SQLitePersistentCookieStoreTest::WaitOnDBEvent,
               base::Unretained(this)));

  // Now the DB-thread queue contains:
  // (active:)
  // 1. Wait (on db_event)
  // (pending:)
  // 2. "Init And Chain-Load First Domain"
  // 3. Priority Load (aaa.com)
  // 4. Wait (on db_event)
  db_thread_event_.Signal();
  key_loaded_event_.Wait();
  ASSERT_EQ(loaded_event_.IsSignaled(), false);
  std::set<std::string> cookies_loaded;
  for (std::vector<net::CookieMonster::CanonicalCookie*>::iterator
       it = cookies_.begin(); it != cookies_.end(); ++it)
    cookies_loaded.insert((*it)->Domain().c_str());
  ASSERT_GT(4U, cookies_loaded.size());
  ASSERT_EQ(cookies_loaded.find("www.aaa.com") != cookies_loaded.end(), true);
  ASSERT_EQ(cookies_loaded.find("travel.aaa.com") != cookies_loaded.end(),
            true);

  db_thread_event_.Signal();
  loaded_event_.Wait();
  for (std::vector<net::CookieMonster::CanonicalCookie*>::iterator
       it = cookies_.begin(); it != cookies_.end(); ++it)
    cookies_loaded.insert((*it)->Domain().c_str());
  ASSERT_EQ(4U, cookies_loaded.size());
  ASSERT_EQ(cookies_loaded.find("http://foo.bar") != cookies_loaded.end(),
            true);
  ASSERT_EQ(cookies_loaded.find("www.bbb.com") != cookies_loaded.end(), true);
}

// Test that we can force the database to be written by calling Flush().
TEST_F(SQLitePersistentCookieStoreTest, TestFlush) {
  // File timestamps don't work well on all platforms, so we'll determine
  // whether the DB file has been modified by checking its size.
  FilePath path = temp_dir_.path().Append(chrome::kCookieFilename);
  base::PlatformFileInfo info;
  ASSERT_TRUE(file_util::GetFileInfo(path, &info));
  int64 base_size = info.size;

  // Write some large cookies, so the DB will have to expand by several KB.
  for (char c = 'a'; c < 'z'; ++c) {
    // Each cookie needs a unique timestamp for creation_utc (see DB schema).
    base::Time t = base::Time::Now() + base::TimeDelta::FromMicroseconds(c);
    std::string name(1, c);
    std::string value(1000, c);
    store_->AddCookie(
        net::CookieMonster::CanonicalCookie(GURL(), name, value,
                                            "http://foo.bar", "/",
                                            std::string(), std::string(),
                                            t, t, t,
                                            false, false, true));
  }

  // Call Flush() and wait until the DB thread is idle.
  store_->Flush(NULL);
  scoped_refptr<base::ThreadTestHelper> helper(
      new base::ThreadTestHelper(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));
  ASSERT_TRUE(helper->Run());

  // We forced a write, so now the file will be bigger.
  ASSERT_TRUE(file_util::GetFileInfo(path, &info));
  ASSERT_GT(info.size, base_size);
}

// Counts the number of times Callback() has been run.
class CallbackCounter : public base::RefCountedThreadSafe<CallbackCounter> {
 public:
  CallbackCounter() : callback_count_(0) {}

  void Callback() {
    ++callback_count_;
  }

  int callback_count() {
    return callback_count_;
  }

 private:
  friend class base::RefCountedThreadSafe<CallbackCounter>;
  volatile int callback_count_;
};

// Test that we can get a completion callback after a Flush().
TEST_F(SQLitePersistentCookieStoreTest, TestFlushCompletionCallback) {
  scoped_refptr<CallbackCounter> counter(new CallbackCounter());

  // Callback shouldn't be invoked until we call Flush().
  ASSERT_EQ(0, counter->callback_count());

  store_->Flush(NewRunnableMethod(counter.get(), &CallbackCounter::Callback));

  scoped_refptr<base::ThreadTestHelper> helper(
      new base::ThreadTestHelper(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));
  ASSERT_TRUE(helper->Run());

  ASSERT_EQ(1, counter->callback_count());
}
