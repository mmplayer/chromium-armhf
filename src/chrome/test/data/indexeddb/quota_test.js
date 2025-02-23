// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test() {
  if (window.webkitStorageInfo) {
    window.jsTestIsAsync = true;
    webkitStorageInfo.queryUsageAndQuota(webkitStorageInfo.TEMPORARY,
                                         initUsageCallback,
                                         unexpectedErrorCallback);
  } else
    debug("This test requires window.webkitStorageInfo.");
}

function initUsageCallback(usage, quota) {
  origReturnedUsage = returnedUsage = usage;
  origReturnedQuota = returnedQuota = quota;
  debug("original quota is " + displaySize(origReturnedQuota));
  debug("original usage is " + displaySize(origReturnedUsage));

  request = webkitIndexedDB.open('database-quota');
  request.onsuccess = openSuccess;
  request.onerror = unexpectedErrorCallback;
}

function openSuccess() {
  window.db = event.target.result;

  request = db.setVersion('new version');
  request.onsuccess = setVersionSuccess;
  request.onerror = unexpectedErrorCallback;
}

function setVersionSuccess() {
  window.trans = event.target.result;
  trans.onabort = unexpectedAbortCallback;
  trans.oncomplete = initQuotaEnforcing;

  deleteAllObjectStores(db);

  objectStore = db.createObjectStore("test123");
}

function displaySize(bytes) {
  var k = bytes / 1024;
  var m = k / 1024;
  return bytes + " (" + k + "k) (" + m + "m)";
}

function initQuotaEnforcing() {
  var availableSpace = origReturnedQuota - origReturnedUsage;
  var kMaxMbPerWrite = 5;
  var kMinWrites = 5;
  var len = Math.min(kMaxMbPerWrite * 1024 * 1024,
                     Math.floor(availableSpace / kMinWrites));
  maxExpectedWrites = Math.floor(availableSpace / len) + 1;
  debug("Chunk size: " + displaySize(len));
  debug("Expecting at most " + maxExpectedWrites + " writes, but we could " +
        "have more if snappy is used or LevelDB is about to compact.");
  data = Array(1+len).join("X");
  dataLength = data.length;
  dataAdded = 0;
  successfulWrites = 0;
  startNewTransaction();
}

function startNewTransaction() {
  if (dataAdded > origReturnedQuota) {
    fail("dataAdded > quota " + dataAdded + " > " + origReturnedQuota);
    return;
  }
  debug("");
  debug("Starting new transaction.");

  var trans = db.transaction([], webkitIDBTransaction.READ_WRITE);
  trans.onabort = onAbort;
  trans.oncomplete = getQuotaAndUsage;
  var store = trans.objectStore('test123');
  request = store.put({x: data}, dataAdded);
  request.onerror = logError;
}

function getQuotaAndUsage() {
  successfulWrites++;
  if (successfulWrites > maxExpectedWrites) {
    debug("Weird: too many writes. There were " + successfulWrites +
          " but we only expected " + maxExpectedWrites);
  }
  webkitStorageInfo.queryUsageAndQuota(webkitStorageInfo.TEMPORARY,
                                       usageCallback, unexpectedErrorCallback);
}

function usageCallback(usage, quota) {
  debug("Transaction finished.");
  dataAdded += dataLength;
  debug("We've added "+ displaySize(dataAdded));
  returnedUsage = usage;
  returnedQuota = quota;
  debug("Allotted quota is " + displaySize(returnedQuota));
  debug("LevelDB usage is " + displaySize(returnedUsage));
  startNewTransaction();
}

function onAbort() {
  done("Transaction aborted. Data added: " + displaySize(dataAdded));
  debug("There were " + successfulWrites + " successful writes");
}

function logError() {
  debug("Error function called: (" + event.target.errorCode + ") " +
        event.target.webkitErrorMessage);
  event.preventDefault();
}

