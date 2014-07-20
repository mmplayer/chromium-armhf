// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {
'use strict';

window.addEventListener('blur', pluginLostFocus_, false);

function pluginLostFocus_() {
  // If the plug loses input focus, release all keys as a precaution against
  // leaving them 'stuck down' on the host.
  if (remoting.session && remoting.session.plugin) {
    remoting.session.plugin.releaseAllKeys();
  }
}

/** @enum {string} */
remoting.AppMode = {
  HOME: 'home',
  UNAUTHENTICATED: 'auth',
  CLIENT: 'client',
    CLIENT_UNCONNECTED: 'client.unconnected',
    CLIENT_CONNECTING: 'client.connecting',
    CLIENT_CONNECT_FAILED: 'client.connect-failed',
    CLIENT_SESSION_FINISHED: 'client.session-finished',
  HOST: 'host',
    HOST_WAITING_FOR_CODE: 'host.waiting-for-code',
    HOST_WAITING_FOR_CONNECTION: 'host.waiting-for-connection',
    HOST_SHARED: 'host.shared',
    HOST_SHARE_FAILED: 'host.share-failed',
    HOST_SHARE_FINISHED: 'host.share-finished',
  IN_SESSION: 'in-session'
};

remoting.EMAIL = 'email';
remoting.HOST_PLUGIN_ID = 'host-plugin-id';

/** @enum {string} */
remoting.ClientError = {
  NO_RESPONSE: /*i18n-content*/'ERROR_NO_RESPONSE',
  INVALID_ACCESS_CODE: /*i18n-content*/'ERROR_INVALID_ACCESS_CODE',
  MISSING_PLUGIN: /*i18n-content*/'ERROR_MISSING_PLUGIN',
  OAUTH_FETCH_FAILED: /*i18n-content*/'ERROR_AUTHENTICATION_FAILED',
  HOST_IS_OFFLINE: /*i18n-content*/'ERROR_HOST_IS_OFFLINE',
  INCOMPATIBLE_PROTOCOL: /*i18n-content*/'ERROR_INCOMPATIBLE_PROTOCOL',
  BAD_PLUGIN_VERSION: /*i18n-content*/'ERROR_BAD_PLUGIN_VERSION',
  OTHER_ERROR: /*i18n-content*/'ERROR_GENERIC'
};

/**
 * Whether or not the plugin should scale itself.
 * @type {boolean}
 */
remoting.scaleToFit = false;

// Constants representing keys used for storing persistent application state.
var KEY_APP_MODE_ = 'remoting-app-mode';
var KEY_EMAIL_ = 'remoting-email';
var KEY_USE_P2P_API_ = 'remoting-use-p2p-api';

// Some constants for pretty-printing the access code.
var kSupportIdLen = 7;
var kHostSecretLen = 5;
var kAccessCodeLen = kSupportIdLen + kHostSecretLen;
var kDigitsPerGroup = 4;

function hasClass(classes, cls) {
  return classes.match(new RegExp('(\\s|^)' + cls + '(\\s|$)'));
}

function addClass(element, cls) {
  if (!hasClass(element.className, cls)) {
    var padded = element.className == '' ? '' : element.className + ' ';
    element.className = padded + cls;
  }
}

function removeClass(element, cls) {
  element.className =
      element.className.replace(new RegExp('\\b' + cls + '\\b', 'g'), '')
                       .replace('  ', ' ');
}

function retrieveEmail_(access_token) {
  var headers = {
    'Authorization': 'OAuth ' + remoting.oauth2.getAccessToken()
  };

  var onResponse = function(xhr) {
    if (xhr.status != 200) {
      // TODO(ajwong): Have a better way of showing an error.
      remoting.debug.log('Unable to get email');
      document.getElementById('current-email').innerText = '???';
      return;
    }

    // TODO(ajwong): See if we can't find a JSON endpoint.
    setEmail(xhr.responseText.split('&')[0].split('=')[1]);
  };

  // TODO(ajwong): Update to new v2 API.
  remoting.xhr.get('https://www.googleapis.com/userinfo/email',
                   onResponse, '', headers);
}

