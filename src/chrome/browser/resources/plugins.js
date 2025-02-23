// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This variable structure is here to document the structure that the template
 * expects to correctly populate the page.
 */
var pluginDataFormat = {
  'plugins': [
    {
      'name': 'Group Name',
      'description': 'description',
      'version': 'version',
      'update_url': 'http://update/',
      'critical': true,
      'enabled': true,
      'plugin_files': [
        {
          'path': '/blahblah/blahblah/MyCrappyPlugin.plugin',
          'name': 'MyCrappyPlugin',
          'version': '1.2.3',
          'description': 'My crappy plugin',
          'mimeTypes': [
            { 'description': 'Foo Media',
              'fileExtensions': [ 'foo' ],
              'mimeType': 'application/x-my-foo' },
            { 'description': 'Bar Stuff',
              'fileExtensions': [ 'bar','baz' ],
              'mimeType': 'application/my-bar' }
          ],
          'enabledMode': 'enabledByUser'
        },
        {
          'path': '/tmp/MyFirst.plugin',
          'name': 'MyFirstPlugin',
          'version': '3.14r15926',
          'description': 'My first plugin',
          'mimeTypes': [
            { 'description': 'New Guy Media',
              'fileExtensions': [ 'mfp' ],
              'mimeType': 'application/x-my-first' }
          ],
          'enabledMode': 'enabledByPolicy'
        },
        {
          'path': '/foobar/baz/YourGreatPlugin.plugin',
          'name': 'YourGreatPlugin',
          'version': '4.5',
          'description': 'Your great plugin',
          'mimeTypes': [
            { 'description': 'Baz Stuff',
              'fileExtensions': [ 'baz' ],
              'mimeType': 'application/x-your-baz' }
          ],
          'enabledMode': 'disabledByUser'
        },
        {
          'path': '/foobiz/bar/HisGreatPlugin.plugin',
          'name': 'HisGreatPlugin',
          'version': '1.2',
          'description': 'His great plugin',
          'mimeTypes': [
            { 'description': 'More baz Stuff',
              'fileExtensions': [ 'bor' ],
              'mimeType': 'application/x-his-bor' }
          ],
          'enabledMode': 'disabledByPolicy'
        }
      ]
    }
  ]
};

/**
 * Takes the |pluginsData| input argument which represents data about the
 * currently installed/running plugins and populates the html jstemplate with
 * that data. It expects an object structure like the above.
 * @param {Object} pluginsData Detailed info about installed plugins
 */
function renderTemplate(pluginsData) {
  // This is the javascript code that processes the template:
  var input = new JsEvalContext(pluginsData);
  var output = document.getElementById('pluginTemplate');
  jstProcess(input, output);
}

/**
 * Asks the C++ PluginsDOMHandler to get details about the installed plugins and
 * return detailed data about the configuration. The PluginsDOMHandler should
 * reply to returnPluginsData() (below).
 */
function requestPluginsData() {
  chrome.send('requestPluginsData', []);
  chrome.send('getShowDetails', []);
}

function loadShowDetailsFromPrefs(show_details) {
  tmiModeExpanded = show_details;
  document.getElementById('collapse').style.display =
      show_details ? 'inline' : 'none';
  document.getElementById('expand').style.display =
      show_details ? 'none' : 'inline';

  document.body.className =
      show_details ? 'showTmiMode' : 'hideTmiMode';
}

/**
 * Called by the web_ui_ to re-populate the page with data representing the
 * current state of installed plugins.
 */
