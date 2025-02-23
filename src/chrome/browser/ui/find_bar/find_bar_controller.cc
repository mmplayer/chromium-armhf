// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/find_bar/find_bar_controller.h"

#include <algorithm>

#include "base/i18n/rtl.h"
#include "build/build_config.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "ui/gfx/rect.h"

// The minimum space between the FindInPage window and the search result.
static const int kMinFindWndDistanceFromSelection = 5;

FindBarController::FindBarController(FindBar* find_bar)
    : find_bar_(find_bar),
      tab_contents_(NULL),
      last_reported_matchcount_(0) {
}

FindBarController::~FindBarController() {
  DCHECK(!tab_contents_);
}

void FindBarController::Show() {
  FindTabHelper* find_tab_helper = tab_contents_->find_tab_helper();

  // Only show the animation if we're not already showing a find bar for the
  // selected TabContents.
  if (!find_tab_helper->find_ui_active()) {
    MaybeSetPrepopulateText();

    find_tab_helper->set_find_ui_active(true);
    find_bar_->Show(true);
  }
  find_bar_->SetFocusAndSelection();
}

void FindBarController::EndFindSession(SelectionAction action) {
  find_bar_->Hide(true);

  // |tab_contents_| can be NULL for a number of reasons, for example when the
  // tab is closing. We must guard against that case. See issue 8030.
  if (tab_contents_) {
    FindTabHelper* find_tab_helper = tab_contents_->find_tab_helper();

    // When we hide the window, we need to notify the renderer that we are done
    // for now, so that we can abort the scoping effort and clear all the
    // tickmarks and highlighting.
    find_tab_helper->StopFinding(action);

    if (action != kKeepSelection)
      find_bar_->ClearResults(find_tab_helper->find_result());

    // When we get dismissed we restore the focus to where it belongs.
    find_bar_->RestoreSavedFocus();
  }
}

void FindBarController::ChangeTabContents(TabContentsWrapper* contents) {
  if (tab_contents_) {
    registrar_.RemoveAll();
    find_bar_->StopAnimation();
  }

  tab_contents_ = contents;

  // Hide any visible find window from the previous tab if NULL |tab_contents|
  // is passed in or if the find UI is not active in the new tab.
  if (find_bar_->IsFindBarVisible() &&
      (!tab_contents_ || !tab_contents_->find_tab_helper()->find_ui_active())) {
    find_bar_->Hide(false);
  }

  if (!tab_contents_)
    return;

  registrar_.Add(this, chrome::NOTIFICATION_FIND_RESULT_AVAILABLE,
                 Source<TabContents>(tab_contents_->tab_contents()));
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 Source<NavigationController>(&tab_contents_->controller()));

  MaybeSetPrepopulateText();

  if (tab_contents_->find_tab_helper()->find_ui_active()) {
    // A tab with a visible find bar just got selected and we need to show the
    // find bar but without animation since it was already animated into its
    // visible state. We also want to reset the window location so that
    // we don't surprise the user by popping up to the left for no apparent
    // reason.
    find_bar_->Show(false);
  }

  UpdateFindBarForCurrentResult();
}

////////////////////////////////////////////////////////////////////////////////
// FindBarHost, NotificationObserver implementation:

void FindBarController::Observe(int type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  FindTabHelper* find_tab_helper = tab_contents_->find_tab_helper();
  if (type == chrome::NOTIFICATION_FIND_RESULT_AVAILABLE) {
    // Don't update for notifications from TabContentses other than the one we
    // are actively tracking.
    if (Source<TabContents>(source).ptr() == tab_contents_->tab_contents()) {
      UpdateFindBarForCurrentResult();
      if (find_tab_helper->find_result().final_update() &&
          find_tab_helper->find_result().number_of_matches() == 0) {
        const string16& last_search = find_tab_helper->previous_find_text();
        const string16& current_search = find_tab_helper->find_text();
        if (last_search.find(current_search) != 0)
          find_bar_->AudibleAlert();
      }
    }
  } else if (type == content::NOTIFICATION_NAV_ENTRY_COMMITTED) {
    NavigationController* source_controller =
        Source<NavigationController>(source).ptr();
    if (source_controller == &tab_contents_->controller()) {
      content::LoadCommittedDetails* commit_details =
          Details<content::LoadCommittedDetails>(details).ptr();
      content::PageTransition transition_type =
          commit_details->entry->transition_type();
      // We hide the FindInPage window when the user navigates away, except on
      // reload.
      if (find_bar_->IsFindBarVisible()) {
        if (content::PageTransitionStripQualifier(transition_type) !=
            content::PAGE_TRANSITION_RELOAD) {
          EndFindSession(kKeepSelection);
        } else {
          // On Reload we want to make sure FindNext is converted to a full Find
          // to make sure highlights for inactive matches are repainted.
          find_tab_helper->set_find_op_aborted(true);
        }
      }
    }
  }
}

