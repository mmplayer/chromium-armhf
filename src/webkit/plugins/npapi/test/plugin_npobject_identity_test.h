// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef WEBKIT_PLUGINS_NPAPI_TEST_PLUGIN_NPOBJECT_IDENTITY_TEST_H_
#define WEBKIT_PLUGINS_NPAPI_TEST_PLUGIN_NPOBJECT_IDENTITY_TEST_H_

#include "webkit/plugins/npapi/test/plugin_test.h"

namespace NPAPIClient {

// The NPObjectProxyTest tests that when we proxy an NPObject that is itself
// a proxy, we don't create a new proxy but instead just use the original
// pointer.

class NPObjectIdentityTest : public PluginTest {
 public:
  // Constructor.
  NPObjectIdentityTest(NPP id, NPNetscapeFuncs *host_functions);

  // NPAPI SetWindow handler.
  virtual NPError SetWindow(NPWindow* pNPWindow);
};

}  // namespace NPAPIClient

#endif  // WEBKIT_PLUGINS_NPAPI_TEST_PLUGIN_NPOBJECT_IDENTITY_TEST_H_