function refreshEmail_() {
  if (!getEmail() && remoting.oauth2.isAuthenticated()) {
    remoting.oauth2.callWithToken(retrieveEmail_);
  }
}

function setEmail(value) {
  window.localStorage.setItem(KEY_EMAIL_, value);
  document.getElementById('current-email').innerText = value;
}

/**
 * @return {?string} The email address associated with the auth credentials.
 */
function getEmail() {
  var result = window.localStorage.getItem(KEY_EMAIL_);
  return typeof result == 'string' ? result : null;
}

function exchangedCodeForToken_() {
  if (!remoting.oauth2.isAuthenticated()) {
    alert('Your OAuth2 token was invalid. Please try again.');
  }
  remoting.oauth2.callWithToken(function(token) {
      retrieveEmail_(token);
  });
}

remoting.clearOAuth2 = function() {
  remoting.oauth2.clear();
  window.localStorage.removeItem(KEY_EMAIL_);
  remoting.setMode(remoting.AppMode.UNAUTHENTICATED);
}

remoting.toggleDebugLog = function() {
  var debugLog = document.getElementById('debug-log');
  if (debugLog.hidden) {
    debugLog.hidden = false;
  } else {
    debugLog.hidden = true;
  }
}

remoting.init = function() {
  l10n.localize();
  var button = document.getElementById('toggle-scaling');
  button.title = chrome.i18n.getMessage(/*i18n-content*/'TOOLTIP_SCALING');
  // Create global objects.
  remoting.oauth2 = new remoting.OAuth2();
  remoting.debug =
      new remoting.DebugLog(document.getElementById('debug-messages'));
  /** @type {XMLHttpRequest} */
  remoting.supportHostsXhr = null;

  refreshEmail_();
  var email = getEmail();
  if (email) {
    document.getElementById('current-email').innerText = email;
  }

  remoting.setMode(getAppStartupMode());
  if (isHostModeSupported()) {
    var unsupported = document.getElementById('client-footer-text-cros');
    unsupported.parentNode.removeChild(unsupported);
  } else {
    var footer = document.getElementById('client-footer-text');
    footer.parentNode.removeChild(footer);
    document.getElementById('client-footer-text-cros').id =
        'client-footer-text';
  }
}

/**
 * Change the app's modal state to |mode|, which is considered to be a dotted
 * hierachy of modes. For example, setMode('host.shared') will show any modal
 * elements with an data-ui-mode attribute of 'host' or 'host.shared' and hide
 * all others.
 *
 * @param {remoting.AppMode} mode The new modal state, expressed as a dotted
 * hiearchy.
 */
remoting.setMode = function(mode) {
  var modes = mode.split('.');
  for (var i = 1; i < modes.length; ++i)
    modes[i] = modes[i - 1] + '.' + modes[i];
  var elements = document.querySelectorAll('[data-ui-mode]');
  for (var i = 0; i < elements.length; ++i) {
    var element = elements[i];
    var hidden = true;
    for (var m = 0; m < modes.length; ++m) {
      if (hasClass(element.getAttribute('data-ui-mode'), modes[m])) {
        hidden = false;
        break;
      }
    }
    element.hidden = hidden;
  }
  remoting.debug.log('App mode: ' + mode);
  remoting.currentMode = mode;
  if (mode == remoting.AppMode.IN_SESSION) {
    document.removeEventListener('keydown', remoting.checkHotkeys, false);
  } else {
    document.addEventListener('keydown', remoting.checkHotkeys, false);
  }
}

/**
 * Get the major mode that the app is running in.
 * @return {remoting.AppMode} The app's current major mode.
 */
remoting.getMajorMode = function() {
  return remoting.currentMode.split('.')[0];
}

