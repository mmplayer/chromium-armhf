// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_BROWSER_THREAD_MODEL_WORKER_H_
#define CHROME_BROWSER_SYNC_GLUE_BROWSER_THREAD_MODEL_WORKER_H_
#pragma once

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "content/browser/browser_thread.h"

namespace base {
class WaitableEvent;
}

namespace browser_sync {

// A ModelSafeWorker for models that accept requests from the syncapi that need
// to be fulfilled on a browser thread, for example autofill on the DB thread.
// TODO(sync): Try to generalize other ModelWorkers (e.g. history, etc).
class BrowserThreadModelWorker : public browser_sync::ModelSafeWorker {
 public:
  BrowserThreadModelWorker(BrowserThread::ID thread, ModelSafeGroup group);
  virtual ~BrowserThreadModelWorker();

  // ModelSafeWorker implementation. Called on the sync thread.
  virtual void DoWorkAndWaitUntilDone(Callback0::Type* work);
  virtual ModelSafeGroup GetModelSafeGroup();

 protected:
  // Marked pure virtual so subclasses have to override, but there is
  // an implementation that subclasses should use.  This is so that
  // (subclass)::CallDoWorkAndSignalTask shows up in callstacks.
  virtual void CallDoWorkAndSignalTask(
      Callback0::Type* work, base::WaitableEvent* done) = 0;

 private:
  BrowserThread::ID thread_;
  ModelSafeGroup group_;

  DISALLOW_COPY_AND_ASSIGN(BrowserThreadModelWorker);
};

// Subclass BrowserThreadModelWorker so that we can distinguish them
// from stack traces alone.

class DatabaseModelWorker : public BrowserThreadModelWorker {
 public:
  DatabaseModelWorker();
  virtual ~DatabaseModelWorker();

 protected:
  virtual void CallDoWorkAndSignalTask(
      Callback0::Type* work, base::WaitableEvent* done) OVERRIDE;
};

class FileModelWorker : public BrowserThreadModelWorker {
 public:
  FileModelWorker();
  virtual ~FileModelWorker();

 protected:
  virtual void CallDoWorkAndSignalTask(
      Callback0::Type* work, base::WaitableEvent* done) OVERRIDE;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_BROWSER_THREAD_MODEL_WORKER_H_
