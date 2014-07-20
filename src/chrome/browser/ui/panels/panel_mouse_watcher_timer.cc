// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher.h"
#include "ui/gfx/screen.h"

// A timer based implementation of PanelMouseWatcher.  Currently used for Gtk
// and Mac panels implementations.
class PanelMouseWatcherTimer : public PanelMouseWatcher {
 public:
  explicit PanelMouseWatcherTimer(Observer* observer);
  virtual ~PanelMouseWatcherTimer();

 protected:
  virtual void Start();
  virtual void Stop();

 private:
  // Specifies the rate at which we want to sample the mouse position.
  static const int kMousePollingIntervalMs = 250;

  // Timer callback function.
  void DoWork();
  friend class base::RepeatingTimer<PanelMouseWatcherTimer>;

  // Timer used to track mouse movements. Some OSes do not provide an easy way
  // of tracking mouse movements across applications.  So we use a timer to
  // accomplish the same.  This could also be more efficient as you end up
  // getting a lot of notifications when tracking mouse movements.
  base::RepeatingTimer<PanelMouseWatcherTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(PanelMouseWatcherTimer);
};

// static
PanelMouseWatcher* PanelMouseWatcher::Create(Observer* observer) {
  return new PanelMouseWatcherTimer(observer);
}

PanelMouseWatcherTimer::PanelMouseWatcherTimer(Observer* observer)
    : PanelMouseWatcher(observer) {
}

PanelMouseWatcherTimer::~PanelMouseWatcherTimer() {
  DCHECK(!timer_.IsRunning());
}

void PanelMouseWatcherTimer::Start() {
  DCHECK(!timer_.IsRunning());
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kMousePollingIntervalMs),
               this, &PanelMouseWatcherTimer::DoWork);
}

void PanelMouseWatcherTimer::Stop() {
  DCHECK(timer_.IsRunning());
  timer_.Stop();
}

void PanelMouseWatcherTimer::DoWork() {
  NotifyMouseMovement(gfx::Screen::GetCursorScreenPoint());
}
