// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information on the state of all socket pools.
 *
 *   - Shows a summary of the state of each socket pool at the top.
 *   - For each pool with allocated sockets or connect jobs, shows all its
 *     groups with any allocated sockets.
 */
var SocketsView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function SocketsView() {
    assertFirstConstructorCall(SocketsView);

    // Call superclass's constructor.
    superClass.call(this, SocketsView.MAIN_BOX_ID);

    g_browser.addSocketPoolInfoObserver(this);
    this.socketPoolDiv_ = $(SocketsView.SOCKET_POOL_DIV_ID);
    this.socketPoolGroupsDiv_ = $(SocketsView.SOCKET_POOL_GROUPS_DIV_ID);

    var closeIdleButton = $(SocketsView.CLOSE_IDLE_SOCKETS_BUTTON_ID);
    closeIdleButton.onclick = this.closeIdleSockets.bind(this);

    var flushSocketsButton = $(SocketsView.SOCKET_POOL_FLUSH_BUTTON_ID);
    flushSocketsButton.onclick = this.flushSocketPools.bind(this);
  }

  // ID for special HTML element in category_tabs.html
  SocketsView.TAB_HANDLE_ID = 'tab-handle-sockets';

  // IDs for special HTML elements in sockets_view.html
  SocketsView.MAIN_BOX_ID = 'sockets-view-tab-content';
  SocketsView.SOCKET_POOL_DIV_ID = 'sockets-view-pool-div';
  SocketsView.SOCKET_POOL_GROUPS_DIV_ID = 'sockets-view-pool-groups-div';
  SocketsView.CLOSE_IDLE_SOCKETS_BUTTON_ID = 'sockets-view-close-idle-button';
  SocketsView.SOCKET_POOL_FLUSH_BUTTON_ID = 'sockets-view-flush-button';

  cr.addSingletonGetter(SocketsView);

  SocketsView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onLoadLogFinish: function(data) {
      return this.onSocketPoolInfoChanged(data.socketPoolInfo);
    },

    onSocketPoolInfoChanged: function(socketPoolInfo) {
      this.socketPoolDiv_.innerHTML = '';
      this.socketPoolGroupsDiv_.innerHTML = '';

      if (!socketPoolInfo)
        return false;

      var socketPools = SocketPoolWrapper.createArrayFrom(socketPoolInfo);
      var tablePrinter = SocketPoolWrapper.createTablePrinter(socketPools);
      tablePrinter.toHTML(this.socketPoolDiv_, 'styledTable');

      // Add table for each socket pool with information on each of its groups.
      for (var i = 0; i < socketPools.length; ++i) {
        if (socketPools[i].origPool.groups != undefined) {
          var p = addNode(this.socketPoolGroupsDiv_, 'p');
          var br = addNode(p, 'br');
          var groupTablePrinter = socketPools[i].createGroupTablePrinter();
          groupTablePrinter.toHTML(p, 'styledTable');
        }
      }
      return true;
    },

    closeIdleSockets: function() {
      g_browser.sendCloseIdleSockets();
      g_browser.checkForUpdatedInfo(false);
    },

    flushSocketPools: function() {
      g_browser.sendFlushSocketPools();
      g_browser.checkForUpdatedInfo(false);
    }
  };

  return SocketsView;
})();
