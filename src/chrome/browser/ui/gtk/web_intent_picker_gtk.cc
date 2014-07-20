// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/web_intent_picker_gtk.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/location_bar_view_gtk.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_types.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "grit/ui_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/image.h"

namespace {

// The width in pixels of the area between the icon on the left and the close
// button on the right.
const int kMainContentWidth = 300;

// The length in pixels of the label at the bottom of the picker. Text longer
// than this width will wrap.
const int kWebStoreLabelLength = 300;

// The pixel size of the label at the bottom of the picker.
const int kWebStoreLabelPixelSize = 11;

// How much to scale the the buttons up from their favicon size.
const int kServiceButtonScale = 3;

GtkThemeService *GetThemeService(TabContentsWrapper* wrapper) {
  return GtkThemeService::GetFrom(wrapper->profile());
}

// Set the image of |button| to |pixbuf|, and scale the button up by
// |kServiceButtonScale|.
void SetServiceButtonImage(GtkWidget* button, GdkPixbuf* pixbuf) {
  int width = gdk_pixbuf_get_width(pixbuf);
  int height = gdk_pixbuf_get_height(pixbuf);
  gtk_button_set_image(GTK_BUTTON(button),
                       gtk_image_new_from_pixbuf(pixbuf));
  gtk_widget_set_size_request(button,
                              width * kServiceButtonScale,
                              height * kServiceButtonScale);
}

} // namespace

// static
WebIntentPicker* WebIntentPicker::Create(Browser* browser,
                                         TabContentsWrapper* wrapper,
                                         WebIntentPickerDelegate* delegate) {
  return new WebIntentPickerGtk(browser, wrapper, delegate);
}

WebIntentPickerGtk::WebIntentPickerGtk(Browser* browser,
                                       TabContentsWrapper* wrapper,
                                       WebIntentPickerDelegate* delegate)
    : wrapper_(wrapper),
      delegate_(delegate),
      contents_(NULL),
      button_hbox_(NULL),
      bubble_(NULL) {
  DCHECK(delegate_ != NULL);
  BrowserWindowGtk* browser_window =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(
          browser->window()->GetNativeHandle());

  GtkWidget* anchor = browser_window->
      GetToolbar()->GetLocationBarView()->location_icon_widget();

  InitContents();

  BubbleGtk::ArrowLocationGtk arrow_location = base::i18n::IsRTL() ?
      BubbleGtk::ARROW_LOCATION_TOP_RIGHT :
      BubbleGtk::ARROW_LOCATION_TOP_LEFT;
  bubble_ = BubbleGtk::Show(anchor,
                            NULL,  // |rect|
                            contents_,
                            arrow_location,
                            true,  // |match_system_theme|
                            true,  // |grab_input|
                            GetThemeService(wrapper),
                            this);  // |delegate|
}

WebIntentPickerGtk::~WebIntentPickerGtk() {
  DCHECK(bubble_ == NULL) << "Should have closed window before destroying.";
}

void WebIntentPickerGtk::SetServiceURLs(const std::vector<GURL>& urls) {
  for (size_t i = 0; i < urls.size(); ++i) {
    GtkWidget* button = gtk_button_new();
    gtk_widget_set_tooltip_text(button, urls[i].spec().c_str());
    gtk_box_pack_start(GTK_BOX(button_hbox_), button, FALSE, FALSE, 0);
    g_signal_connect(button,
                     "clicked",
                     G_CALLBACK(OnServiceButtonClickThunk),
                     this);
  }

  // Add the '+' button, to use the Chrome Web Store.
  GtkWidget* plus_button = gtk_button_new();
  gtk_widget_set_tooltip_text(
      plus_button,
      l10n_util::GetStringUTF8(IDS_FIND_MORE_INTENT_HANDLER_TOOLTIP).c_str());
  gtk_box_pack_start(GTK_BOX(button_hbox_), plus_button, FALSE, FALSE, 0);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  GdkPixbuf* icon_pixbuf = rb.GetNativeImageNamed(IDR_SIDETABS_NEW_TAB);
  SetServiceButtonImage(plus_button, icon_pixbuf);

  gtk_widget_show_all(button_hbox_);
}

