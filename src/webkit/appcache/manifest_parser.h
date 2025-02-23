// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is a port of ManifestParser.h from WebKit/WebCore/loader/appcache.

/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WEBKIT_APPCACHE_MANIFEST_PARSER_H_
#define WEBKIT_APPCACHE_MANIFEST_PARSER_H_

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "webkit/appcache/appcache_export.h"

class GURL;

namespace appcache {

typedef std::pair<GURL, GURL> FallbackNamespace;

struct APPCACHE_EXPORT Manifest {
  Manifest();
  ~Manifest();

  base::hash_set<std::string> explicit_urls;
  std::vector<FallbackNamespace> fallback_namespaces;
  std::vector<GURL> online_whitelist_namespaces;
  bool online_whitelist_all;
};

APPCACHE_EXPORT bool ParseManifest(const GURL& manifest_url,
                                   const char* data,
                                   int length,
                                   Manifest& manifest);

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_MANIFEST_PARSER_H_