function returnPluginsData(pluginsData){
  var bodyContainer = document.getElementById('body-container');
  var body = document.body;

  // Set all page content to be visible so we can measure heights.
  bodyContainer.style.visibility = 'hidden';
  body.className = '';
  var slidables = document.getElementsByClassName('showInTmiMode');
  for (var i = 0; i < slidables.length; i++)
    slidables[i].style.height = 'auto';

  renderTemplate(pluginsData);

  // Add handlers to dynamically created HTML elements.
  var links = document.getElementsByClassName('disable-plugin-link');
  for (var i = 0; i < links.length; ++i) {
    links[i].onclick = function () {
      handleEnablePlugin(this, false, false);
      return false;
    };
  }
  links = document.getElementsByClassName('enable-plugin-link');
  for (var i = 0; i < links.length; ++i) {
    links[i].onclick = function () {
      handleEnablePlugin(this, true, false);
      return false;
    };
  }
  links = document.getElementsByClassName('disable-group-link');
  for (var i = 0; i < links.length; ++i) {
    links[i].onclick = function () {
      handleEnablePlugin(this, false, true);
      return false;
    };
  }
  links = document.getElementsByClassName('enable-group-link');
  for (var i = 0; i < links.length; ++i) {
    links[i].onclick = function () {
      handleEnablePlugin(this, true, true);
      return false;
    };
  }

  // Make sure the left column (with "Description:", "Location:", etc.) is the
  // same size for all plugins.
  var labels = document.getElementsByClassName('plugin-details-label');
  var maxLabelWidth = 0;
  for (var i = 0; i < labels.length; i++)
    labels[i].style.width = 'auto';
  for (var i = 0; i < labels.length; i++)
    maxLabelWidth = Math.max(maxLabelWidth, labels[i].offsetWidth);
  for (var i = 0; i < labels.length; i++)
    labels[i].style.width = maxLabelWidth + 'px';

  // Explicitly set the height for each element that wants to be "slid" in and
  // out when the tmiModeExpanded is toggled.
  var slidables = document.getElementsByClassName('showInTmiMode');
  for (var i = 0; i < slidables.length; i++)
    slidables[i].style.height = slidables[i].offsetHeight + 'px';

  // Reset visibility of page based on the current tmi mode.
  document.getElementById('collapse').style.display =
     tmiModeExpanded ? 'inline' : 'none';
  document.getElementById('expand').style.display =
     tmiModeExpanded ? 'none' : 'inline';
  bodyContainer.style.visibility = 'visible';
  body.className = tmiModeExpanded ?
     'showTmiModeInitial' : 'hideTmiModeInitial';
}

/**
 * Handles a 'enable' or 'disable' button getting clicked.
 */
function handleEnablePlugin(node, enable, isGroup) {
  // Tell the C++ PluginsDOMHandler to enable/disable the plugin.
  chrome.send('enablePlugin', [String(node.path), String(enable),
              String(isGroup)]);
}

// Keeps track of whether details have been made visible (expanded) or not.
var tmiModeExpanded = false;

/*
 * Toggles visibility of details.
 */
function toggleTmiMode() {
  tmiModeExpanded = !tmiModeExpanded;

  document.getElementById('collapse').style.display =
      tmiModeExpanded ? 'inline' : 'none';
  document.getElementById('expand').style.display =
      tmiModeExpanded ? 'none' : 'inline';

  document.body.className =
      tmiModeExpanded ? 'showTmiMode' : 'hideTmiMode';

  chrome.send('saveShowDetailsToPrefs', [String(tmiModeExpanded)]);
}

/**
 * Determines whether a plugin's version should be displayed.
 */
function shouldDisplayPluginVersion(plugin) {
  return !!plugin.version && plugin.version != '0';
}

/**
 * Determines whether a plugin's description should be displayed.
 */
function shouldDisplayPluginDescription(plugin) {
  // Only display the description if it's not blank and if it's not just the
  // name, version, or combination thereof.
  return plugin.description &&
         plugin.description != plugin.name &&
         plugin.description != plugin.version &&
         plugin.description != 'Version ' + plugin.version &&
         plugin.description != plugin.name + ' ' + plugin.version;
}

/**
 * Determines whether a plugin is enabled or not.
 */
function isPluginEnabled(plugin) {
  return plugin.enabledMode == 'enabledByUser' ||
         plugin.enabledMode == 'enabledByPolicy';
}

// Unfortunately, we don't have notifications for plugin (list) status changes
// (yet), so in the meanwhile just update regularly.
setInterval(requestPluginsData, 30000);

// Get data and have it displayed upon loading.
document.addEventListener('DOMContentLoaded', requestPluginsData);

// Add handlers to static HTML elements.
document.getElementById('collapse').onclick = toggleTmiMode;
document.getElementById('expand').onclick = toggleTmiMode;
document.getElementById('details-link').onclick = toggleTmiMode;