// static
gfx::Rect FindBarController::GetLocationForFindbarView(
    gfx::Rect view_location,
    const gfx::Rect& dialog_bounds,
    const gfx::Rect& avoid_overlapping_rect) {
  if (base::i18n::IsRTL()) {
    int boundary = dialog_bounds.width() - view_location.width();
    view_location.set_x(std::min(view_location.x(), boundary));
  } else {
    view_location.set_x(std::max(view_location.x(), dialog_bounds.x()));
  }

  gfx::Rect new_pos = view_location;

  // If the selection rectangle intersects the current position on screen then
  // we try to move our dialog to the left (right for RTL) of the selection
  // rectangle.
  if (!avoid_overlapping_rect.IsEmpty() &&
      avoid_overlapping_rect.Intersects(new_pos)) {
    if (base::i18n::IsRTL()) {
      new_pos.set_x(avoid_overlapping_rect.x() +
                    avoid_overlapping_rect.width() +
                    (2 * kMinFindWndDistanceFromSelection));

      // If we moved it off-screen to the right, we won't move it at all.
      if (new_pos.x() + new_pos.width() > dialog_bounds.width())
        new_pos = view_location;  // Reset.
    } else {
      new_pos.set_x(avoid_overlapping_rect.x() - new_pos.width() -
        kMinFindWndDistanceFromSelection);

      // If we moved it off-screen to the left, we won't move it at all.
      if (new_pos.x() < 0)
        new_pos = view_location;  // Reset.
    }
  }

  return new_pos;
}

void FindBarController::UpdateFindBarForCurrentResult() {
  FindTabHelper* find_tab_helper = tab_contents_->find_tab_helper();
  const FindNotificationDetails& find_result = find_tab_helper->find_result();

  // Avoid bug 894389: When a new search starts (and finds something) it reports
  // an interim match count result of 1 before the scoping effort starts. This
  // is to provide feedback as early as possible that we will find something.
  // As you add letters to the search term, this creates a flashing effect when
  // we briefly show "1 of 1" matches because there is a slight delay until
  // the scoping effort starts updating the match count. We avoid this flash by
  // ignoring interim results of 1 if we already have a positive number.
  if (find_result.number_of_matches() > -1) {
    if (last_reported_matchcount_ > 0 &&
        find_result.number_of_matches() == 1 &&
        !find_result.final_update())
      return;  // Don't let interim result override match count.
    last_reported_matchcount_ = find_result.number_of_matches();
  }

  find_bar_->UpdateUIForFindResult(find_result, find_tab_helper->find_text());
}

void FindBarController::MaybeSetPrepopulateText() {
#if !defined(OS_MACOSX)
  // Find out what we should show in the find text box. Usually, this will be
  // the last search in this tab, but if no search has been issued in this tab
  // we use the last search string (from any tab).
  FindTabHelper* find_tab_helper = tab_contents_->find_tab_helper();
  string16 find_string = find_tab_helper->find_text();
  if (find_string.empty())
    find_string = find_tab_helper->previous_find_text();
  if (find_string.empty()) {
    find_string =
        FindBarState::GetLastPrepopulateText(tab_contents_->profile());
  }

  // Update the find bar with existing results and search text, regardless of
  // whether or not the find bar is visible, so that if it's subsequently
  // shown it is showing the right state for this tab. We update the find text
  // _first_ since the FindBarView checks its emptiness to see if it should
  // clear the result count display when there's nothing in the box.
  find_bar_->SetFindText(find_string);
#else
  // Having a per-tab find_string is not compatible with OS X's find pasteboard,
  // so we always have the same find text in all find bars. This is done through
  // the find pasteboard mechanism, so don't set the text here.
#endif
}
