// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/menu_gtk.h"

#include <map>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/gtk/gtk_custom_menu.h"
#include "chrome/browser/ui/gtk/gtk_custom_menu_item.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/accelerator_gtk.h"
#include "ui/base/models/button_menu_item_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/gtk_util.h"
#include "webkit/glue/window_open_disposition.h"

bool MenuGtk::block_activation_ = false;

namespace {

// Sets the ID of a menu item.
void SetMenuItemID(GtkWidget* menu_item, int menu_id) {
  DCHECK_GE(menu_id, 0);

  // Add 1 to the menu_id to avoid setting zero (null) to "menu-id".
  g_object_set_data(G_OBJECT(menu_item), "menu-id",
                    GINT_TO_POINTER(menu_id + 1));
}

// Gets the ID of a menu item.
// Returns true if the menu item has an ID.
bool GetMenuItemID(GtkWidget* menu_item, int* menu_id) {
  gpointer id_ptr = g_object_get_data(G_OBJECT(menu_item), "menu-id");
  if (id_ptr != NULL) {
    *menu_id = GPOINTER_TO_INT(id_ptr) - 1;
    return true;
  }

  return false;
}

ui::MenuModel* ModelForMenuItem(GtkMenuItem* menu_item) {
  return reinterpret_cast<ui::MenuModel*>(
      g_object_get_data(G_OBJECT(menu_item), "model"));
}

void SetupButtonShowHandler(GtkWidget* button,
                            ui::ButtonMenuItemModel* model,
                            int index) {
  g_object_set_data(G_OBJECT(button), "button-model",
                    model);
  g_object_set_data(G_OBJECT(button), "button-model-id",
                    GINT_TO_POINTER(index));
}

void OnSubmenuShowButtonImage(GtkWidget* widget, GtkButton* button) {
  MenuGtk::Delegate* delegate = reinterpret_cast<MenuGtk::Delegate*>(
      g_object_get_data(G_OBJECT(button), "menu-gtk-delegate"));
  int icon_idr = GPOINTER_TO_INT(g_object_get_data(
      G_OBJECT(button), "button-image-idr"));

  GtkIconSet* icon_set = delegate->GetIconSetForId(icon_idr);
  if (icon_set) {
    gtk_button_set_image(
        button, gtk_image_new_from_icon_set(icon_set,
                                            GTK_ICON_SIZE_MENU));
  }
}

void SetupImageIcon(GtkWidget* button,
                    GtkWidget* menu,
                    int icon_idr,
                    MenuGtk::Delegate* menu_gtk_delegate) {
  g_object_set_data(G_OBJECT(button), "button-image-idr",
                    GINT_TO_POINTER(icon_idr));
  g_object_set_data(G_OBJECT(button), "menu-gtk-delegate",
                    menu_gtk_delegate);

  g_signal_connect(menu, "show", G_CALLBACK(OnSubmenuShowButtonImage), button);
}

// Popup menus may get squished if they open up too close to the bottom of the
// screen. This function takes the size of the screen, the size of the menu,
// an optional widget, the Y position of the mouse click, and adjusts the popup
// menu's Y position to make it fit if it's possible to do so.
// Returns the new Y position of the popup menu.
int CalculateMenuYPosition(const GdkRectangle* screen_rect,
                           const GtkRequisition* menu_req,
                           const GtkWidget* widget, const int y) {
  CHECK(screen_rect);
  CHECK(menu_req);
  // If the menu would run off the bottom of the screen, and there is enough
  // screen space upwards to accommodate the menu, then pop upwards. If there
  // is a widget, then also move the anchor point to the top of the widget
  // rather than the bottom.
  const int screen_top = screen_rect->y;
  const int screen_bottom = screen_rect->y + screen_rect->height;
  const int menu_bottom = y + menu_req->height;
  int alternate_y = y - menu_req->height;
  if (widget)
    alternate_y -= widget->allocation.height;
  if (menu_bottom >= screen_bottom && alternate_y >= screen_top)
    return alternate_y;
  return y;
}

}  // namespace

