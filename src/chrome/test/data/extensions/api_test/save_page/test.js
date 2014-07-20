// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API test for chrome.extension.savePage.
// browser_tests.exe --gtest_filter=ExtensionApiTest.SavePage

const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;

var testUrl = 'http://www.a.com:PORT' +
    '/files/extensions/api_test/save_page/google.html';

function waitForCurrentTabLoaded(callback) {
  chrome.tabs.getSelected(null, function(tab) {
    if (tab.status == "complete" && tab.url == testUrl) {
      callback();
      return;
    }
    window.setTimeout(function() { waitForCurrentTabLoaded(callback); }, 100);
  });
}

chrome.test.getConfig(function(config) {
  testUrl = testUrl.replace(/PORT/, config.testServer.port);

  chrome.test.runTests([
    function savePageAsMHTML() {
      chrome.tabs.getSelected(null, function(tab) {
        chrome.tabs.update(null, { "url": testUrl });
        waitForCurrentTabLoaded(function() {
          chrome.experimental.savePage.saveAsMHTML(tab.id, function(data) {
            assertEq(undefined, chrome.extension.lastError);
            assertTrue(data != null);
            // It should contain few KBs of data.
            assertTrue(data.size > 100);
            // Let's make sure it contains some well known strings.
            var reader = new FileReader();
            reader.onload = function(e) {
              var text = e.target.result;
              assertTrue(text.indexOf(testUrl) != -1);
              assertTrue(text.indexOf("logo.png") != -1);
              chrome.test.notifyPass();
            };
            reader.readAsText(data);
          });
        });
      });
    }
  ]);
});

