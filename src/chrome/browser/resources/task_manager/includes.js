// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This script includes additional resources via document.write(). Hence, it
// must be a separate script file loaded before other scripts which would
// reference the resources.

var css = [
  'list.css',
  'table.css',
  'menu.css',
];

var script = [
  'local_strings.js',
  'i18n_template.js',

  'util.js',
  'cr.js',
  'cr/ui.js',
  'cr/event_target.js',
  'cr/ui/array_data_model.js',
  'cr/ui/list_item.js',
  'cr/ui/list_selection_model.js',
  'cr/ui/list_single_selection_model.js',
  'cr/ui/list_selection_controller.js',
  'cr/ui/list.js',

  'cr/ui/splitter.js',
  'cr/ui/table/table_splitter.js',

  'cr/ui/table/table_column.js',
  'cr/ui/table/table_column_model.js',
  'cr/ui/table/table_header.js',
  'cr/ui/table/table_list.js',
  'cr/ui/table.js',

  'cr/ui/grid.js',

  'cr/ui/command.js',
  'cr/ui/position_util.js',
  'cr/ui/menu_item.js',
  'cr/ui/menu.js',
  'cr/ui/context_menu_handler.js',
];

(function() {
  // Switch to 'test harness' mode when loading from a file url.
  var isHarness = document.location.protocol == 'file:';

  // In test harness mode we load resources from relative dirs.
  var prefix = isHarness ? './shared/' : 'chrome://resources/';

  for (var i = 0; i < css.length; ++i) {
    document.write('<link href="' + prefix + 'css/' + css[i] +
                   '" rel="stylesheet"></link>');
  }

  for (var i = 0; i < script.length; ++i) {
    document.write('<script src="' + prefix + 'js/' + script[i] +
                   '"><\/script>');
  }
})();
