// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/avatar_menu_bubble_gtk.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu_model.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/avatar_menu_item_gtk.h"
#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/location_bar_view_gtk.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// The minimum width in pixels of the bubble.
const int kBubbleMinWidth = 175;

// The number of pixels of padding on the left of the 'New Profile' link at the
// bottom of the bubble.
const int kNewProfileLinkLeftPadding = 40;

}  // namespace

AvatarMenuBubbleGtk::AvatarMenuBubbleGtk(Browser* browser,
                                         GtkWidget* anchor,
                                         BubbleGtk::ArrowLocationGtk arrow,
                                         const gfx::Rect* rect)
    : contents_(NULL),
      theme_service_(GtkThemeService::GetFrom(browser->profile())),
      minimum_width_(kBubbleMinWidth) {
  avatar_menu_model_.reset(new AvatarMenuModel(
      &g_browser_process->profile_manager()->GetProfileInfoCache(),
      this, browser));

  InitContents();

  OnAvatarMenuModelChanged(avatar_menu_model_.get());

  bubble_ = BubbleGtk::Show(anchor,
                            rect,
                            contents_,
                            arrow,
                            true,  // |match_system_theme|
                            true,  // |grab_input|
                            theme_service_,
                            this);  // |delegate|
  g_signal_connect(contents_, "destroy",
                   G_CALLBACK(&OnDestroyThunk), this);
}

AvatarMenuBubbleGtk::~AvatarMenuBubbleGtk() {
  STLDeleteContainerPointers(items_.begin(), items_.end());
}

void AvatarMenuBubbleGtk::OnDestroy(GtkWidget* widget) {
  // We are self deleting, we have a destroy signal setup to catch when we
  // destroyed (via the BubbleGtk being destroyed), and delete ourself.
  delete this;
}

void AvatarMenuBubbleGtk::BubbleClosing(BubbleGtk* bubble,
                                        bool closed_by_escape) {
}

void AvatarMenuBubbleGtk::OnAvatarMenuModelChanged(
    AvatarMenuModel* avatar_menu_model) {
  STLDeleteContainerPointers(items_.begin(), items_.end());
  items_.clear();
  minimum_width_ = kBubbleMinWidth;

  InitContents();
}

void AvatarMenuBubbleGtk::OpenProfile(size_t profile_index) {
  avatar_menu_model_->SwitchToProfile(profile_index);
  bubble_->Close();
}

void AvatarMenuBubbleGtk::EditProfile(size_t profile_index) {
  avatar_menu_model_->EditProfile(profile_index);
  bubble_->Close();
}

void AvatarMenuBubbleGtk::OnSizeRequest(GtkWidget* widget,
                                        GtkRequisition* req) {
  // Always use the maximum width ever requested.
  if (req->width < minimum_width_)
    req->width = minimum_width_;
  else
    minimum_width_ = req->width;
}

void AvatarMenuBubbleGtk::OnNewProfileLinkClicked(GtkWidget* link) {
  avatar_menu_model_->AddNewProfile();
  bubble_->Close();
}

void AvatarMenuBubbleGtk::InitContents() {
  size_t profile_count = avatar_menu_model_->GetNumberOfItems();

  contents_ = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(contents_),
                                 ui::kContentAreaBorder);
  g_signal_connect(contents_, "size-request",
                   G_CALLBACK(OnSizeRequestThunk), this);

  GtkWidget* items_vbox = gtk_vbox_new(FALSE, ui::kContentAreaSpacing);

  for (size_t i = 0; i < profile_count; ++i) {
    AvatarMenuItemGtk* item = new AvatarMenuItemGtk(
        this, avatar_menu_model_->GetItemAt(i), i, theme_service_);

    items_.push_back(item);

    gtk_box_pack_start(GTK_BOX(items_vbox), item->widget(), TRUE, TRUE, 0);
  }

  gtk_box_pack_start(GTK_BOX(contents_), items_vbox, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(contents_), gtk_hseparator_new(), TRUE, TRUE, 0);

  // The new profile link.
  GtkWidget* new_profile_link = gtk_chrome_link_button_new(
      l10n_util::GetStringUTF8(IDS_PROFILES_CREATE_NEW_PROFILE_LINK).c_str());
  g_signal_connect(new_profile_link, "clicked",
                   G_CALLBACK(OnNewProfileLinkClickedThunk), this);

  GtkWidget* link_align = gtk_alignment_new(0, 0, 0, 0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(link_align),
                            0, 0, kNewProfileLinkLeftPadding, 0);
  gtk_container_add(GTK_CONTAINER(link_align), new_profile_link);

  gtk_box_pack_start(GTK_BOX(contents_), link_align, FALSE, FALSE, 0);
}