GtkWidget* MenuGtk::Delegate::GetDefaultImageForCommandId(int command_id) {
  const char* stock;
  switch (command_id) {
    case IDC_NEW_TAB:
    case IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB:
    case IDC_CONTENT_CONTEXT_OPENLINKNEWTAB:
    case IDC_CONTENT_CONTEXT_OPENAVNEWTAB:
      stock = GTK_STOCK_NEW;
      break;

    case IDC_CLOSE_TAB:
      stock = GTK_STOCK_CLOSE;
      break;

    case IDC_CONTENT_CONTEXT_SAVEIMAGEAS:
    case IDC_CONTENT_CONTEXT_SAVEAVAS:
    case IDC_CONTENT_CONTEXT_SAVELINKAS:
      stock = GTK_STOCK_SAVE_AS;
      break;

    case IDC_SAVE_PAGE:
      stock = GTK_STOCK_SAVE;
      break;

    case IDC_COPY:
    case IDC_COPY_URL:
    case IDC_CONTENT_CONTEXT_COPYIMAGELOCATION:
    case IDC_CONTENT_CONTEXT_COPYLINKLOCATION:
    case IDC_CONTENT_CONTEXT_COPYAVLOCATION:
    case IDC_CONTENT_CONTEXT_COPYEMAILADDRESS:
    case IDC_CONTENT_CONTEXT_COPY:
      stock = GTK_STOCK_COPY;
      break;

    case IDC_CUT:
    case IDC_CONTENT_CONTEXT_CUT:
      stock = GTK_STOCK_CUT;
      break;

    case IDC_PASTE:
    case IDC_CONTENT_CONTEXT_PASTE:
      stock = GTK_STOCK_PASTE;
      break;

    case IDC_CONTENT_CONTEXT_DELETE:
    case IDC_BOOKMARK_BAR_REMOVE:
      stock = GTK_STOCK_DELETE;
      break;

    case IDC_CONTENT_CONTEXT_UNDO:
      stock = GTK_STOCK_UNDO;
      break;

    case IDC_CONTENT_CONTEXT_REDO:
      stock = GTK_STOCK_REDO;
      break;

    case IDC_SEARCH:
    case IDC_FIND:
    case IDC_CONTENT_CONTEXT_SEARCHWEBFOR:
      stock = GTK_STOCK_FIND;
      break;

    case IDC_CONTENT_CONTEXT_SELECTALL:
      stock = GTK_STOCK_SELECT_ALL;
      break;

    case IDC_CLEAR_BROWSING_DATA:
      stock = GTK_STOCK_CLEAR;
      break;

    case IDC_BACK:
      stock = GTK_STOCK_GO_BACK;
      break;

    case IDC_RELOAD:
      stock = GTK_STOCK_REFRESH;
      break;

    case IDC_FORWARD:
      stock = GTK_STOCK_GO_FORWARD;
      break;

    case IDC_PRINT:
      stock = GTK_STOCK_PRINT;
      break;

    case IDC_CONTENT_CONTEXT_VIEWPAGEINFO:
      stock = GTK_STOCK_INFO;
      break;

    case IDC_SPELLCHECK_MENU:
      stock = GTK_STOCK_SPELL_CHECK;
      break;

    case IDC_RESTORE_TAB:
      stock = GTK_STOCK_UNDELETE;
      break;

    case IDC_HOME:
      stock = GTK_STOCK_HOME;
      break;

    case IDC_STOP:
      stock = GTK_STOCK_STOP;
      break;

    case IDC_ABOUT:
      stock = GTK_STOCK_ABOUT;
      break;

    case IDC_EXIT:
      stock = GTK_STOCK_QUIT;
      break;

    case IDC_HELP_PAGE:
      stock = GTK_STOCK_HELP;
      break;

    case IDC_OPTIONS:
      stock = GTK_STOCK_PREFERENCES;
      break;

    case IDC_CONTENT_CONTEXT_GOTOURL:
      stock = GTK_STOCK_JUMP_TO;
      break;

    case IDC_DEV_TOOLS_INSPECT:
    case IDC_CONTENT_CONTEXT_INSPECTELEMENT:
      stock = GTK_STOCK_PROPERTIES;
      break;

    case IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK:
      stock = GTK_STOCK_ADD;
      break;

    case IDC_BOOKMARK_BAR_RENAME_FOLDER:
    case IDC_BOOKMARK_BAR_EDIT:
      stock = GTK_STOCK_EDIT;
      break;

    case IDC_BOOKMARK_BAR_NEW_FOLDER:
      stock = GTK_STOCK_DIRECTORY;
      break;

    case IDC_BOOKMARK_BAR_OPEN_ALL:
      stock = GTK_STOCK_OPEN;
      break;

    default:
      stock = NULL;
  }

  return stock ? gtk_image_new_from_stock(stock, GTK_ICON_SIZE_MENU) : NULL;
}

