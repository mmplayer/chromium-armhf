// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_BACK_FORWARD_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_BACK_FORWARD_MENU_MODEL_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "ui/base/models/menu_model.h"
#include "webkit/glue/window_open_disposition.h"

class Browser;
class SkBitmap;
class TabContents;
class NavigationEntry;

///////////////////////////////////////////////////////////////////////////////
//
// BackForwardMenuModel
//
// Interface for the showing of the dropdown menu for the Back/Forward buttons.
// Actual implementations are platform-specific.
///////////////////////////////////////////////////////////////////////////////
class BackForwardMenuModel : public ui::MenuModel {
 public:
  // These are IDs used to identify individual UI elements within the
  // browser window using View::GetViewByID.
  enum ModelType {
    FORWARD_MENU = 1,
    BACKWARD_MENU = 2
  };

  BackForwardMenuModel(Browser* browser, ModelType model_type);
  virtual ~BackForwardMenuModel();

  // MenuModel implementation.
  virtual bool HasIcons() const;
  // Returns how many items the menu should show, including history items,
  // chapter-stops, separators and the Show Full History link. This function
  // uses GetHistoryItemCount() and GetChapterStopCount() internally to figure
  // out the total number of items to show.
  virtual int GetItemCount() const;
  virtual ItemType GetTypeAt(int index) const;
  virtual int GetCommandIdAt(int index) const;
  virtual string16 GetLabelAt(int index) const;
  virtual bool IsItemDynamicAt(int index) const;
  virtual bool GetAcceleratorAt(int index,
                                ui::Accelerator* accelerator) const;
  virtual bool IsItemCheckedAt(int index) const;
  virtual int GetGroupIdAt(int index) const;
  virtual bool GetIconAt(int index, SkBitmap* icon);
  virtual ui::ButtonMenuItemModel* GetButtonMenuItemAt(int index) const;
  virtual bool IsEnabledAt(int index) const;
  virtual MenuModel* GetSubmenuModelAt(int index) const;
  virtual void HighlightChangedTo(int index);
  virtual void ActivatedAt(int index) OVERRIDE;
  virtual void ActivatedAt(int index, int event_flags) OVERRIDE;
  virtual void MenuWillShow();

  // Is the item at |index| a separator?
  bool IsSeparator(int index) const;

  // Set the delegate for triggering OnIconChanged.
  virtual void SetMenuModelDelegate(ui::MenuModelDelegate* menu_model_delegate);

 protected:
   ui::MenuModelDelegate* menu_model_delegate() { return menu_model_delegate_; }

 private:
  friend class BackFwdMenuModelTest;
  FRIEND_TEST_ALL_PREFIXES(BackFwdMenuModelTest, BasicCase);
  FRIEND_TEST_ALL_PREFIXES(BackFwdMenuModelTest, MaxItemsTest);
  FRIEND_TEST_ALL_PREFIXES(BackFwdMenuModelTest, ChapterStops);
  FRIEND_TEST_ALL_PREFIXES(BackFwdMenuModelTest, EscapeLabel);
  FRIEND_TEST_ALL_PREFIXES(BackFwdMenuModelTest, FaviconLoadTest);

  // Requests a favicon from the FaviconService. Called by GetIconAt if the
  // NavigationEntry has an invalid favicon.
  void FetchFavicon(NavigationEntry* entry);

  // Callback from the favicon service.
  void OnFavIconDataAvailable(FaviconService::Handle handle,
                              history::FaviconData favicon);

  // Allows the unit test to use its own dummy tab contents.
  void set_test_tab_contents(TabContents* test_tab_contents) {
    test_tab_contents_ = test_tab_contents;
  }

  // Returns how many history items the menu should show. For example, if the
  // navigation controller of the current tab has a current entry index of 5 and
  // forward_direction_ is false (we are the back button delegate) then this
  // function will return 5 (representing 0-4). If forward_direction_ is
  // true (we are the forward button delegate), then this function will return
  // the number of entries after 5. Note, though, that in either case it will
  // not report more than kMaxHistoryItems. The number returned also does not
  // include the separator line after the history items (nor the separator for
  // the "Show Full History" link).
  int GetHistoryItemCount() const;