remoting.tryShare = function() {
  remoting.debug.log('Attempting to share...');
  remoting.lastShareWasCancelled = false;
  if (remoting.oauth2.needsNewAccessToken()) {
    remoting.debug.log('Refreshing token...');
    remoting.oauth2.refreshAccessToken(function() {
      if (remoting.oauth2.needsNewAccessToken()) {
        // If we still need it, we're going to infinite loop.
        showShareError_(/*i18n-content*/'ERROR_UNABLE_TO_GET_TOKEN');
        throw 'Unable to get access token';
      }
      remoting.tryShare();
    });
    return;
  }

  remoting.setMode(remoting.AppMode.HOST_WAITING_FOR_CODE);
  document.getElementById('cancel-button').disabled = false;
  disableTimeoutCountdown_();

  var div = document.getElementById('host-plugin-container');
  var plugin = /** @type {remoting.HostPlugin} */
      document.createElement('embed');
  plugin.type = remoting.PLUGIN_MIMETYPE;
  plugin.id = remoting.HOST_PLUGIN_ID;
  // Hiding the plugin means it doesn't load, so make it size zero instead.
  plugin.width = 0;
  plugin.height = 0;
  div.appendChild(plugin);
  onNatTraversalPolicyChanged_(true);  // Hide warning by default.
  plugin.onNatTraversalPolicyChanged = onNatTraversalPolicyChanged_;
  plugin.onStateChanged = onStateChanged_;
  plugin.logDebugInfo = debugInfoCallback_;
  plugin.localize(chrome.i18n.getMessage);
  plugin.connect(/** @type {string} */ (getEmail()),
                 'oauth2:' + remoting.oauth2.getAccessToken());
}

function disableTimeoutCountdown_() {
  if (remoting.timerRunning) {
    clearInterval(remoting.accessCodeTimerId);
    remoting.timerRunning = false;
    updateTimeoutStyles_();
  }
}

var ACCESS_CODE_TIMER_DISPLAY_THRESHOLD = 30;
var ACCESS_CODE_RED_THRESHOLD = 10;

/**
 * Show/hide or restyle various elements, depending on the remaining countdown
 * and timer state.
 *
 * @return {boolean} True if the timeout is in progress, false if it has
 * expired.
 */
function updateTimeoutStyles_() {
  if (remoting.timerRunning) {
    if (remoting.accessCodeExpiresIn <= 0) {
      remoting.cancelShare();
      return false;
    }
    if (remoting.accessCodeExpiresIn <= ACCESS_CODE_RED_THRESHOLD) {
      addClass(document.getElementById('access-code-display'), 'expiring');
    } else {
      removeClass(document.getElementById('access-code-display'), 'expiring');
    }
  }
  document.getElementById('access-code-countdown').hidden =
      (remoting.accessCodeExpiresIn > ACCESS_CODE_TIMER_DISPLAY_THRESHOLD) ||
      !remoting.timerRunning;
  return true;
}

remoting.decrementAccessCodeTimeout_ = function() {
  --remoting.accessCodeExpiresIn;
  remoting.updateAccessCodeTimeoutElement_();
}

remoting.updateAccessCodeTimeoutElement_ = function() {
  var pad = (remoting.accessCodeExpiresIn < 10) ? '0:0' : '0:';
  l10n.localizeElement(document.getElementById('seconds-remaining'),
                       pad + remoting.accessCodeExpiresIn);
  if (!updateTimeoutStyles_()) {
    disableTimeoutCountdown_();
  }
}

function onNatTraversalPolicyChanged_(enabled) {
  var container = document.getElementById('nat-box-container');
  container.hidden = enabled;
}