GtkWidget* MenuGtk::Delegate::GetImageForCommandId(int command_id) const {
  return GetDefaultImageForCommandId(command_id);
}

MenuGtk::MenuGtk(MenuGtk::Delegate* delegate,
                 ui::MenuModel* model)
    : delegate_(delegate),
      model_(model),
      dummy_accel_group_(gtk_accel_group_new()),
      menu_(gtk_custom_menu_new()),
      weak_factory_(this) {
  DCHECK(model);
  g_object_ref_sink(menu_);
  ConnectSignalHandlers();
  BuildMenuFromModel();
}

MenuGtk::~MenuGtk() {
  Cancel();

  gtk_widget_destroy(menu_);
  g_object_unref(menu_);

  g_object_unref(dummy_accel_group_);
}

void MenuGtk::ConnectSignalHandlers() {
  // We connect afterwards because OnMenuShow calls SetMenuItemInfo, which may
  // take a long time or even start a nested message loop.
  g_signal_connect(menu_, "show", G_CALLBACK(OnMenuShowThunk), this);
  g_signal_connect(menu_, "hide", G_CALLBACK(OnMenuHiddenThunk), this);
  GtkWidget *toplevel_window = gtk_widget_get_toplevel(menu_);
  signal_.Connect(toplevel_window, "focus-out-event",
                  G_CALLBACK(OnMenuFocusOutThunk), this);
}

GtkWidget* MenuGtk::AppendMenuItemWithLabel(int command_id,
                                            const std::string& label) {
  std::string converted_label = gfx::ConvertAcceleratorsFromWindowsStyle(label);
  GtkWidget* menu_item = BuildMenuItemWithLabel(label, command_id);
  return AppendMenuItem(command_id, menu_item);
}

GtkWidget* MenuGtk::AppendMenuItemWithIcon(int command_id,
                                           const std::string& label,
                                           const SkBitmap& icon) {
  std::string converted_label = gfx::ConvertAcceleratorsFromWindowsStyle(label);
  GtkWidget* menu_item = BuildMenuItemWithImage(converted_label, icon);
  return AppendMenuItem(command_id, menu_item);
}

GtkWidget* MenuGtk::AppendCheckMenuItemWithLabel(int command_id,
                                                 const std::string& label) {
  std::string converted_label = gfx::ConvertAcceleratorsFromWindowsStyle(label);
  GtkWidget* menu_item =
      gtk_check_menu_item_new_with_mnemonic(converted_label.c_str());
  return AppendMenuItem(command_id, menu_item);
}

GtkWidget* MenuGtk::AppendSeparator() {
  GtkWidget* menu_item = gtk_separator_menu_item_new();
  gtk_widget_show(menu_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_), menu_item);
  return menu_item;
}

GtkWidget* MenuGtk::AppendMenuItem(int command_id, GtkWidget* menu_item) {
  if (delegate_ && delegate_->AlwaysShowIconForCmd(command_id) &&
      GTK_IS_IMAGE_MENU_ITEM(menu_item))
    gtk_util::SetAlwaysShowImage(menu_item);

  return AppendMenuItemToMenu(command_id, NULL, menu_item, menu_, true);
}

