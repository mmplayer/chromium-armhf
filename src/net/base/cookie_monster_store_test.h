// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains test infrastructure for multiple files
// (current cookie_monster_unittest.cc and cookie_monster_perftest.cc)
// that need to test out CookieMonster interactions with the backing store.
// It should only be included by test code.

#ifndef NET_BASE_COOKIE_MONSTER_STORE_TEST_H_
#define NET_BASE_COOKIE_MONSTER_STORE_TEST_H_
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>
#include "net/base/cookie_monster.h"

namespace base {
class Time;
}

namespace net {

// Wrapper class for posting a loaded callback. Since the Callback class is not
// reference counted, we cannot post a callback to the message loop directly,
// instead we post a LoadedCallbackTask.
class LoadedCallbackTask
    : public base::RefCountedThreadSafe<LoadedCallbackTask> {
 public:
  typedef CookieMonster::PersistentCookieStore::LoadedCallback LoadedCallback;

  LoadedCallbackTask(LoadedCallback loaded_callback,
                     std::vector<CookieMonster::CanonicalCookie*> cookies);
  ~LoadedCallbackTask();

  void Run() {
    loaded_callback_.Run(cookies_);
  }

 private:
  LoadedCallback loaded_callback_;
  std::vector<CookieMonster::CanonicalCookie*> cookies_;

  DISALLOW_COPY_AND_ASSIGN(LoadedCallbackTask);
};  // Wrapper class LoadedCallbackTask

// Describes a call to one of the 3 functions of PersistentCookieStore.
struct CookieStoreCommand {
  enum Type {
    ADD,
    UPDATE_ACCESS_TIME,
    REMOVE,
  };

  CookieStoreCommand(Type type,
                     const CookieMonster::CanonicalCookie& cookie)
      : type(type),
        cookie(cookie) {}

  Type type;
  CookieMonster::CanonicalCookie cookie;
};

// Implementation of PersistentCookieStore that captures the
// received commands and saves them to a list.
// The result of calls to Load() can be configured using SetLoadExpectation().
class MockPersistentCookieStore
    : public CookieMonster::PersistentCookieStore {
 public:
  typedef std::vector<CookieStoreCommand> CommandList;

  MockPersistentCookieStore();
  virtual ~MockPersistentCookieStore();

  void SetLoadExpectation(
      bool return_value,
      const std::vector<CookieMonster::CanonicalCookie*>& result);

  const CommandList& commands() const {
    return commands_;
  }

  virtual void Load(const LoadedCallback& loaded_callback) OVERRIDE;

  virtual void LoadCookiesForKey(const std::string& key,
    const LoadedCallback& loaded_callback) OVERRIDE;

  virtual void AddCookie(const CookieMonster::CanonicalCookie& cookie) OVERRIDE;

  virtual void UpdateCookieAccessTime(
      const CookieMonster::CanonicalCookie& cookie) OVERRIDE;

  virtual void DeleteCookie(
      const CookieMonster::CanonicalCookie& cookie) OVERRIDE;

  virtual void Flush(Task* completion_task) OVERRIDE;

  // No files are created so nothing to clear either
  virtual void SetClearLocalStateOnExit(bool clear_local_state) OVERRIDE;

 private:
  CommandList commands_;

  // Deferred result to use when Load() is called.
  bool load_return_value_;
  std::vector<CookieMonster::CanonicalCookie*> load_result_;
  // Indicates if the store has been fully loaded to avoid returning duplicate
  // cookies.
  bool loaded_;

  DISALLOW_COPY_AND_ASSIGN(MockPersistentCookieStore);
};

// Mock for CookieMonster::Delegate
class MockCookieMonsterDelegate : public CookieMonster::Delegate {
 public:
  typedef std::pair<CookieMonster::CanonicalCookie, bool>
      CookieNotification;

  MockCookieMonsterDelegate();

  const std::vector<CookieNotification>& changes() const { return changes_; }

  void reset() { changes_.clear(); }

  virtual void OnCookieChanged(
      const CookieMonster::CanonicalCookie& cookie,
      bool removed,
      CookieMonster::Delegate::ChangeCause cause);

 private:
  virtual ~MockCookieMonsterDelegate();

  std::vector<CookieNotification> changes_;

  DISALLOW_COPY_AND_ASSIGN(MockCookieMonsterDelegate);
};

// Helper to build a single CanonicalCookie.
CookieMonster::CanonicalCookie BuildCanonicalCookie(
    const std::string& key,
    const std::string& cookie_line,
    const base::Time& creation_time);

// Helper to build a list of CanonicalCookie*s.
void AddCookieToList(
    const std::string& key,
    const std::string& cookie_line,
    const base::Time& creation_time,
    std::vector<CookieMonster::CanonicalCookie*>* out_list);

// Just act like a backing database.  Keep cookie information from
// Add/Update/Delete and regurgitate it when Load is called.
class MockSimplePersistentCookieStore
    : public CookieMonster::PersistentCookieStore {
 public:
  MockSimplePersistentCookieStore();
  virtual ~MockSimplePersistentCookieStore();

  virtual void Load(const LoadedCallback& loaded_callback) OVERRIDE;

  virtual void LoadCookiesForKey(const std::string& key,
      const LoadedCallback& loaded_callback) OVERRIDE;

  virtual void AddCookie(
      const CookieMonster::CanonicalCookie& cookie) OVERRIDE;

  virtual void UpdateCookieAccessTime(
      const CookieMonster::CanonicalCookie& cookie) OVERRIDE;

  virtual void DeleteCookie(
      const CookieMonster::CanonicalCookie& cookie) OVERRIDE;

  virtual void Flush(Task* completion_task) OVERRIDE;

  virtual void SetClearLocalStateOnExit(bool clear_local_state) OVERRIDE;

 private:
  typedef std::map<int64, CookieMonster::CanonicalCookie>
      CanonicalCookieMap;

  CanonicalCookieMap cookies_;

  // Indicates if the store has been fully loaded to avoid return duplicate
  // cookies in subsequent load requests
  bool loaded_;
};

// Helper function for creating a CookieMonster backed by a
// MockSimplePersistentCookieStore for garbage collection testing.
//
// Fill the store through import with |num_cookies| cookies, |num_old_cookies|
// with access time Now()-days_old, the rest with access time Now().
// Do two SetCookies().  Return whether each of the two SetCookies() took
// longer than |gc_perf_micros| to complete, and how many cookie were
// left in the store afterwards.
CookieMonster* CreateMonsterFromStoreForGC(
    int num_cookies,
    int num_old_cookies,
    int days_old);

}  // namespace net

#endif  // NET_BASE_COOKIE_MONSTER_STORE_TEST_H_
