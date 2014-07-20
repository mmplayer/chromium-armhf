// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Constants definitions

#include "chrome/common/net/gaia/gaia_constants.h"

namespace GaiaConstants {

// Gaia uses this for accounting where login is coming from.
const char kChromeOSSource[] = "chromeos";
const char kChromeSource[] = "ChromiumBrowser";

// Service name for Gaia.  Used to convert to cookie auth.
const char kGaiaService[] = "gaia";
// Service name for Picasa API. API is used to get user's image.
const char kPicasaService[] = "lh2";

// Service/scope names for sync.
const char kSyncService[] = "chromiumsync";
const char kSyncServiceOAuth[] = "https://www.googleapis.com/auth/chromesync";

// Service name for XMPP Google Talk.
const char kTalkService[] = "talk";
// Service name for remoting.
const char kRemotingService[] = "chromoting";
// Service name for cloud print.
const char kCloudPrintService[] = "cloudprint";

// Service/scope names for device management (cloud-based policy) server.
const char kDeviceManagementService[] = "mobilesync";
const char kDeviceManagementServiceOAuth[] =
    "https://www.googleapis.com/auth/chromeosdevicemanagement";

// Used to mint uber auth tokens when needed.
const char kGaiaSid[] = "sid";
const char kGaiaLsid[] = "lsid";
extern const char kGaiaOAuthToken[] = "oauthToken";
extern const char kGaiaOAuthSecret[] = "oauthSecret";
extern const char kGaiaOAuthDuration[] = "3600";

}  // namespace GaiaConstants
