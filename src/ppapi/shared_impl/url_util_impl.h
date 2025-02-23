// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_URL_UTIL_IMPL_H_
#define PPAPI_SHARED_IMPL_URL_UTIL_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "googleurl/src/url_parse.h"
#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

class GURL;

namespace ppapi {

// Contains the implementation of PPB_URLUtil that is shared between the proxy
// and the renderer.
class PPAPI_SHARED_EXPORT URLUtilImpl {
 public:
  // PPB_URLUtil shared functions.
  static PP_Var Canonicalize(PP_Var url,
                             PP_URLComponents_Dev* components);
  static PP_Var ResolveRelativeToURL(PP_Var base_url,
                                     PP_Var relative,
                                     PP_URLComponents_Dev* components);
  static PP_Bool IsSameSecurityOrigin(PP_Var url_a, PP_Var url_b);

  // Used for returning the given GURL from a PPAPI function, with an optional
  // out param indicating the components.
  static PP_Var GenerateURLReturn(PP_Module pp_module,
                                  const GURL& url,
                                  PP_URLComponents_Dev* components);

  // Helper function that optionally take a components structure and fills it
  // out with the parsed version of the given URL. If the components pointer is
  // NULL, this function will do nothing.
  //
  // It's annoying to serialze the large PP_URLComponents structure across IPC
  // and the data isn't often requested by plugins. This function is used on
  // the plugin side to fill in the components for those cases where it's
  // actually needed.
  static PP_Var ConvertComponentsAndReturnURL(const PP_Var& url,
                                              PP_URLComponents_Dev* components);
};

}  // namespace ppapi

#endif
