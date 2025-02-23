// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY2_UI_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY2_UI_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"
#include "content/browser/cancelable_request.h"
#include "content/common/notification_registrar.h"

class GURL;

// Temporary fork for development of new history UI.
// TODO(pamg): merge back in when new UI is complete.

class HistoryUIHTMLSource2 : public ChromeURLDataManager::DataSource {
 public:
  HistoryUIHTMLSource2();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);

  virtual std::string GetMimeType(const std::string&) const;

 private:
  virtual ~HistoryUIHTMLSource2() {}

  DISALLOW_COPY_AND_ASSIGN(HistoryUIHTMLSource2);
};

// The handler for Javascript messages related to the "history" view.
class BrowsingHistoryHandler2 : public WebUIMessageHandler,
                                public NotificationObserver {
 public:
  BrowsingHistoryHandler2();
  virtual ~BrowsingHistoryHandler2();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // Callback for the "getHistory" message.
  void HandleGetHistory(const base::ListValue* args);

  // Callback for the "searchHistory" message.
  void HandleSearchHistory(const base::ListValue* args);

  // Callback for the "removeURLsOnOneDay" message.
  void HandleRemoveURLsOnOneDay(const base::ListValue* args);

  // Handle for "clearBrowsingData" message.
  void HandleClearBrowsingData(const base::ListValue* args);

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  // Callback from the history system when the history list is available.
  void QueryComplete(HistoryService::Handle request_handle,
                     history::QueryResults* results);

  // Callback from the history system when visits were deleted.
  void RemoveComplete();

  // Extract the arguments from the call to HandleSearchHistory.
  void ExtractSearchHistoryArguments(const base::ListValue* args,
                                     int* month,
                                     string16* query);

  // Figure out the query options for a month-wide query.
  history::QueryOptions CreateMonthQueryOptions(int month);

  NotificationRegistrar registrar_;

  // Current search text.
  string16 search_text_;

  // Our consumer for search requests to the history service.
  CancelableRequestConsumerT<int, 0> cancelable_search_consumer_;

  // Our consumer for delete requests to the history service.
  CancelableRequestConsumerT<int, 0> cancelable_delete_consumer_;

  // The list of URLs that are in the process of being deleted.
  std::set<GURL> urls_to_be_deleted_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingHistoryHandler2);
};

class HistoryUI2 : public ChromeWebUI {
 public:
  explicit HistoryUI2(TabContents* contents);

  // Return the URL for a given search term.
  static const GURL GetHistoryURLWithSearchText(const string16& text);

  static RefCountedMemory* GetFaviconResourceBytes();

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryUI2);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY2_UI_H_