GtkWidget* MenuGtk::AppendMenuItemToMenu(int index,
                                         ui::MenuModel* model,
                                         GtkWidget* menu_item,
                                         GtkWidget* menu,
                                         bool connect_to_activate) {
  SetMenuItemID(menu_item, index);

  // Native menu items do their own thing, so only selectively listen for the
  // activate signal.
  if (connect_to_activate) {
    g_signal_connect(menu_item, "activate",
                     G_CALLBACK(OnMenuItemActivatedThunk), this);
  }

  // AppendMenuItemToMenu is used both internally when we control menu creation
  // from a model (where the model can choose to hide certain menu items), and
  // with immediate commands which don't provide the option.
  if (model) {
    if (model->IsVisibleAt(index))
      gtk_widget_show(menu_item);
  } else {
    gtk_widget_show(menu_item);
  }
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
  return menu_item;
}

void MenuGtk::PopupForWidget(GtkWidget* widget, int button,
                             guint32 event_time) {
  gtk_menu_popup(GTK_MENU(menu_), NULL, NULL,
                 WidgetMenuPositionFunc,
                 widget,
                 button, event_time);
}

void MenuGtk::PopupAsContext(const gfx::Point& point, guint32 event_time) {
  // gtk_menu_popup doesn't like the "const" qualifier on point.
  gfx::Point nonconst_point(point);
  gtk_menu_popup(GTK_MENU(menu_), NULL, NULL,
                 PointMenuPositionFunc, &nonconst_point,
                 3, event_time);
}

void MenuGtk::PopupAsContextForStatusIcon(guint32 event_time, guint32 button,
                                          GtkStatusIcon* icon) {
  gtk_menu_popup(GTK_MENU(menu_), NULL, NULL, gtk_status_icon_position_menu,
                 icon, button, event_time);
}

void MenuGtk::PopupAsFromKeyEvent(GtkWidget* widget) {
  PopupForWidget(widget, 0, gtk_get_current_event_time());
  gtk_menu_shell_select_first(GTK_MENU_SHELL(menu_), FALSE);
}

void MenuGtk::Cancel() {
  gtk_menu_popdown(GTK_MENU(menu_));
}

void MenuGtk::UpdateMenu() {
  gtk_container_foreach(GTK_CONTAINER(menu_), SetMenuItemInfo, this);
}

GtkWidget* MenuGtk::BuildMenuItemWithImage(const std::string& label,
                                           GtkWidget* image) {
  GtkWidget* menu_item =
      gtk_image_menu_item_new_with_mnemonic(label.c_str());
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), image);
  return menu_item;
}

GtkWidget* MenuGtk::BuildMenuItemWithImage(const std::string& label,
                                           const SkBitmap& icon) {
  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&icon);
  GtkWidget* menu_item = BuildMenuItemWithImage(label,
      gtk_image_new_from_pixbuf(pixbuf));
  g_object_unref(pixbuf);
  return menu_item;
}

GtkWidget* MenuGtk::BuildMenuItemWithLabel(const std::string& label,
                                           int command_id) {
  GtkWidget* img =
      delegate_ ? delegate_->GetImageForCommandId(command_id) :
                  MenuGtk::Delegate::GetDefaultImageForCommandId(command_id);
  return img ? BuildMenuItemWithImage(label, img) :
               gtk_menu_item_new_with_mnemonic(label.c_str());
}

void MenuGtk::BuildMenuFromModel() {
  BuildSubmenuFromModel(model_, menu_);
}

