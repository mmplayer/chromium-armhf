// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(USE_AURA)
#include "chrome/default_plugin/plugin_impl_aura.h"
#elif defined(OS_WIN)
#include "chrome/default_plugin/plugin_impl_win.h"
#elif defined(OS_MACOSX)
#include "chrome/default_plugin/plugin_impl_mac.h"
#elif defined(TOOLKIT_USES_GTK)
#include "chrome/default_plugin/plugin_impl_gtk.h"
#endif