void WebIntentPickerGtk::SetServiceIcon(size_t index, const SkBitmap& icon) {
  if (icon.empty())
    return;

  GtkWidget* button = GetServiceButton(index);

  GdkPixbuf* icon_pixbuf = gfx::GdkPixbufFromSkBitmap(&icon);
  SetServiceButtonImage(button, icon_pixbuf);
  g_object_unref(icon_pixbuf);
}

void WebIntentPickerGtk::SetDefaultServiceIcon(size_t index) {
  GtkWidget* button = GetServiceButton(index);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  GdkPixbuf* icon_pixbuf = rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON);
  SetServiceButtonImage(button, icon_pixbuf);
}

void WebIntentPickerGtk::Close() {
  bubble_->Close();
  bubble_ = NULL;
}

void WebIntentPickerGtk::BubbleClosing(BubbleGtk* bubble,
                                       bool closed_by_escape) {
  delegate_->OnCancelled();
}

void WebIntentPickerGtk::OnCloseButtonClick(GtkWidget* button) {
  delegate_->OnCancelled();
}

void WebIntentPickerGtk::OnServiceButtonClick(GtkWidget* button) {
  GList* button_list = gtk_container_get_children(GTK_CONTAINER(button_hbox_));
  gint index = g_list_index(button_list, button);
  DCHECK(index != -1);

  delegate_->OnServiceChosen(static_cast<size_t>(index));
}

void WebIntentPickerGtk::InitContents() {
  GtkThemeService* theme_service = GetThemeService(wrapper_);

  contents_ = gtk_hbox_new(FALSE, ui::kContentAreaSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(contents_),
                                 ui::kContentAreaBorder);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  GdkPixbuf* image_pixbuf = rb.GetNativeImageNamed(IDR_PAGEINFO_INFO);
  GtkWidget* image = gtk_image_new_from_pixbuf(image_pixbuf);
  gtk_box_pack_start(GTK_BOX(contents_), image, FALSE, FALSE, 0);
  gtk_misc_set_alignment(GTK_MISC(image), 0, 0);

  GtkWidget* vbox = gtk_vbox_new(FALSE, ui::kContentAreaSpacing);

  GtkWidget* label = theme_service->BuildLabel(
      l10n_util::GetStringUTF8(IDS_CHOOSE_INTENT_HANDLER_MESSAGE).c_str(),
      ui::kGdkBlack);
  gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

  gtk_widget_set_size_request(vbox, kMainContentWidth, -1);

  button_hbox_ = gtk_hbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(vbox), button_hbox_, TRUE, TRUE, 0);

  GtkWidget* cws_label = theme_service->BuildLabel(
      l10n_util::GetStringUTF8(IDS_FIND_MORE_INTENT_HANDLER_MESSAGE).c_str(),
      ui::kGdkBlack);
  gtk_box_pack_start(GTK_BOX(vbox), cws_label, TRUE, TRUE, 0);
  gtk_misc_set_alignment(GTK_MISC(cws_label), 0, 0);
  gtk_util::SetLabelWidth(cws_label, kWebStoreLabelLength);
  gtk_util::ForceFontSizePixels(cws_label, kWebStoreLabelPixelSize);

  gtk_box_pack_start(GTK_BOX(contents_), vbox, TRUE, TRUE, 0);

  close_button_.reset(
      CustomDrawButton::CloseButton(GetThemeService(wrapper_)));
  g_signal_connect(close_button_->widget(),
                   "clicked",
                   G_CALLBACK(OnCloseButtonClickThunk),
                   this);
  gtk_widget_set_can_focus(close_button_->widget(), FALSE);

  GtkWidget* close_vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(close_vbox), close_button_->widget(), FALSE, FALSE,
                     0);
  gtk_box_pack_end(GTK_BOX(contents_), close_vbox, FALSE, FALSE, 0);
}

GtkWidget* WebIntentPickerGtk::GetServiceButton(size_t index) {
  GList* button_list = gtk_container_get_children(GTK_CONTAINER(button_hbox_));
  GtkWidget* button = GTK_WIDGET(g_list_nth_data(button_list, index));
  DCHECK(button != NULL);

  return button;
}
