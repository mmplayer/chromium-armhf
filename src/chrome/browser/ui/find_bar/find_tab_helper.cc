// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/find_bar/find_tab_helper.h"

#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/common/notification_service.h"
#include "content/common/view_message_enums.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFindOptions.h"

using WebKit::WebFindOptions;

// static
int FindTabHelper::find_request_id_counter_ = -1;

FindTabHelper::FindTabHelper(TabContents* tab_contents)
    : TabContentsObserver(tab_contents),
      find_ui_active_(false),
      find_op_aborted_(false),
      current_find_request_id_(find_request_id_counter_++),
      last_search_case_sensitive_(false),
      last_search_result_() {
}

FindTabHelper::~FindTabHelper() {
}

void FindTabHelper::StartFinding(string16 search_string,
                                 bool forward_direction,
                                 bool case_sensitive) {
  // If search_string is empty, it means FindNext was pressed with a keyboard
  // shortcut so unless we have something to search for we return early.
  if (search_string.empty() && find_text_.empty()) {
    Profile* profile =
        Profile::FromBrowserContext(tab_contents()->browser_context());
    string16 last_search_prepopulate_text =
        FindBarState::GetLastPrepopulateText(profile);

    // Try the last thing we searched for on this tab, then the last thing
    // searched for on any tab.
    if (!previous_find_text_.empty())
      search_string = previous_find_text_;
    else if (!last_search_prepopulate_text.empty())
      search_string = last_search_prepopulate_text;
    else
      return;
  }

  // Keep track of the previous search.
  previous_find_text_ = find_text_;

  // This is a FindNext operation if we are searching for the same text again,
  // or if the passed in search text is empty (FindNext keyboard shortcut). The
  // exception to this is if the Find was aborted (then we don't want FindNext
  // because the highlighting has been cleared and we need it to reappear). We
  // therefore treat FindNext after an aborted Find operation as a full fledged
  // Find.
  bool find_next = (find_text_ == search_string || search_string.empty()) &&
                   (last_search_case_sensitive_ == case_sensitive) &&
                   !find_op_aborted_;
  if (!find_next)
    current_find_request_id_ = find_request_id_counter_++;

  if (!search_string.empty())
    find_text_ = search_string;
  last_search_case_sensitive_ = case_sensitive;

  find_op_aborted_ = false;

  // Keep track of what the last search was across the tabs.
  Profile* profile =
      Profile::FromBrowserContext(tab_contents()->browser_context());
  FindBarState* find_bar_state = profile->GetFindBarState();
  find_bar_state->set_last_prepopulate_text(find_text_);

  WebFindOptions options;
  options.forward = forward_direction;
  options.matchCase = case_sensitive;
  options.findNext = find_next;
  tab_contents()->render_view_host()->Find(current_find_request_id_,
                                           find_text_, options);
}

void FindTabHelper::StopFinding(
    FindBarController::SelectionAction selection_action) {
  if (selection_action == FindBarController::kClearSelection) {
    // kClearSelection means the find string has been cleared by the user, but
    // the UI has not been dismissed. In that case we want to clear the
    // previously remembered search (http://crbug.com/42639).
    previous_find_text_ = string16();
  } else {
    find_ui_active_ = false;
    if (!find_text_.empty())
      previous_find_text_ = find_text_;
  }
  find_text_.clear();
  find_op_aborted_ = true;
  last_search_result_ = FindNotificationDetails();

  ViewMsg_StopFinding_Params params;
  switch (selection_action) {
    case FindBarController::kClearSelection:
      params.action = ViewMsg_StopFinding_Params::kClearSelection;
      break;
    case FindBarController::kKeepSelection:
      params.action = ViewMsg_StopFinding_Params::kKeepSelection;
      break;
    case FindBarController::kActivateSelection:
      params.action = ViewMsg_StopFinding_Params::kActivateSelection;
      break;
    default:
      NOTREACHED();
      params.action = ViewMsg_StopFinding_Params::kKeepSelection;
  }
  tab_contents()->render_view_host()->StopFinding(params);
}

void FindTabHelper::HandleFindReply(int request_id,
                                    int number_of_matches,
                                    const gfx::Rect& selection_rect,
                                    int active_match_ordinal,
                                    bool final_update) {
  // Ignore responses for requests that have been aborted.
  // Ignore responses for requests other than the one we have most recently
  // issued. That way we won't act on stale results when the user has
  // already typed in another query.
  if (!find_op_aborted_ && request_id == current_find_request_id_) {
    if (number_of_matches == -1)
      number_of_matches = last_search_result_.number_of_matches();
    if (active_match_ordinal == -1)
      active_match_ordinal = last_search_result_.active_match_ordinal();

    gfx::Rect selection = selection_rect;
    if (selection.IsEmpty())
      selection = last_search_result_.selection_rect();

    // Notify the UI, automation and any other observers that a find result was
    // found.
    last_search_result_ = FindNotificationDetails(
        request_id, number_of_matches, selection, active_match_ordinal,
        final_update);
    NotificationService::current()->Notify(
        chrome::NOTIFICATION_FIND_RESULT_AVAILABLE,
        Source<TabContents>(tab_contents()),
        Details<FindNotificationDetails>(&last_search_result_));
  }
}