  // Returns how many chapter-stop items the menu should show. For the
  // definition of a chapter-stop, see GetIndexOfNextChapterStop(). The number
  // returned does not include the separator lines before and after the
  // chapter-stops.
  int GetChapterStopCount(int history_items) const;

  // Finds the next chapter-stop in the NavigationEntryList starting from
  // the index specified in |start_from| and continuing in the direction
  // specified (|forward|) until either a chapter-stop is found or we reach the
  // end, in which case -1 is returned. If |start_from| is out of bounds, -1
  // will also be returned. A chapter-stop is defined as the last page the user
  // browsed to within the same domain. For example, if the user's homepage is
  // Google and she navigates to Google pages G1, G2 and G3 before heading over
  // to WikiPedia for pages W1 and W2 and then back to Google for pages G4 and
  // G5 then G3, W2 and G5 are considered chapter-stops. The return value from
  // this function is an index into the NavigationEntryList vector.
  int GetIndexOfNextChapterStop(int start_from, bool forward) const;

  // Finds a given chapter-stop starting at the currently active entry in the
  // NavigationEntryList vector advancing first forward or backward by |offset|
  // (depending on the direction specified in parameter |forward|). It also
  // allows you to skip chapter-stops by specifying a positive value for |skip|.
  // Example: FindChapterStop(5, false, 3) starts with the currently active
  // index, subtracts 5 from it and then finds the fourth chapter-stop before
  // that index (skipping the first 3 it finds).
  // Example: FindChapterStop(0, true, 0) is functionally equivalent to
  // calling GetIndexOfNextChapterStop(GetCurrentEntryIndex(), true).
  //
  // NOTE: Both |offset| and |skip| must be non-negative. The return value from
  // this function is an index into the NavigationEntryList vector. If |offset|
  // is out of bounds or if we skip too far (run out of chapter-stops) this
  // function returns -1.
  int FindChapterStop(int offset, bool forward, int skip) const;

  // How many items (max) to show in the back/forward history menu dropdown.
  static const int kMaxHistoryItems;

  // How many chapter-stops (max) to show in the back/forward dropdown list.
  static const int kMaxChapterStops;

  // Takes a menu item index as passed in through one of the menu delegate
  // functions and converts it into an index into the NavigationEntryList
  // vector. |index| can point to a separator, or the
  // "Show Full History" link in which case this function returns -1.
  int MenuIndexToNavEntryIndex(int index) const;

  // Does the item have a command associated with it?
  bool ItemHasCommand(int index) const;

  // Returns true if there is an icon for this menu item.
  bool ItemHasIcon(int index) const;

  // Allow the unit test to use the "Show Full History" label.
  string16 GetShowFullHistoryLabel() const;

  // Looks up a NavigationEntry by menu id.
  NavigationEntry* GetNavigationEntry(int index) const;

  // Retrieves the TabContents pointer to use, which is either the one that
  // the unit test sets (using SetTabContentsForUnitTest) or the one from
  // the browser window.
  TabContents* GetTabContents() const;

  // Build a string version of a user action on this menu, used as an
  // identifier for logging user behavior.
  // E.g. BuildActionName("Click", 2) returns "BackMenu_Click2".
  // An index of -1 means no index.
  std::string BuildActionName(const std::string& name, int index) const;

  Browser* browser_;

  // The unit tests will provide their own TabContents to use.
  TabContents* test_tab_contents_;

  // Represents whether this is the delegate for the forward button or the
  // back button.
  ModelType model_type_;

  // Keeps track of which favicons have already been requested from the history
  // to prevent duplicate requests, identified by NavigationEntry->unique_id().
  std::set<int> requested_favicons_;

  // Used for loading favicons from history.
  CancelableRequestConsumerTSimple<int> load_consumer_;

  // Used for receiving notifications when an icon is changed.
  ui::MenuModelDelegate* menu_model_delegate_;

  DISALLOW_COPY_AND_ASSIGN(BackForwardMenuModel);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_BACK_FORWARD_MENU_MODEL_H_
