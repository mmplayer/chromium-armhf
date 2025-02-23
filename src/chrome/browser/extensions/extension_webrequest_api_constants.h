// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the WebRequest API.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_API_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_API_CONSTANTS_H_
#pragma once

namespace extension_webrequest_api_constants {

// Keys.
extern const char kChallengerKey[];
extern const char kErrorKey[];
extern const char kFrameIdKey[];
extern const char kFromCache[];
extern const char kHostKey[];
extern const char kIpKey[];
extern const char kMethodKey[];
extern const char kPortKey[];
extern const char kRedirectUrlKey[];
extern const char kRequestIdKey[];
extern const char kStatusCodeKey[];
extern const char kStatusLineKey[];
extern const char kTabIdKey[];
extern const char kTimeStampKey[];
extern const char kTypeKey[];
extern const char kUrlKey[];
extern const char kRequestHeadersKey[];
extern const char kResponseHeadersKey[];
extern const char kHeadersKey[];
extern const char kHeaderNameKey[];
extern const char kHeaderValueKey[];
extern const char kIsProxyKey[];
extern const char kSchemeKey[];
extern const char kRealmKey[];
extern const char kAuthCredentialsKey[];
extern const char kUsernameKey[];
extern const char kPasswordKey[];

// Events.
extern const char kOnAuthRequired[];
extern const char kOnBeforeRedirect[];
extern const char kOnBeforeRequest[];
extern const char kOnBeforeSendHeaders[];
extern const char kOnCompleted[];
extern const char kOnErrorOccurred[];
extern const char kOnHeadersReceived[];
extern const char kOnResponseStarted[];
extern const char kOnSendHeaders[];

// Error messages.
extern const char kInvalidRedirectUrl[];
extern const char kInvalidBlockingResponse[];
extern const char kInvalidRequestFilterUrl[];

}  // namespace extension_webrequest_api_constants

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_API_CONSTANTS_H_
