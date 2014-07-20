// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Session class that handles creation and teardown of a remoting session.
 *
 * This abstracts a <embed> element and controls the plugin which does the
 * actual remoting work.  There should be no UI code inside this class.  It
 * should be purely thought of as a controller of sorts.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {
/**
 * @param {string} hostJid The jid of the host to connect to.
 * @param {string} hostPublicKey The base64 encoded version of the host's
 *     public key.
 * @param {string} accessCode The access code for the IT2Me connection.
 * @param {string} email The username for the talk network.
 * @param {function(remoting.ClientSession.State):void} onStateChange
 *     The callback to invoke when the session changes state. This callback
 *     occurs after the state changes and is passed the previous state; the
 *     new state is accessible via ClientSession's |state| property.
 * @constructor
 */
remoting.ClientSession = function(hostJid, hostPublicKey, accessCode, email,
                                  onStateChange) {
  this.state = remoting.ClientSession.State.CREATED;

  this.hostJid = hostJid;
  this.hostPublicKey = hostPublicKey;
  this.accessCode = accessCode;
  this.email = email;
  this.clientJid = '';
  this.onStateChange = onStateChange;
};

/** @enum {number} */
remoting.ClientSession.State = {
  UNKNOWN: 0,
  CREATED: 1,
  BAD_PLUGIN_VERSION: 2,
  UNKNOWN_PLUGIN_ERROR: 3,
  CONNECTING: 4,
  INITIALIZING: 5,
  CONNECTED: 6,
  CLOSED: 7,
  CONNECTION_FAILED: 8
};

/** @enum {number} */
remoting.ClientSession.ConnectionError = {
  NONE: 0,
  HOST_IS_OFFLINE: 1,
  SESSION_REJECTED: 2,
  INCOMPATIBLE_PROTOCOL: 3,
  NETWORK_FAILURE: 4,
  OTHER: 5
};

/**
 * The current state of the session.
 * @type {remoting.ClientSession.State}
 */
remoting.ClientSession.prototype.state = remoting.ClientSession.State.UNKNOWN;

/**
 * The last connection error. Set when state is set to CONNECTION_FAILED.
 * @type {remoting.ClientSession.ConnectionError}
 */
remoting.ClientSession.prototype.error =
    remoting.ClientSession.ConnectionError.NONE;

/**
 * Chromoting session API version (for this javascript).
 * This is compared with the plugin API version to verify that they are
 * compatible.
 *
 * @const
 * @private
 */
remoting.ClientSession.prototype.API_VERSION_ = 2;

/**
 * The oldest API version that we support.
 * This will differ from the |API_VERSION_| if we maintain backward
 * compatibility with older API versions.
 *
 * @const
 * @private
 */
remoting.ClientSession.prototype.API_MIN_VERSION_ = 1;

/**
 * Server used to bridge into the Jabber network for establishing Jingle
 * connections.
 *
 * @const
 * @private
 */
remoting.ClientSession.prototype.HTTP_XMPP_PROXY_ =
    'https://chromoting-httpxmpp-oauth2-dev.corp.google.com';

/**
 * The id of the client plugin
 *
 * @const
 */
remoting.ClientSession.prototype.PLUGIN_ID = 'session-client-plugin';

/**
 * Callback to invoke when the state is changed.
 *
 * @type {function(remoting.ClientSession.State):void}
 */
remoting.ClientSession.prototype.onStateChange = function(state) { };

/**
 * Adds <embed> element to |container| and readies the sesion object.
 *
 * @param {Element} container The element to add the plugin to.
 * @param {string} oauth2AccessToken A valid OAuth2 access token.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.createPluginAndConnect =
    function(container, oauth2AccessToken) {
  this.plugin = /** @type {remoting.ViewerPlugin} */
      document.createElement('embed');
  this.plugin.id = this.PLUGIN_ID;
  this.plugin.src = 'about://none';
  this.plugin.type = 'pepper-application/x-chromoting';
  this.plugin.width = 0;
  this.plugin.height = 0;
  container.appendChild(this.plugin);

  if (!this.isPluginVersionSupported_(this.plugin)) {
    // TODO(ajwong): Remove from parent.
    delete this.plugin;
    this.setState_(remoting.ClientSession.State.BAD_PLUGIN_VERSION);
    return;
  }

  var that = this;
  this.plugin.sendIq = function(msg) { that.sendIq_(msg); };
  this.plugin.debugInfo = function(msg) {
    remoting.debug.log('plugin: ' + msg);
  };

  // TODO(ajwong): Is it even worth having this class handle these events?
  // Or would it be better to just allow users to pass in their own handlers
  // and leave these blank by default?
  this.plugin.connectionInfoUpdate = function() {
    that.connectionInfoUpdateCallback();
  };
  this.plugin.desktopSizeUpdate = function() { that.onDesktopSizeChanged_(); };

  // For IT2Me, we are pre-authorized so there is no login challenge.
  this.plugin.loginChallenge = function() {};

  // TODO(garykac): Clean exit if |connect| isn't a function.
  if (typeof this.plugin.connect === 'function') {
    this.connectPluginToWcs_(oauth2AccessToken);
  } else {
    remoting.debug.log('ERROR: remoting plugin not loaded');
    this.setState_(remoting.ClientSession.State.UNKNOWN_PLUGIN_ERROR);
  }
};