void MenuGtk::BuildSubmenuFromModel(ui::MenuModel* model, GtkWidget* menu) {
  std::map<int, GtkWidget*> radio_groups;
  GtkWidget* menu_item = NULL;
  for (int i = 0; i < model->GetItemCount(); ++i) {
    SkBitmap icon;
    std::string label =
        gfx::ConvertAcceleratorsFromWindowsStyle(
            UTF16ToUTF8(model->GetLabelAt(i)));
    bool connect_to_activate = true;

    switch (model->GetTypeAt(i)) {
      case ui::MenuModel::TYPE_SEPARATOR:
        menu_item = gtk_separator_menu_item_new();
        break;

      case ui::MenuModel::TYPE_CHECK:
        menu_item = gtk_check_menu_item_new_with_mnemonic(label.c_str());
        break;

      case ui::MenuModel::TYPE_RADIO: {
        std::map<int, GtkWidget*>::iterator iter =
            radio_groups.find(model->GetGroupIdAt(i));

        if (iter == radio_groups.end()) {
          menu_item = gtk_radio_menu_item_new_with_mnemonic(
              NULL, label.c_str());
          radio_groups[model->GetGroupIdAt(i)] = menu_item;
        } else {
          menu_item = gtk_radio_menu_item_new_with_mnemonic_from_widget(
              GTK_RADIO_MENU_ITEM(iter->second), label.c_str());
        }
        break;
      }
      case ui::MenuModel::TYPE_BUTTON_ITEM: {
        ui::ButtonMenuItemModel* button_menu_item_model =
            model->GetButtonMenuItemAt(i);
        menu_item = BuildButtonMenuItem(button_menu_item_model, menu);
        connect_to_activate = false;
        break;
      }
      case ui::MenuModel::TYPE_SUBMENU:
      case ui::MenuModel::TYPE_COMMAND: {
        int command_id = model->GetCommandIdAt(i);
        if (model->GetIconAt(i, &icon))
          menu_item = BuildMenuItemWithImage(label, icon);
        else
          menu_item = BuildMenuItemWithLabel(label, command_id);
        if (delegate_ && delegate_->AlwaysShowIconForCmd(command_id) &&
            GTK_IS_IMAGE_MENU_ITEM(menu_item)) {
          gtk_util::SetAlwaysShowImage(menu_item);
        }
        break;
      }

      default:
        NOTREACHED();
    }

    if (model->GetTypeAt(i) == ui::MenuModel::TYPE_SUBMENU) {
      GtkWidget* submenu = gtk_menu_new();
      ui::MenuModel* submenu_model = model->GetSubmenuModelAt(i);
      g_object_set_data(G_OBJECT(menu_item), "submenu-model", submenu_model);
      // We will build the submenu on demand when activated.
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
    }

    ui::AcceleratorGtk accelerator;
    if (model->GetAcceleratorAt(i, &accelerator)) {
      gtk_widget_add_accelerator(menu_item,
                                 "activate",
                                 dummy_accel_group_,
                                 accelerator.GetGdkKeyCode(),
                                 accelerator.gdk_modifier_type(),
                                 GTK_ACCEL_VISIBLE);
    }

    g_object_set_data(G_OBJECT(menu_item), "model", model);
    AppendMenuItemToMenu(i, model, menu_item, menu, connect_to_activate);

    menu_item = NULL;
  }
}

GtkWidget* MenuGtk::BuildButtonMenuItem(ui::ButtonMenuItemModel* model,
                                        GtkWidget* menu) {
  GtkWidget* menu_item = gtk_custom_menu_item_new(
      gfx::RemoveWindowsStyleAccelerators(UTF16ToUTF8(model->label())).c_str());

  // Set up the callback to the model for when it is clicked.
  g_object_set_data(G_OBJECT(menu_item), "button-model", model);
  g_signal_connect(menu_item, "button-pushed",
                   G_CALLBACK(OnMenuButtonPressedThunk), this);
  g_signal_connect(menu_item, "try-button-pushed",
                   G_CALLBACK(OnMenuTryButtonPressedThunk), this);

  GtkSizeGroup* group = NULL;
  for (int i = 0; i < model->GetItemCount(); ++i) {
    GtkWidget* button = NULL;

    switch (model->GetTypeAt(i)) {
      case ui::ButtonMenuItemModel::TYPE_SPACE: {
        gtk_custom_menu_item_add_space(GTK_CUSTOM_MENU_ITEM(menu_item));
        break;
      }
      case ui::ButtonMenuItemModel::TYPE_BUTTON: {
        button = gtk_custom_menu_item_add_button(
            GTK_CUSTOM_MENU_ITEM(menu_item),
            model->GetCommandIdAt(i));

        int icon_idr;
        if (model->GetIconAt(i, &icon_idr)) {
          SetupImageIcon(button, menu, icon_idr, delegate_);
        } else {
          gtk_button_set_label(
              GTK_BUTTON(button),
              gfx::RemoveWindowsStyleAccelerators(
                  UTF16ToUTF8(model->GetLabelAt(i))).c_str());
        }

        SetupButtonShowHandler(button, model, i);
        break;
      }
      case ui::ButtonMenuItemModel::TYPE_BUTTON_LABEL: {
        button = gtk_custom_menu_item_add_button_label(
            GTK_CUSTOM_MENU_ITEM(menu_item),
            model->GetCommandIdAt(i));
        gtk_button_set_label(
            GTK_BUTTON(button),
            gfx::RemoveWindowsStyleAccelerators(
                UTF16ToUTF8(model->GetLabelAt(i))).c_str());
        SetupButtonShowHandler(button, model, i);
        break;
      }
    }

    if (button && model->PartOfGroup(i)) {
      if (!group)
        group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

      gtk_size_group_add_widget(group, button);
    }
  }

  if (group)
    g_object_unref(group);

  return menu_item;
}