function onStateChanged_() {
  var plugin = /** @type {remoting.HostPlugin} */
      document.getElementById(remoting.HOST_PLUGIN_ID);
  var state = plugin.state;
  if (state == plugin.STARTING) {
    // Nothing to do here.
    remoting.debug.log('Host plugin state: STARTING');
  } else if (state == plugin.REQUESTED_ACCESS_CODE) {
    // Nothing to do here.
    remoting.debug.log('Host plugin state: REQUESTED_ACCESS_CODE');
  } else if (state == plugin.RECEIVED_ACCESS_CODE) {
    remoting.debug.log('Host plugin state: RECEIVED_ACCESS_CODE');
    var accessCode = plugin.accessCode;
    var accessCodeDisplay = document.getElementById('access-code-display');
    accessCodeDisplay.innerText = '';
    // Display the access code in groups of four digits for readability.
    for (var i = 0; i < accessCode.length; i += kDigitsPerGroup) {
      var nextFourDigits = document.createElement('span');
      nextFourDigits.className = 'access-code-digit-group';
      nextFourDigits.innerText = accessCode.substring(i, i + kDigitsPerGroup);
      accessCodeDisplay.appendChild(nextFourDigits);
    }
    remoting.accessCodeExpiresIn = plugin.accessCodeLifetime;
    if (remoting.accessCodeExpiresIn > 0) {  // Check it hasn't expired.
      remoting.accessCodeTimerId = setInterval(
          'remoting.decrementAccessCodeTimeout_()', 1000);
      remoting.timerRunning = true;
      remoting.updateAccessCodeTimeoutElement_();
      updateTimeoutStyles_();
      remoting.setMode(remoting.AppMode.HOST_WAITING_FOR_CONNECTION);
    } else {
      // This can only happen if the cloud tells us that the code lifetime is
      // <= 0s, which shouldn't happen so we don't care how clean this UX is.
      remoting.debug.log('Access code already invalid on receipt!');
      remoting.cancelShare();
    }
  } else if (state == plugin.CONNECTED) {
    remoting.debug.log('Host plugin state: CONNECTED');
    var element = document.getElementById('host-shared-message');
    var client = plugin.client;
    l10n.localizeElement(element, client);
    remoting.setMode(remoting.AppMode.HOST_SHARED);
    disableTimeoutCountdown_();
  } else if (state == plugin.DISCONNECTING) {
    remoting.debug.log('Host plugin state: DISCONNECTING');
  } else if (state == plugin.DISCONNECTED) {
    remoting.debug.log('Host plugin state: DISCONNECTED');
    if (remoting.currentMode != remoting.AppMode.HOST_SHARE_FAILED) {
      // If an error is being displayed, then the plugin should not be able to
      // hide it by setting the state. Errors must be dismissed by the user
      // clicking OK, which puts the app into mode HOME.
      if (remoting.lastShareWasCancelled) {
        remoting.setMode(remoting.AppMode.HOME);
      } else {
        remoting.setMode(remoting.AppMode.HOST_SHARE_FINISHED);
      }
    }
    plugin.parentNode.removeChild(plugin);
  } else if (state == plugin.ERROR) {
    remoting.debug.log('Host plugin state: ERROR');
    showShareError_(/*i18n-content*/'ERROR_GENERIC');
  } else {
    remoting.debug.log('Unknown state -> ' + state);
  }
}

/**
 * This is the callback that the host plugin invokes to indicate that there
 * is additional debug log info to display.
 * @param {string} msg The message (which will not be localized) to be logged.
 */
function debugInfoCallback_(msg) {
  remoting.debug.log('plugin: ' + msg);
}

/**
 * Show a host-side error message.
 *
 * @param {string} errorTag The error message to be localized and displayed.
 * @return {void} Nothing.
 */
function showShareError_(errorTag) {
  var errorDiv = document.getElementById('host-plugin-error');
  l10n.localizeElementFromTag(errorDiv, errorTag);
  remoting.debug.log('Sharing error: ' + errorTag);
  remoting.setMode(remoting.AppMode.HOST_SHARE_FAILED);
}

/**
 * Cancel an active or pending share operation.
 *
 * @return {void} Nothing.
 */
remoting.cancelShare = function() {
  remoting.debug.log('Canceling share...');
  remoting.lastShareWasCancelled = true;
  var plugin = /** @type {remoting.HostPlugin} */
      document.getElementById(remoting.HOST_PLUGIN_ID);
  try {
    plugin.disconnect();
  } catch (error) {
    remoting.debug.log('Error disconnecting: ' + error.description +
                       '. The host plugin probably crashed.');
    // TODO(jamiewalch): Clean this up. We should have a class representing
    // the host plugin, like we do for the client, which should handle crash
    // reporting and it should use a more detailed error message than the
    // default 'generic' one. See crbug.com/94624
    showShareError_(/*i18n-content*/'ERROR_GENERIC');
  }
  disableTimeoutCountdown_();
}

/**
 * Cancel an incomplete connect operation.
 *
 * @return {void} Nothing.
 */
