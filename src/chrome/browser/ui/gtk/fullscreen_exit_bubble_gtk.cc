// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/fullscreen_exit_bubble_gtk.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/rounded_window.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/ui_strings.h"
#include "ui/base/gtk/gtk_floating_container.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const GdkColor kFrameColor = GDK_COLOR_RGB(0x63, 0x63, 0x63);
const int kMiddlePaddingPx = 30;
}  // namespace

FullscreenExitBubbleGtk::FullscreenExitBubbleGtk(
    GtkFloatingContainer* container,
    Browser* browser,
    const GURL& url,
    FullscreenExitBubbleType bubble_type)
    : FullscreenExitBubble(browser, url, bubble_type),
      container_(container) {
  InitWidgets();
}

FullscreenExitBubbleGtk::~FullscreenExitBubbleGtk() {
}

void FullscreenExitBubbleGtk::UpdateContent(
    const GURL& url,
    FullscreenExitBubbleType bubble_type) {
  if (bubble_type == FEB_TYPE_NONE) {
    NOTREACHED();
    bubble_type = FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION;
  }

  url_ = url;
  bubble_type_ = bubble_type;

  gtk_label_set_text(GTK_LABEL(message_label_),
                     UTF16ToUTF8(GetCurrentMessageText()).c_str());
  if (fullscreen_bubble::ShowButtonsForType(bubble_type)) {
    gtk_widget_hide(link_);
    gtk_widget_hide(instruction_label_);
    gtk_widget_show(allow_button_);
    gtk_button_set_label(GTK_BUTTON(deny_button_),
                         UTF16ToUTF8(GetCurrentDenyButtonText()).c_str());
    gtk_widget_show(deny_button_);
  } else {
    if (bubble_type == FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION) {
      gtk_widget_show(link_);
      gtk_widget_hide(instruction_label_);
    } else {
      gtk_widget_hide(link_);
      gtk_widget_show(instruction_label_);
    }
    gtk_widget_hide(allow_button_);
    gtk_widget_hide(deny_button_);
  }

  Show();
  StopWatchingMouse();
  StartWatchingMouseIfNecessary();
}

void FullscreenExitBubbleGtk::InitWidgets() {
  // The exit bubble is a gtk_chrome_link_button inside a gtk event box and gtk
  // alignment (these provide the background color).  This is then made rounded
  // and put into a slide widget.

  // The Windows code actually looks up the accelerator key in the accelerator
  // table and then converts the key to a string (in a switch statement). This
  // doesn't seem to be implemented for Gtk, so we just use F11 directly.
  std::string exit_text_utf8(l10n_util::GetStringFUTF8(
      IDS_EXIT_FULLSCREEN_MODE, l10n_util::GetStringUTF16(IDS_APP_F11_KEY)));

  hbox_ = gtk_hbox_new(false, ui::kControlSpacing);

  message_label_ = gtk_label_new(GetMessage(url_).c_str());
  gtk_box_pack_start(GTK_BOX(hbox_), message_label_, FALSE, FALSE, 0);

  allow_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_FULLSCREEN_ALLOW).c_str());
  gtk_widget_set_can_focus(allow_button_, FALSE);
  gtk_widget_set_no_show_all(allow_button_, FALSE);
  gtk_box_pack_start(GTK_BOX(hbox_), allow_button_, FALSE, FALSE, 0);

  deny_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_FULLSCREEN_DENY).c_str());
  gtk_widget_set_can_focus(deny_button_, FALSE);
  gtk_widget_set_no_show_all(deny_button_, FALSE);
  gtk_box_pack_start(GTK_BOX(hbox_), deny_button_, FALSE, FALSE, 0);

  link_ = gtk_chrome_link_button_new(exit_text_utf8.c_str());
  gtk_widget_set_can_focus(link_, FALSE);
  gtk_widget_set_no_show_all(link_, FALSE);
  gtk_chrome_link_button_set_use_gtk_theme(GTK_CHROME_LINK_BUTTON(link_),
                                           FALSE);
  gtk_box_pack_start(GTK_BOX(hbox_), link_, FALSE, FALSE, 0);

  instruction_label_ = gtk_label_new(UTF16ToUTF8(GetInstructionText()).c_str());
  gtk_widget_set_no_show_all(instruction_label_, FALSE);
  gtk_box_pack_start(GTK_BOX(hbox_), instruction_label_, FALSE, FALSE, 0);

  GtkWidget* bubble = gtk_util::CreateGtkBorderBin(
      hbox_, &ui::kGdkWhite,
      kPaddingPx, kPaddingPx, kPaddingPx, kPaddingPx);
  gtk_util::ActAsRoundedWindow(bubble, kFrameColor, 3,
      gtk_util::ROUNDED_ALL, gtk_util::BORDER_ALL);
  GtkWidget* alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 5, 0, 0, 0);
  gtk_container_add(GTK_CONTAINER(alignment), bubble);
  ui_container_.Own(alignment);

  slide_widget_.reset(new SlideAnimatorGtk(ui_container_.get(),
      SlideAnimatorGtk::DOWN, kSlideOutDurationMs, false, false, NULL));
  gtk_widget_set_name(widget(), "exit-fullscreen-bubble");
  gtk_widget_show_all(ui_container_.get());
  gtk_widget_show(widget());
  slide_widget_->OpenWithoutAnimation();

  gtk_floating_container_add_floating(GTK_FLOATING_CONTAINER(container_),
                                      widget());

  signals_.Connect(container_, "set-floating-position",
                   G_CALLBACK(OnSetFloatingPositionThunk), this);
  signals_.Connect(link_, "clicked", G_CALLBACK(OnLinkClickedThunk), this);
  signals_.Connect(allow_button_, "clicked",
                   G_CALLBACK(&OnAllowClickedThunk), this);
  signals_.Connect(deny_button_, "clicked",
                   G_CALLBACK(&OnDenyClickedThunk), this);

  UpdateContent(url_, bubble_type_);
}