void MenuGtk::OnMenuItemActivated(GtkWidget* menuitem) {
  if (block_activation_)
    return;

  ui::MenuModel* model = ModelForMenuItem(GTK_MENU_ITEM(menuitem));

  // We receive activation messages when highlighting a menu that has a
  // submenu. We build submenus on demand, and tear them down on hide, to
  // allow submenu models to be constructed and destroyed on demand.
  GtkWidget* submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menuitem));
  if (submenu) {
    int id;
    if (!GetMenuItemID(menuitem, &id))
      return;

    ui::MenuModel* submenu_model = static_cast<ui::MenuModel*>(
        g_object_get_data(G_OBJECT(menuitem), "submenu-model"));
    DCHECK(submenu_model);
    if (!submenu_model)
      return;

    // This might be just the temporary stub submenu, or it might be a full
    // submenu that we built but never destroyed. (This can happen if we briefly
    // hover over a submenu but never actually show it; we get the activation
    // event but no hide event.) We want to destroy it either way.
    gtk_widget_destroy(submenu);

    submenu_model->MenuWillShow();

    submenu = gtk_menu_new();
    BuildSubmenuFromModel(submenu_model, submenu);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

    // Update all the menu item info in the newly-generated menu.
    gtk_container_foreach(GTK_CONTAINER(submenu), SetMenuItemInfo, this);

    // Hook up the hide signal so the submenu knows when it is hidden.
    g_signal_connect(submenu, "hide", G_CALLBACK(OnSubmenuHidden),
                     implicit_cast<gpointer>(menuitem));
    return;
  }

  // The activate signal is sent to radio items as they get deselected;
  // ignore it in this case.
  if (GTK_IS_RADIO_MENU_ITEM(menuitem) &&
      !gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem))) {
    return;
  }

  int id;
  if (!GetMenuItemID(menuitem, &id))
    return;

  // The menu item can still be activated by hotkeys even if it is disabled.
  if (model->IsEnabledAt(id))
    ExecuteCommand(model, id);
}

void MenuGtk::OnMenuButtonPressed(GtkWidget* menu_item, int command_id) {
  ui::ButtonMenuItemModel* model =
      reinterpret_cast<ui::ButtonMenuItemModel*>(
          g_object_get_data(G_OBJECT(menu_item), "button-model"));
  if (model && model->IsCommandIdEnabled(command_id)) {
    if (delegate_)
      delegate_->CommandWillBeExecuted();

    model->ActivatedCommand(command_id);
  }
}

gboolean MenuGtk::OnMenuTryButtonPressed(GtkWidget* menu_item,
                                         int command_id) {
  gboolean pressed = FALSE;
  ui::ButtonMenuItemModel* model =
      reinterpret_cast<ui::ButtonMenuItemModel*>(
          g_object_get_data(G_OBJECT(menu_item), "button-model"));
  if (model &&
      model->IsCommandIdEnabled(command_id) &&
      !model->DoesCommandIdDismissMenu(command_id)) {
    if (delegate_)
      delegate_->CommandWillBeExecuted();

    model->ActivatedCommand(command_id);
    pressed = TRUE;
  }

  return pressed;
}