/**
 * Deletes the <embed> element from the container, without sending a
 * session_terminate request.  This is to be called when the session was
 * disconnected by the Host.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.removePlugin = function() {
  var plugin = document.getElementById(this.PLUGIN_ID);
  if (plugin) {
    var parentNode = this.plugin.parentNode;
    parentNode.removeChild(plugin);
    plugin = null;
  }
}

/**
 * Deletes the <embed> element from the container and disconnects.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.disconnect = function() {
  if (remoting.wcs) {
    remoting.wcs.setOnIq(function(stanza) {});
  }
  this.removePlugin();
  this.sendIq_(
      '<cli:iq ' +
          'to="' + this.hostJid + '" ' +
          'type="set" ' +
          'id="session-terminate" ' +
          'xmlns:cli="jabber:client">' +
        '<jingle ' +
            'xmlns="urn:xmpp:jingle:1" ' +
            'action="session-terminate" ' +
            'initiator="' + this.clientJid + '" ' +
            'sid="' + this.sessionId + '">' +
          '<reason><success/></reason>' +
        '</jingle>' +
      '</cli:iq>');
};

/**
 * Sends an IQ stanza via the http xmpp proxy.
 *
 * @private
 * @param {string} msg XML string of IQ stanza to send to server.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.sendIq_ = function(msg) {
  remoting.debug.log('Sending Iq: ' + msg);
  // Extract the session id, so we can close the session later.
  var parser = new DOMParser();
  var iqNode = parser.parseFromString(msg, 'text/xml').firstChild;
  var jingleNode = iqNode.firstChild;
  if (jingleNode) {
    var action = jingleNode.getAttribute('action');
    if (jingleNode.nodeName == 'jingle' && action == 'session-initiate') {
      this.sessionId = jingleNode.getAttribute('sid');
    }
  }

  // Send the stanza.
  if (remoting.wcs) {
    remoting.wcs.sendIq(msg);
  } else {
    remoting.debug.log('Tried to send IQ before WCS was ready.');
    this.setState_(remoting.ClientSession.State.CONNECTION_FAILED);
  }
};

/**
 * @private
 * @param {remoting.ViewerPlugin} plugin The embed element for the plugin.
 * @return {boolean} True if the plugin and web-app versions are compatible.
 */
remoting.ClientSession.prototype.isPluginVersionSupported_ = function(plugin) {
  return this.API_VERSION_ >= plugin.apiMinVersion &&
      plugin.apiVersion >= this.API_MIN_VERSION_;
};

/**
 * Connects the plugin to WCS.
 *
 * @private
 * @param {string} oauth2AccessToken A valid OAuth2 access token.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.connectPluginToWcs_ =
    function(oauth2AccessToken) {
  this.clientJid = remoting.wcs.getJid();
  if (this.clientJid == '') {
    remoting.debug.log('Tried to connect without a full JID.');
  }
  var that = this;
  remoting.wcs.setOnIq(function(stanza) {
      remoting.debug.log('Receiving Iq: ' + stanza);
      that.plugin.onIq(stanza);
  });
  that.plugin.connect(this.hostJid, this.hostPublicKey, this.clientJid,
                      this.accessCode);
};

/**
 * Callback that the plugin invokes to indicate that the connection
 * status has changed.
 */
