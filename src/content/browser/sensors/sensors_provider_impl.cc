// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sensors/sensors_provider_impl.h"

#include "base/memory/singleton.h"

namespace sensors {

ProviderImpl* ProviderImpl::GetInstance() {
  return Singleton<ProviderImpl>::get();
}

ProviderImpl::ProviderImpl()
    : listeners_(new ListenerList()) {
}

ProviderImpl::~ProviderImpl() {
}

void ProviderImpl::AddListener(Listener* listener) {
  listeners_->AddObserver(listener);
}

void ProviderImpl::RemoveListener(Listener* listener) {
  listeners_->RemoveObserver(listener);
}

void ProviderImpl::ScreenOrientationChanged(
    const ScreenOrientation& change) {
  listeners_->Notify(&Listener::OnScreenOrientationChanged, change);
}

}  // namespace sensors