// static
void MenuGtk::WidgetMenuPositionFunc(GtkMenu* menu,
                                     int* x,
                                     int* y,
                                     gboolean* push_in,
                                     void* void_widget) {
  GtkWidget* widget = GTK_WIDGET(void_widget);
  GtkRequisition menu_req;

  gtk_widget_size_request(GTK_WIDGET(menu), &menu_req);

  gdk_window_get_origin(widget->window, x, y);
  GdkScreen *screen = gtk_widget_get_screen(widget);
  gint monitor = gdk_screen_get_monitor_at_point(screen, *x, *y);

  GdkRectangle screen_rect;
  gdk_screen_get_monitor_geometry(screen, monitor,
                                  &screen_rect);

  if (GTK_WIDGET_NO_WINDOW(widget)) {
    *x += widget->allocation.x;
    *y += widget->allocation.y;
  }
  *y += widget->allocation.height;

  bool start_align =
    !!g_object_get_data(G_OBJECT(widget), "left-align-popup");
  if (base::i18n::IsRTL())
    start_align = !start_align;

  if (!start_align)
    *x += widget->allocation.width - menu_req.width;

  *y = CalculateMenuYPosition(&screen_rect, &menu_req, widget, *y);

  *push_in = FALSE;
}

// static
void MenuGtk::PointMenuPositionFunc(GtkMenu* menu,
                                    int* x,
                                    int* y,
                                    gboolean* push_in,
                                    gpointer userdata) {
  *push_in = TRUE;

  gfx::Point* point = reinterpret_cast<gfx::Point*>(userdata);
  *x = point->x();
  *y = point->y();

  GtkRequisition menu_req;
  gtk_widget_size_request(GTK_WIDGET(menu), &menu_req);
  GdkScreen* screen;
  gdk_display_get_pointer(gdk_display_get_default(), &screen, NULL, NULL, NULL);
  gint monitor = gdk_screen_get_monitor_at_point(screen, *x, *y);

  GdkRectangle screen_rect;
  gdk_screen_get_monitor_geometry(screen, monitor, &screen_rect);

  *y = CalculateMenuYPosition(&screen_rect, &menu_req, NULL, *y);
}

void MenuGtk::ExecuteCommand(ui::MenuModel* model, int id) {
  if (delegate_)
    delegate_->CommandWillBeExecuted();

  GdkEvent* event = gtk_get_current_event();
  int event_flags = 0;

  if (event && event->type == GDK_BUTTON_RELEASE)
    event_flags = event_utils::EventFlagsFromGdkState(event->button.state);
  model->ActivatedAt(id, event_flags);

  if (event)
    gdk_event_free(event);
}

void MenuGtk::OnMenuShow(GtkWidget* widget) {
  model_->MenuWillShow();
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MenuGtk::UpdateMenu, weak_factory_.GetWeakPtr()));
}

void MenuGtk::OnMenuHidden(GtkWidget* widget) {
  if (delegate_)
    delegate_->StoppedShowing();
  model_->MenuClosed();
}

gboolean MenuGtk::OnMenuFocusOut(GtkWidget* widget, GdkEventFocus* event) {
  gtk_widget_hide(menu_);
  return TRUE;
}

// static
void MenuGtk::OnSubmenuHidden(GtkWidget* widget, gpointer userdata) {
  GtkWidget* menuitem = static_cast<GtkWidget*>(userdata);
  GtkWidget* submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menuitem));
  DCHECK_EQ(widget, submenu);
  if (widget != submenu)
    return;
  // This method is called before we've actually processed menu activations.
  // If we were to handle it right away, we might lose the activations. So,
  // we handle it a little later on. We use a weak reference to the menu item
  // to be sure we won't end up calling this on a destroyed object.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MenuGtk::OnSubmenuHiddenCallback, GObjectWeakRef(menuitem)));
}

// static
void MenuGtk::OnSubmenuHiddenCallback(const GObjectWeakRef& menuitem) {
  // Check that the weak reference is still there.
  if (!menuitem.get())
    return;

  GtkWidget* submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menuitem.get()));
  DCHECK(submenu);
  if (!submenu)
    return;
  ui::MenuModel* submenu_model = static_cast<ui::MenuModel*>(
      g_object_get_data(menuitem.get(), "submenu-model"));
  DCHECK(submenu_model);
  if (!submenu_model)
    return;

  // Destroy the dynamic submenu now so that its model can be safely destroyed
  // as well (e.g. for the bookmarks model); it will be rebuilt if necessary.
  gtk_widget_destroy(submenu);
  submenu = gtk_menu_new();
  // We don't need to do any further setup here. This temporary submenu
  // will be destroyed and rebuilt before being shown anyway.
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem.get()), submenu);

  // Notify the submenu model that the menu has been hidden.
  submenu_model->MenuClosed();
}