remoting.ClientSession.prototype.connectionInfoUpdateCallback = function() {
  var state = this.plugin.status;

  // TODO(ajwong): We're doing silly type translation here. Any way to avoid?
  if (state == this.plugin.STATUS_UNKNOWN) {
    this.setState_(remoting.ClientSession.State.UNKNOWN);
  } else if (state == this.plugin.STATUS_CONNECTING) {
    this.setState_(remoting.ClientSession.State.CONNECTING);
  } else if (state == this.plugin.STATUS_INITIALIZING) {
    this.setState_(remoting.ClientSession.State.INITIALIZING);
  } else if (state == this.plugin.STATUS_CONNECTED) {
    this.onDesktopSizeChanged_();
    this.setState_(remoting.ClientSession.State.CONNECTED);
  } else if (state == this.plugin.STATUS_CLOSED) {
    this.setState_(remoting.ClientSession.State.CLOSED);
  } else if (state == this.plugin.STATUS_FAILED) {
    var error = this.plugin.error;
    if (error == this.plugin.ERROR_HOST_IS_OFFLINE) {
      this.error = remoting.ClientSession.ConnectionError.HOST_IS_OFFLINE;
    } else if (error == this.plugin.ERROR_SESSION_REJECTED) {
      this.error = remoting.ClientSession.ConnectionError.SESSION_REJECTED;
    } else if (error == this.plugin.ERROR_INCOMPATIBLE_PROTOCOL) {
      this.error = remoting.ClientSession.ConnectionError.INCOMPATIBLE_PROTOCOL;
    } else if (error == this.plugin.NETWORK_FAILURE) {
      this.error = remoting.ClientSession.ConnectionError.NETWORK_FAILURE;
    } else {
      this.error = remoting.ClientSession.ConnectionError.OTHER;
    }
    this.setState_(remoting.ClientSession.State.CONNECTION_FAILED);
  }
};

/**
 * @private
 * @param {remoting.ClientSession.State} state The new state for the session.
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.setState_ = function(state) {
  var oldState = this.state;
  this.state = state;
  if (this.onStateChange) {
    this.onStateChange(oldState);
  }
};

/**
 * This is a callback that gets called when the window is resized.
 *
 * @private
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.onWindowSizeChanged = function() {
  remoting.debug.log('window size changed: ' +
                     window.innerWidth + 'x' +
                     window.innerHeight);
  this.updateDimensions();
};

/**
 * This is a callback that gets called when the plugin notifies us of a change
 * in the size of the remote desktop.
 *
 * @private
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.onDesktopSizeChanged_ = function() {
  remoting.debug.log('desktop size changed: ' +
                     this.plugin.desktopWidth + 'x' +
                     this.plugin.desktopHeight);
  this.updateDimensions();
};

/**
 * Refreshes the plugin's dimensions, taking into account the sizes of the
 * remote desktop and client window, and the current scale-to-fit setting.
 *
 * @return {void} Nothing.
 */
remoting.ClientSession.prototype.updateDimensions = function() {
  if (this.plugin.desktopWidth == 0 ||
      this.plugin.desktopHeight == 0)
    return;

  var windowWidth = window.innerWidth;
  var windowHeight = window.innerHeight;
  var scale = 1.0;

  if (remoting.scaleToFit) {
    var scaleFitHeight = 1.0 * windowHeight / this.plugin.desktopHeight;
    var scaleFitWidth = 1.0 * windowWidth / this.plugin.desktopWidth;
    scale = Math.min(1.0, scaleFitHeight, scaleFitWidth);
  }

  // Resize the plugin if necessary.
  this.plugin.width = this.plugin.desktopWidth * scale;
  this.plugin.height = this.plugin.desktopHeight * scale;

  // Position the container.
  // TODO(wez): We should take into account scrollbars when positioning.
  var parentNode = this.plugin.parentNode;
  if (this.plugin.width < windowWidth)
    parentNode.style.left = (windowWidth - this.plugin.width) / 2 + 'px';
  else
    parentNode.style.left = 0;
  if (this.plugin.height < windowHeight)
    parentNode.style.top = (windowHeight - this.plugin.height) / 2 + 'px';
  else
    parentNode.style.top = 0;

  remoting.debug.log('plugin dimensions: ' +
                     parentNode.style.left + ',' +
                     parentNode.style.top + '-' +
                     this.plugin.width + 'x' + this.plugin.height + '.');
  this.plugin.setScaleToFit(remoting.scaleToFit);
};

/**
 * Returns an associative array with a set of stats for this connection.
 *
 * @return {Object} The connection statistics.
 */
remoting.ClientSession.prototype.stats = function() {
  return {
    'video_bandwidth': this.plugin.videoBandwidth,
    'capture_latency': this.plugin.videoCaptureLatency,
    'encode_latency': this.plugin.videoEncodeLatency,
    'decode_latency': this.plugin.videoDecodeLatency,
    'render_latency': this.plugin.videoRenderLatency,
    'roundtrip_latency': this.plugin.roundTripLatency
  };
};

}());