remoting.cancelConnect = function() {
  if (remoting.supportHostsXhr) {
    remoting.supportHostsXhr.abort();
    remoting.supportHostsXhr = null;
  }
  if (remoting.session) {
    remoting.session.removePlugin();
    remoting.session = null;
  }
  remoting.setMode(remoting.AppMode.HOME);
}

function updateStatistics() {
  if (!remoting.session)
    return;
  if (remoting.session.state != remoting.ClientSession.State.CONNECTED)
    return;
  var stats = remoting.session.stats();

  var units = '';
  var videoBandwidth = stats['video_bandwidth'];
  if (videoBandwidth < 1024) {
    units = 'Bps';
  } else if (videoBandwidth < 1048576) {
    units = 'KiBps';
    videoBandwidth = videoBandwidth / 1024;
  } else if (videoBandwidth < 1073741824) {
    units = 'MiBps';
    videoBandwidth = videoBandwidth / 1048576;
  } else {
    units = 'GiBps';
    videoBandwidth = videoBandwidth / 1073741824;
  }

  var statistics = document.getElementById('statistics');
  statistics.innerText =
      'Bandwidth: ' + videoBandwidth.toFixed(2) + units +
      ', Capture: ' + stats['capture_latency'].toFixed(2) + 'ms' +
      ', Encode: ' + stats['encode_latency'].toFixed(2) + 'ms' +
      ', Decode: ' + stats['decode_latency'].toFixed(2) + 'ms' +
      ', Render: ' + stats['render_latency'].toFixed(2) + 'ms' +
      ', Latency: ' + stats['roundtrip_latency'].toFixed(2) + 'ms';

  // Update the stats once per second.
  window.setTimeout(updateStatistics, 1000);
}

function showToolbarPreview_() {
  var toolbar = document.getElementById('session-toolbar');
  addClass(toolbar, 'toolbar-preview');
  window.setTimeout(removeClass, 3000, toolbar, 'toolbar-preview');
}

function onClientStateChange_(oldState) {
  if (!remoting.session) {
    // If the connection has been cancelled, then we no longer have a reference
    // to the session object and should ignore any state changes.
    return;
  }
  var state = remoting.session.state;
  if (state == remoting.ClientSession.State.CREATED) {
    remoting.debug.log('Created plugin');
  } else if (state == remoting.ClientSession.State.BAD_PLUGIN_VERSION) {
    showConnectError_(remoting.ClientError.BAD_PLUGIN_VERSION);
  } else if (state == remoting.ClientSession.State.CONNECTING) {
    remoting.debug.log('Connecting as ' + remoting.username);
  } else if (state == remoting.ClientSession.State.INITIALIZING) {
    remoting.debug.log('Initializing connection');
  } else if (state == remoting.ClientSession.State.CONNECTED) {
    if (remoting.session) {
      remoting.setMode(remoting.AppMode.IN_SESSION);
      recenterToolbar_();
      showToolbarPreview_();
      updateStatistics();
    }
  } else if (state == remoting.ClientSession.State.CLOSED) {
    if (oldState == remoting.ClientSession.State.CONNECTED) {
      remoting.session.removePlugin();
      remoting.session = null;
      remoting.debug.log('Connection closed by host');
      remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED);
    } else {
      // The transition from CONNECTING to CLOSED state may happen
      // only with older client plugins. Current version should go the
      // FAILED state when connection fails.
      showConnectError_(remoting.ClientError.INVALID_ACCESS_CODE);
    }
  } else if (state == remoting.ClientSession.State.CONNECTION_FAILED) {
    remoting.debug.log('Client plugin reported connection failed');
    if (remoting.session.error ==
        remoting.ClientSession.ConnectionError.HOST_IS_OFFLINE) {
      showConnectError_(remoting.ClientError.HOST_IS_OFFLINE);
    } else if (remoting.session.error ==
               remoting.ClientSession.ConnectionError.SESSION_REJECTED) {
      showConnectError_(remoting.ClientError.INVALID_ACCESS_CODE);
    } else if (remoting.session.error ==
               remoting.ClientSession.ConnectionError.INCOMPATIBLE_PROTOCOL) {
      showConnectError_(remoting.ClientError.INCOMPATIBLE_PROTOCOL);
    } else if (remoting.session.error ==
               remoting.ClientSession.ConnectionError.NETWORK_FAILURE) {
      showConnectError_(remoting.ClientError.OTHER_ERROR);
    } else {
      showConnectError_(remoting.ClientError.OTHER_ERROR);
    }
  } else {
    remoting.debug.log('Unexpected client plugin state: ' + state);
    // This should only happen if the web-app and client plugin get out of
    // sync, and even then the version check should allow compatibility.
    showConnectError_(remoting.ClientError.MISSING_PLUGIN);
  }
}