// static
void MenuGtk::SetButtonItemInfo(GtkWidget* button, gpointer userdata) {
  ui::ButtonMenuItemModel* model =
      reinterpret_cast<ui::ButtonMenuItemModel*>(
          g_object_get_data(G_OBJECT(button), "button-model"));
  int index = GPOINTER_TO_INT(g_object_get_data(
      G_OBJECT(button), "button-model-id"));

  if (model->IsItemDynamicAt(index)) {
    std::string label =
        gfx::ConvertAcceleratorsFromWindowsStyle(
            UTF16ToUTF8(model->GetLabelAt(index)));
    gtk_button_set_label(GTK_BUTTON(button), label.c_str());
  }

  gtk_widget_set_sensitive(GTK_WIDGET(button), model->IsEnabledAt(index));
}

// static
void MenuGtk::SetMenuItemInfo(GtkWidget* widget, gpointer userdata) {
  if (GTK_IS_SEPARATOR_MENU_ITEM(widget)) {
    // We need to explicitly handle this case because otherwise we'll ask the
    // menu delegate about something with an invalid id.
    return;
  }

  int id;
  if (!GetMenuItemID(widget, &id))
    return;

  ui::MenuModel* model = ModelForMenuItem(GTK_MENU_ITEM(widget));
  if (!model) {
    // If we're not providing the sub menu, then there's no model.  For
    // example, the IME submenu doesn't have a model.
    return;
  }

  if (GTK_IS_CHECK_MENU_ITEM(widget)) {
    GtkCheckMenuItem* item = GTK_CHECK_MENU_ITEM(widget);

    // gtk_check_menu_item_set_active() will send the activate signal. Touching
    // the underlying "active" property will also call the "activate" handler
    // for this menu item. So we prevent the "activate" handler from
    // being called while we set the checkbox.
    // Why not use one of the glib signal-blocking functions?  Because when we
    // toggle a radio button, it will deactivate one of the other radio buttons,
    // which we don't have a pointer to.
    // Wny not make this a member variable?  Because "menu" is a pointer to the
    // root of the MenuGtk and we want to disable *all* MenuGtks, including
    // submenus.
    block_activation_ = true;
    gtk_check_menu_item_set_active(item, model->IsItemCheckedAt(id));
    block_activation_ = false;
  }

  if (GTK_IS_CUSTOM_MENU_ITEM(widget)) {
    // Iterate across all the buttons to update their visible properties.
    gtk_custom_menu_item_foreach_button(GTK_CUSTOM_MENU_ITEM(widget),
                                        SetButtonItemInfo,
                                        userdata);
  }

  if (GTK_IS_MENU_ITEM(widget)) {
    gtk_widget_set_sensitive(widget, model->IsEnabledAt(id));

    if (model->IsVisibleAt(id)) {
      // Update the menu item label if it is dynamic.
      if (model->IsItemDynamicAt(id)) {
        std::string label =
            gfx::ConvertAcceleratorsFromWindowsStyle(
                UTF16ToUTF8(model->GetLabelAt(id)));

        gtk_menu_item_set_label(GTK_MENU_ITEM(widget), label.c_str());
        if (GTK_IS_IMAGE_MENU_ITEM(widget)) {
          SkBitmap icon;
          if (model->GetIconAt(id, &icon)) {
            GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&icon);
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(widget),
                                          gtk_image_new_from_pixbuf(pixbuf));
            g_object_unref(pixbuf);
          } else {
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(widget), NULL);
          }
        }
      }

      gtk_widget_show(widget);
    } else {
      gtk_widget_hide(widget);
    }

    GtkWidget* submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget));
    if (submenu) {
      gtk_container_foreach(GTK_CONTAINER(submenu), &SetMenuItemInfo,
                            userdata);
    }
  }
}