std::string FullscreenExitBubbleGtk::GetMessage(const GURL& url) {
  if (url.is_empty()) {
    return l10n_util::GetStringUTF8(
        IDS_FULLSCREEN_USER_ENTERED_FULLSCREEN);
  }
  if (url.SchemeIsFile())
    return l10n_util::GetStringUTF8(IDS_FULLSCREEN_ENTERED_FULLSCREEN);
  return l10n_util::GetStringFUTF8(IDS_FULLSCREEN_SITE_ENTERED_FULLSCREEN,
      UTF8ToUTF16(url.host()));
}

gfx::Rect FullscreenExitBubbleGtk::GetPopupRect(
    bool ignore_animation_state) const {
  GtkRequisition bubble_size;
  if (ignore_animation_state) {
    gtk_widget_size_request(ui_container_.get(), &bubble_size);
  } else {
    gtk_widget_size_request(widget(), &bubble_size);
  }
  return gfx::Rect(bubble_size.width, bubble_size.height);
}

gfx::Point FullscreenExitBubbleGtk::GetCursorScreenPoint() {
  GdkDisplay* display = gtk_widget_get_display(widget());

  // Get cursor position.
  // TODO: this hits the X server, so we may want to consider decreasing
  // kPositionCheckHz if we detect that we're running remotely.
  int x, y;
  gdk_display_get_pointer(display, NULL, &x, &y, NULL);

  return gfx::Point(x, y);
}

bool FullscreenExitBubbleGtk::WindowContainsPoint(gfx::Point pos) {
  GtkWindow* window = GTK_WINDOW(
      gtk_widget_get_ancestor(widget(), GTK_TYPE_WINDOW));
  int width, height, x, y;
  gtk_window_get_size(window, &width, &height);
  gtk_window_get_position(window, &x, &y);
  return gfx::Rect(x, y, width, height).Contains(pos);
}

bool FullscreenExitBubbleGtk::IsWindowActive() {
  if (!widget()->parent)
    return false;
  GtkWindow* window = GTK_WINDOW(
      gtk_widget_get_ancestor(widget(), GTK_TYPE_WINDOW));
  return gtk_window_is_active(window);
}

void FullscreenExitBubbleGtk::Hide() {
  slide_widget_->Close();
}

void FullscreenExitBubbleGtk::Show() {
  slide_widget_->Open();
}

bool FullscreenExitBubbleGtk::IsAnimating() {
  return slide_widget_->IsAnimating();
}

void FullscreenExitBubbleGtk::StartWatchingMouseIfNecessary() {
  if (!fullscreen_bubble::ShowButtonsForType(bubble_type_))
    StartWatchingMouse();
}

void FullscreenExitBubbleGtk::OnSetFloatingPosition(
    GtkWidget* floating_container,
    GtkAllocation* allocation) {
  GtkRequisition bubble_size;
  gtk_widget_size_request(widget(), &bubble_size);

  // Position the bubble at the top center of the screen.
  GValue value = { 0, };
  g_value_init(&value, G_TYPE_INT);
  g_value_set_int(&value, (allocation->width - bubble_size.width) / 2);
  gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                   widget(), "x", &value);

  g_value_set_int(&value, 0);
  gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                   widget(), "y", &value);
  g_value_unset(&value);
}

void FullscreenExitBubbleGtk::OnLinkClicked(GtkWidget* link) {
  ToggleFullscreen();
}

void FullscreenExitBubbleGtk::OnAllowClicked(GtkWidget* button) {
  Accept();
  UpdateContent(url_, bubble_type_);
  StartWatchingMouse();
}
void FullscreenExitBubbleGtk::OnDenyClicked(GtkWidget* button) {
  Cancel();
}