function startSession_() {
  remoting.debug.log('Starting session...');
  var accessCode = document.getElementById('access-code-entry');
  accessCode.value = '';  // The code has been validated and won't work again.
  remoting.username =
      /** @type {string} email must be non-NULL to get here */ getEmail();
  remoting.session =
      new remoting.ClientSession(remoting.hostJid, remoting.hostPublicKey,
                                 remoting.accessCode, remoting.username,
                                 onClientStateChange_);
  remoting.oauth2.callWithToken(function(token) {
    remoting.session.createPluginAndConnect(
        document.getElementById('session-mode'),
        token);
  });
}

/**
 * Show a client-side error message.
 *
 * @param {remoting.ClientError} errorTag The error to be localized and
 * displayed.
 * @return {void} Nothing.
 */
function showConnectError_(errorTag) {
  remoting.debug.log('Connection failed: ' + errorTag);
  var errorDiv = document.getElementById('connect-error-message');
  l10n.localizeElementFromTag(errorDiv, /** @type {string} */ (errorTag));
  remoting.accessCode = '';
  if (remoting.session) {
    remoting.session.disconnect();
    remoting.session = null;
  }
  remoting.setMode(remoting.AppMode.CLIENT_CONNECT_FAILED);
}

/**
 * @param {XMLHttpRequest} xhr The XMLHttpRequest object.
 * @return {void} Nothing.
 */
function parseServerResponse_(xhr) {
  remoting.supportHostsXhr = null;
  remoting.debug.log('parseServerResponse: status = ' + xhr.status);
  if (xhr.status == 200) {
    var host = JSON.parse(xhr.responseText);
    if (host.data && host.data.jabberId) {
      remoting.hostJid = host.data.jabberId;
      remoting.hostPublicKey = host.data.publicKey;
      var split = remoting.hostJid.split('/');
      document.getElementById('connected-to').innerText = split[0];
      startSession_();
      return;
    }
  }
  var errorMsg = remoting.ClientError.OTHER_ERROR;
  if (xhr.status == 404) {
    errorMsg = remoting.ClientError.INVALID_ACCESS_CODE;
  } else if (xhr.status == 0) {
    errorMsg = remoting.ClientError.NO_RESPONSE;
  } else {
    remoting.debug.log('The server responded: ' + xhr.responseText);
  }
  showConnectError_(errorMsg);
}

function normalizeAccessCode_(accessCode) {
  // Trim whitespace.
  // TODO(sergeyu): Do we need to do any other normalization here?
  return accessCode.replace(/\s/g, '');
}

function resolveSupportId(supportId) {
  var headers = {
    'Authorization': 'OAuth ' + remoting.oauth2.getAccessToken()
  };

  remoting.supportHostsXhr = remoting.xhr.get(
      'https://www.googleapis.com/chromoting/v1/support-hosts/' +
          encodeURIComponent(supportId),
      parseServerResponse_,
      '',
      headers);
}

remoting.tryConnect = function() {
  document.getElementById('cancel-button').disabled = false;
  if (remoting.oauth2.needsNewAccessToken()) {
    remoting.oauth2.refreshAccessToken(function(xhr) {
      if (remoting.oauth2.needsNewAccessToken()) {
        // Failed to get access token
        remoting.debug.log('tryConnect: OAuth2 token fetch failed');
        showConnectError_(remoting.ClientError.OAUTH_FETCH_FAILED);
        return;
      }
      remoting.tryConnectWithAccessToken();
    });
  } else {
    remoting.tryConnectWithAccessToken();
  }
}

