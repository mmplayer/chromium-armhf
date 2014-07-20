// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_desktop_delegate.h"

#include "ui/aura/desktop.h"

namespace aura {
namespace test {

TestDesktopDelegate::TestDesktopDelegate()
    : default_container_(new ToplevelWindowContainer) {
  Desktop::GetInstance()->SetDelegate(this);
  default_container_->Init();
  default_container_->SetBounds(
      gfx::Rect(gfx::Point(), Desktop::GetInstance()->GetSize()));
  Desktop::GetInstance()->window()->AddChild(default_container_.get());
  default_container_->Show();
}

TestDesktopDelegate::~TestDesktopDelegate() {
}

void TestDesktopDelegate::AddChildToDefaultParent(Window* window) {
  default_container_->AddChild(window);
}

Window* TestDesktopDelegate::GetTopmostWindowToActivate(Window* ignore) const {
  return default_container_->GetTopmostWindowToActivate(ignore);
}

}  // namespace test
}  // namespace aura