remoting.tryConnectWithAccessToken = function() {
  if (!remoting.wcsLoader) {
    remoting.wcsLoader = new remoting.WcsLoader();
  }
  remoting.wcsLoader.start(
      remoting.oauth2.getAccessToken(),
      function(setToken) {
        remoting.oauth2.callWithToken(setToken);
      },
      remoting.tryConnectWithWcs);
}

remoting.tryConnectWithWcs = function() {
  var accessCode = document.getElementById('access-code-entry').value;
  remoting.accessCode = normalizeAccessCode_(accessCode);
  // At present, only 12-digit access codes are supported, of which the first
  // 7 characters are the supportId.
  if (remoting.accessCode.length != kAccessCodeLen) {
    remoting.debug.log('Bad access code length');
    showConnectError_(remoting.ClientError.INVALID_ACCESS_CODE);
  } else {
    var supportId = remoting.accessCode.substring(0, kSupportIdLen);
    remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);
    resolveSupportId(supportId);
  }
}

remoting.cancelPendingOperation = function() {
  document.getElementById('cancel-button').disabled = true;
  switch (remoting.getMajorMode()) {
    case remoting.AppMode.HOST:
      remoting.cancelShare();
      break;
    case remoting.AppMode.CLIENT:
      remoting.cancelConnect();
      break;
  }
}

/**
 * Gets the major-mode that this application should start up in.
 *
 * @return {remoting.AppMode} The mode to start in.
 */
function getAppStartupMode() {
  if (!remoting.oauth2.isAuthenticated()) {
    return remoting.AppMode.UNAUTHENTICATED;
  }
  if (isHostModeSupported()) {
    return remoting.AppMode.HOME;
  } else {
    return remoting.AppMode.CLIENT_UNCONNECTED;
  }
}

/**
 * Returns whether Host mode is supported on this platform.
 *
 * @return {boolean} True if Host mode is supported.
 */
function isHostModeSupported() {
  // Currently, sharing on Chromebooks is not supported.
  return !navigator.userAgent.match(/\bCrOS\b/);
}

/**
 * Enable or disable scale-to-fit.
 *
 * @param {Element} button The scale-to-fit button. The style of this button is
 * updated to reflect the new scaling state.
 * @return {void} Nothing.
 */
remoting.toggleScaleToFit = function(button) {
  remoting.scaleToFit = !remoting.scaleToFit;
  if (remoting.scaleToFit) {
    addClass(button, 'toggle-button-active');
  } else {
    removeClass(button, 'toggle-button-active');
  }
  remoting.session.updateDimensions();
}

/**
 * Update the remoting client layout in response to a resize event.
 *
 * @return {void} Nothing.
 */
remoting.onResize = function() {
  if (remoting.session)
    remoting.session.onWindowSizeChanged();
  recenterToolbar_();
}

/**
 * Disconnect the remoting client.
 *
 * @return {void} Nothing.
 */
remoting.disconnect = function() {
  if (remoting.session) {
    remoting.session.disconnect();
    remoting.session = null;
    remoting.debug.log('Disconnected.');
    remoting.setMode(remoting.AppMode.CLIENT_SESSION_FINISHED);
  }
}

/**
 * If the client is connected, or the host is shared, prompt before closing.
 *
 * @return {?string} The prompt string if a connection is active.
 */
remoting.promptClose = function() {
  switch (remoting.currentMode) {
    case remoting.AppMode.CLIENT_CONNECTING:
    case remoting.AppMode.HOST_WAITING_FOR_CODE:
    case remoting.AppMode.HOST_WAITING_FOR_CONNECTION:
    case remoting.AppMode.HOST_SHARED:
    case remoting.AppMode.IN_SESSION:
      var result = chrome.i18n.getMessage(/*i18n-content*/'CLOSE_PROMPT');
      return result;
    default:
      return null;
  }
}

remoting.checkHotkeys = function(event) {
  if (String.fromCharCode(event.which) == 'D') {
    remoting.toggleDebugLog();
  }
}

function recenterToolbar_() {
  var toolbar = document.getElementById('session-toolbar');
  var toolbarX = (window.innerWidth - toolbar.clientWidth) / 2;
  toolbar.style['left'] = toolbarX + 'px';
}

}());
