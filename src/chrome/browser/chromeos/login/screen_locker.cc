// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_locker.h"

#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <string>
#include <vector>
// Evil hack to undo X11 evil #define. See crosbug.com/
#undef Status

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/cros/screen_lock_library.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/login_performer.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/login/screen_lock_view.h"
#include "chrome/browser/chromeos/login/shutdown_button.h"
#include "chrome/browser/chromeos/system_key_event_listener.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/browser/user_metrics.h"
#include "content/common/notification_service.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/screen.h"
#include "views/widget/native_widget_gtk.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chromeos/wm_ipc.h"
#endif

namespace {

// The maximum duration for which locker should try to grab the keyboard and
// mouse and its interval for regrabbing on failure.
const int kMaxGrabFailureSec = 30;
const int64 kRetryGrabIntervalMs = 500;

// Maximum number of times we'll try to grab the keyboard and mouse before
// giving up.  If we hit the limit, Chrome exits and the session is terminated.
const int kMaxGrabFailures = kMaxGrabFailureSec * 1000 / kRetryGrabIntervalMs;

// A idle time to show the screen saver in seconds.
const int kScreenSaverIdleTimeout = 15;

// Observer to start ScreenLocker when the screen lock
class ScreenLockObserver : public chromeos::ScreenLockLibrary::Observer,
                           public NotificationObserver {
 public:
  ScreenLockObserver() {
    registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                   NotificationService::AllSources());
  }

  // NotificationObserver overrides:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE {
    if (type == chrome::NOTIFICATION_LOGIN_USER_CHANGED) {
      // Register Screen Lock after login screen to make sure
      // we don't show the screen lock on top of the login screen by accident.
      if (chromeos::CrosLibrary::Get()->EnsureLoaded())
        chromeos::CrosLibrary::Get()->GetScreenLockLibrary()->AddObserver(this);
    }
  }

  virtual void LockScreen(chromeos::ScreenLockLibrary* obj) OVERRIDE {
    VLOG(1) << "In: ScreenLockObserver::LockScreen";
    SetupInputMethodsForScreenLocker();
    chromeos::ScreenLocker::Show();
  }

  virtual void UnlockScreen(chromeos::ScreenLockLibrary* obj) OVERRIDE {
    RestoreInputMethods();
    chromeos::ScreenLocker::Hide();
  }

  virtual void UnlockScreenFailed(chromeos::ScreenLockLibrary* obj) OVERRIDE {
    chromeos::ScreenLocker::UnlockScreenFailed();
  }

 private:
  // Temporarily deactivates all input methods (e.g. Chinese, Japanese, Arabic)
  // since they are not necessary to input a login password. Users are still
  // able to use/switch active keyboard layouts (e.g. US qwerty, US dvorak,
  // French).
  void SetupInputMethodsForScreenLocker() {
    if (// The LockScreen function is also called when the OS is suspended, and
        // in that case |saved_active_input_method_list_| might be non-empty.
        saved_active_input_method_list_.empty()) {
      chromeos::input_method::InputMethodManager* manager =
          chromeos::input_method::InputMethodManager::GetInstance();

      saved_previous_input_method_id_ = manager->previous_input_method().id();
      saved_current_input_method_id_ = manager->current_input_method().id();
      scoped_ptr<chromeos::input_method::InputMethodDescriptors>
          active_input_method_list(manager->GetActiveInputMethods());

      const std::string hardware_keyboard_id =
          manager->GetInputMethodUtil()->GetHardwareInputMethodId();
      // We'll add the hardware keyboard if it's not included in
      // |active_input_method_list| so that the user can always use the hardware
      // keyboard on the screen locker.
      bool should_add_hardware_keyboard = true;

      chromeos::input_method::ImeConfigValue value;
      value.type = chromeos::input_method::ImeConfigValue::kValueTypeStringList;
      for (size_t i = 0; i < active_input_method_list->size(); ++i) {
        const std::string& input_method_id =
            active_input_method_list->at(i).id();
        saved_active_input_method_list_.push_back(input_method_id);
        // Skip if it's not a keyboard layout.
        if (!chromeos::input_method::InputMethodUtil::IsKeyboardLayout(
                input_method_id))
          continue;
        value.string_list_value.push_back(input_method_id);
        if (input_method_id == hardware_keyboard_id) {
          should_add_hardware_keyboard = false;
        }
      }
      if (should_add_hardware_keyboard) {
        value.string_list_value.push_back(hardware_keyboard_id);
      }
      // We don't want to shut down the IME, even if the hardware layout is the
      // only IME left.
      manager->SetEnableAutoImeShutdown(false);
      manager->SetImeConfig(
          chromeos::language_prefs::kGeneralSectionName,
          chromeos::language_prefs::kPreloadEnginesConfigName,
          value);
    }
  }

  void RestoreInputMethods() {
    if (!saved_active_input_method_list_.empty()) {
      chromeos::input_method::InputMethodManager* manager =
          chromeos::input_method::InputMethodManager::GetInstance();

      chromeos::input_method::ImeConfigValue value;
      value.type = chromeos::input_method::ImeConfigValue::kValueTypeStringList;
      value.string_list_value = saved_active_input_method_list_;
      manager->SetEnableAutoImeShutdown(true);
      manager->SetImeConfig(
          chromeos::language_prefs::kGeneralSectionName,
          chromeos::language_prefs::kPreloadEnginesConfigName,
          value);
      // Send previous input method id first so Ctrl+space would work fine.
      if (!saved_previous_input_method_id_.empty())
        manager->ChangeInputMethod(saved_previous_input_method_id_);
      if (!saved_current_input_method_id_.empty())
        manager->ChangeInputMethod(saved_current_input_method_id_);

      saved_previous_input_method_id_.clear();
      saved_current_input_method_id_.clear();
      saved_active_input_method_list_.clear();
    }
  }

  NotificationRegistrar registrar_;
  std::string saved_previous_input_method_id_;
  std::string saved_current_input_method_id_;
  std::vector<std::string> saved_active_input_method_list_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockObserver);
};

static base::LazyInstance<ScreenLockObserver> g_screen_lock_observer(
    base::LINKER_INITIALIZED);

// A ScreenLock window that covers entire screen to keep the keyboard
// focus/events inside the grab widget.
class LockWindow : public views::NativeWidgetGtk {
 public:
  LockWindow()
      : views::NativeWidgetGtk(new views::Widget),
        toplevel_focus_widget_(NULL) {
    EnableDoubleBuffer(true);
  }

  // GTK propagates key events from parents to children.
  // Make sure LockWindow will never handle key events.
  virtual gboolean OnEventKey(GtkWidget* widget, GdkEventKey* event) OVERRIDE {
    // Don't handle key event in the lock window.
    return false;
  }

  virtual gboolean OnButtonPress(GtkWidget* widget,
                                 GdkEventButton* event) OVERRIDE {
    // Don't handle mouse event in the lock wnidow and
    // nor propagate to child.
    return true;
  }

  virtual void OnDestroy(GtkWidget* object) OVERRIDE {
    VLOG(1) << "OnDestroy: LockWindow destroyed";
    views::NativeWidgetGtk::OnDestroy(object);
  }

  virtual void ClearNativeFocus() OVERRIDE {
    DCHECK(toplevel_focus_widget_);
    gtk_widget_grab_focus(toplevel_focus_widget_);
  }

  // Sets the widget to move the focus to when clearning the native
  // widget's focus.
  void set_toplevel_focus_widget(GtkWidget* widget) {
    gtk_widget_set_can_focus(widget, TRUE);
    toplevel_focus_widget_ = widget;
  }

 private:
  // The widget we set focus to when clearning the focus on native
  // widget.  In screen locker, gdk input is grabbed in GrabWidget,
  // and resetting the focus by using gtk_window_set_focus seems to
  // confuse gtk and doesn't let focus move to native widget under
  // GrabWidget.
  GtkWidget* toplevel_focus_widget_;

  DISALLOW_COPY_AND_ASSIGN(LockWindow);
};

// GrabWidget's root view to layout the ScreenLockView at the center
// and the Shutdown button at the left top.
class GrabWidgetRootView
    : public views::View,
      public chromeos::ScreenLocker::ScreenLockViewContainer {
 public:
  explicit GrabWidgetRootView(chromeos::ScreenLockView* screen_lock_view)
      : screen_lock_view_(screen_lock_view),
        shutdown_button_(new chromeos::ShutdownButton()) {
    shutdown_button_->Init();
    AddChildView(screen_lock_view_);
    AddChildView(shutdown_button_);
  }

  // views::View implementation.
  virtual void Layout() OVERRIDE {
    gfx::Size size = screen_lock_view_->GetPreferredSize();
    gfx::Rect b = bounds();
    screen_lock_view_->SetBounds(
        b.width() - size.width(), b.height() - size.height(),
        size.width(), size.height());
    shutdown_button_->LayoutIn(this);
  }

  // ScreenLocker::ScreenLockViewContainer implementation:
  void SetScreenLockView(views::View* screen_lock_view) OVERRIDE {
    if (screen_lock_view_) {
      RemoveChildView(screen_lock_view_);
    }
    screen_lock_view_ =  screen_lock_view;
    if (screen_lock_view_) {
      AddChildViewAt(screen_lock_view_, 0);
    }
    Layout();
  }

 private:
  views::View* screen_lock_view_;

  chromeos::ShutdownButton* shutdown_button_;

  DISALLOW_COPY_AND_ASSIGN(GrabWidgetRootView);
};

// A child widget that grabs both keyboard and pointer input.
class GrabWidget : public views::NativeWidgetGtk {
 public:
  explicit GrabWidget(chromeos::ScreenLocker* screen_locker)
      : views::NativeWidgetGtk(new views::Widget),
        screen_locker_(screen_locker),
        ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
        grab_failure_count_(0),
        kbd_grab_status_(GDK_GRAB_INVALID_TIME),
        mouse_grab_status_(GDK_GRAB_INVALID_TIME),
        signout_link_(NULL),
        shutdown_(NULL) {
  }

  virtual void Show() OVERRIDE {
    views::NativeWidgetGtk::Show();
    signout_link_ =
        screen_locker_->GetViewByID(VIEW_ID_SCREEN_LOCKER_SIGNOUT_LINK);
    shutdown_ = screen_locker_->GetViewByID(VIEW_ID_SCREEN_LOCKER_SHUTDOWN);
    // These can be null in guest mode.
  }

  void ClearGtkGrab() {
    GtkWidget* current_grab_window;
    // Grab gtk input first so that the menu holding gtk grab will
    // close itself.
    gtk_grab_add(window_contents());

    // Make sure there is no gtk grab widget so that gtk simply propagates
    // an event.  This is necessary to allow message bubble and password
    // field, button to process events simultaneously. GTK
    // maintains grab widgets in a linked-list, so we need to remove
    // until it's empty.
    while ((current_grab_window = gtk_grab_get_current()) != NULL)
      gtk_grab_remove(current_grab_window);
  }

  virtual gboolean OnEventKey(GtkWidget* widget, GdkEventKey* event) OVERRIDE {
    views::KeyEvent key_event(reinterpret_cast<GdkEvent*>(event));
    // This is a hack to workaround the issue crosbug.com/10655 due to
    // the limitation that a focus manager cannot handle views in
    // TYPE_CHILD NativeWidgetGtk correctly.
    if (signout_link_ &&
        event->type == GDK_KEY_PRESS &&
        (event->keyval == GDK_Tab ||
         event->keyval == GDK_ISO_Left_Tab ||
         event->keyval == GDK_KP_Tab)) {
      DCHECK(shutdown_);
      bool reverse = event->state & GDK_SHIFT_MASK;
      if (reverse && signout_link_->HasFocus()) {
        shutdown_->RequestFocus();
        return true;
      }
      if (!reverse && shutdown_->HasFocus()) {
        signout_link_->RequestFocus();
        return true;
      }
    }
    return views::NativeWidgetGtk::OnEventKey(widget, event);
  }

  virtual gboolean OnButtonPress(GtkWidget* widget,
                                 GdkEventButton* event) OVERRIDE {
    NativeWidgetGtk::OnButtonPress(widget, event);
    // Never propagate event to parent.
    return true;
  }

  // Try to grab all inputs. It initiates another try if it fails to
  // grab and the retry count is within a limit, or fails with CHECK.
  void TryGrabAllInputs();

  // This method tries to steal pointer/keyboard grab from other
  // client by sending events that will hopefully close menus or windows
  // that have the grab.
  void TryUngrabOtherClients();

 private:
  virtual void HandleGtkGrabBroke() OVERRIDE {
    // Input should never be stolen from ScreenLocker once it's
    // grabbed.  If this happens, it's a bug and has to be fixed. We
    // let chrome crash to get a crash report and dump, and
    // SessionManager will terminate the session to logout.
    CHECK_NE(GDK_GRAB_SUCCESS, kbd_grab_status_);
    CHECK_NE(GDK_GRAB_SUCCESS, mouse_grab_status_);
  }

  // Define separate methods for each error code so that stack trace
  // will tell which error the grab failed with.
  void FailedWithGrabAlreadyGrabbed() {
    LOG(FATAL) << "Grab already grabbed";
  }
  void FailedWithGrabInvalidTime() {
    LOG(FATAL) << "Grab invalid time";
  }
  void FailedWithGrabNotViewable() {
    LOG(FATAL) << "Grab not viewable";
  }
  void FailedWithGrabFrozen() {
    LOG(FATAL) << "Grab frozen";
  }
  void FailedWithUnknownError() {
    LOG(FATAL) << "Grab uknown";
  }

  chromeos::ScreenLocker* screen_locker_;
  base::WeakPtrFactory<GrabWidget> weak_factory_;

  // The number times the widget tried to grab all focus.
  int grab_failure_count_;
  // Status of keyboard and mouse grab.
  GdkGrabStatus kbd_grab_status_;
  GdkGrabStatus mouse_grab_status_;

  views::View* signout_link_;
  views::View* shutdown_;

  DISALLOW_COPY_AND_ASSIGN(GrabWidget);
};

void GrabWidget::TryGrabAllInputs() {
  // Grab x server so that we can atomically grab and take
  // action when grab fails.
  gdk_x11_grab_server();
  if (kbd_grab_status_ != GDK_GRAB_SUCCESS) {
    kbd_grab_status_ = gdk_keyboard_grab(window_contents()->window, FALSE,
                                         GDK_CURRENT_TIME);
  }
  if (mouse_grab_status_ != GDK_GRAB_SUCCESS) {
    mouse_grab_status_ =
        gdk_pointer_grab(window_contents()->window,
                         FALSE,
                         static_cast<GdkEventMask>(
                             GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                             GDK_POINTER_MOTION_MASK),
                         NULL,
                         NULL,
                         GDK_CURRENT_TIME);
  }
  if ((kbd_grab_status_ != GDK_GRAB_SUCCESS ||
       mouse_grab_status_ != GDK_GRAB_SUCCESS) &&
      grab_failure_count_++ < kMaxGrabFailures) {
    LOG(WARNING) << "Failed to grab inputs. Trying again in "
                 << kRetryGrabIntervalMs << " ms: kbd="
                 << kbd_grab_status_ << ", mouse=" << mouse_grab_status_;
    TryUngrabOtherClients();
    gdk_x11_ungrab_server();
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&GrabWidget::TryGrabAllInputs, weak_factory_.GetWeakPtr()),
        kRetryGrabIntervalMs);
  } else {
    gdk_x11_ungrab_server();
    GdkGrabStatus status = kbd_grab_status_;
    if (status == GDK_GRAB_SUCCESS) {
      status = mouse_grab_status_;
    }
    switch (status) {
      case GDK_GRAB_SUCCESS:
        break;
      case GDK_GRAB_ALREADY_GRABBED:
        FailedWithGrabAlreadyGrabbed();
        break;
      case GDK_GRAB_INVALID_TIME:
        FailedWithGrabInvalidTime();
        break;
      case GDK_GRAB_NOT_VIEWABLE:
        FailedWithGrabNotViewable();
        break;
      case GDK_GRAB_FROZEN:
        FailedWithGrabFrozen();
        break;
      default:
        FailedWithUnknownError();
        break;
    }
    DVLOG(1) << "Grab Success";
    screen_locker_->OnGrabInputs();
  }
}

void GrabWidget::TryUngrabOtherClients() {
#if !defined(NDEBUG)
  {
    int event_base, error_base;
    int major, minor;
    // Make sure we have XTest extension.
    DCHECK(XTestQueryExtension(ui::GetXDisplay(),
                               &event_base, &error_base,
                               &major, &minor));
  }
#endif

  // The following code is an attempt to grab inputs by closing
  // supposedly opened menu. This happens when a plugin has a menu
  // opened.
  if (mouse_grab_status_ == GDK_GRAB_ALREADY_GRABBED ||
      mouse_grab_status_ == GDK_GRAB_FROZEN) {
    // Successfully grabbed the keyboard, but pointer is still
    // grabbed by other client. Another attempt to close supposedly
    // opened menu by emulating keypress at the left top corner.
    Display* display = ui::GetXDisplay();
    Window root, child;
    int root_x, root_y, win_x, winy;
    unsigned int mask;
    XQueryPointer(display,
                  ui::GetX11WindowFromGtkWidget(window_contents()),
                  &root, &child, &root_x, &root_y,
                  &win_x, &winy, &mask);
    XTestFakeMotionEvent(display, -1, -10000, -10000, CurrentTime);
    XTestFakeButtonEvent(display, 1, True, CurrentTime);
    XTestFakeButtonEvent(display, 1, False, CurrentTime);
    // Move the pointer back.
    XTestFakeMotionEvent(display, -1, root_x, root_y, CurrentTime);
    XFlush(display);
  } else if (kbd_grab_status_ == GDK_GRAB_ALREADY_GRABBED ||
             kbd_grab_status_ == GDK_GRAB_FROZEN) {
    // Successfully grabbed the pointer, but keyboard is still grabbed
    // by other client. Another attempt to close supposedly opened
    // menu by emulating escape key.  Such situation must be very
    // rare, but handling this just in case
    Display* display = ui::GetXDisplay();
    KeyCode escape = XKeysymToKeycode(display, XK_Escape);
    XTestFakeKeyEvent(display, escape, True, CurrentTime);
    XTestFakeKeyEvent(display, escape, False, CurrentTime);
    XFlush(display);
  }
}

// BackgroundView for ScreenLocker, which layouts a lock widget in
// addition to other background components.
class ScreenLockerBackgroundView
    : public chromeos::BackgroundView,
      public chromeos::ScreenLocker::ScreenLockViewContainer {
 public:
  ScreenLockerBackgroundView(views::Widget* lock_widget,
                             views::View* screen_lock_view)
      : lock_widget_(lock_widget),
        screen_lock_view_(screen_lock_view) {
  }

  virtual ScreenMode GetScreenMode() const OVERRIDE {
    return kScreenLockerMode;
  }

  virtual void Layout() OVERRIDE {
    chromeos::BackgroundView::Layout();
    gfx::Rect screen = bounds();
    if (screen_lock_view_) {
      gfx::Size size = screen_lock_view_->GetPreferredSize();
      gfx::Point origin((screen.width() - size.width()) / 2,
                        (screen.height() - size.height()) / 2);
      gfx::Size widget_size(screen.size());
      widget_size.Enlarge(-origin.x(), -origin.y());
      lock_widget_->SetBounds(gfx::Rect(widget_size));
    } else {
      // No password entry. Move the lock widget to off screen.
      lock_widget_->SetBounds(gfx::Rect(-100, -100, 1, 1));
    }
  }

  // ScreenLocker::ScreenLockViewContainer implementation:
  virtual void SetScreenLockView(views::View* screen_lock_view) OVERRIDE {
    screen_lock_view_ =  screen_lock_view;
    Layout();
  }

 private:
  views::Widget* lock_widget_;

  views::View* screen_lock_view_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockerBackgroundView);
};

}  // namespace

namespace chromeos {

// static
ScreenLocker* ScreenLocker::screen_locker_ = NULL;

// A event observer that forwards gtk events from one window to another.
// See screen_locker.h for more details.
class MouseEventRelay : public MessageLoopForUI::Observer {
 public:
  MouseEventRelay(GdkWindow* src, GdkWindow* dest)
      : src_(src),
        dest_(dest),
        initialized_(false) {
    DCHECK(src_);
    DCHECK(dest_);
  }

  virtual void WillProcessEvent(GdkEvent* event) OVERRIDE {}

  virtual void DidProcessEvent(GdkEvent* event) OVERRIDE {
    if (event->any.window != src_)
      return;
    if (!initialized_) {
      gint src_x, src_y, dest_x, dest_y, width, height, depth;
      gdk_window_get_geometry(dest_, &dest_x, &dest_y, &width, &height, &depth);
      // wait to compute offset until the info bubble widget's location
      // is available.
      if (dest_x < 0 || dest_y < 0)
        return;
      gdk_window_get_geometry(src_, &src_x, &src_y, &width, &height, &depth);
      offset_.SetPoint(dest_x - src_x, dest_y - src_y);
      initialized_ = true;
    }
    if (event->type == GDK_BUTTON_PRESS ||
        event->type == GDK_BUTTON_RELEASE) {
      GdkEvent* copy = gdk_event_copy(event);
      copy->button.window = dest_;
      g_object_ref(copy->button.window);
      copy->button.x -= offset_.x();
      copy->button.y -= offset_.y();

      gdk_event_put(copy);
      gdk_event_free(copy);
    } else if (event->type == GDK_MOTION_NOTIFY) {
      GdkEvent* copy = gdk_event_copy(event);
      copy->motion.window = dest_;
      g_object_ref(copy->motion.window);
      copy->motion.x -= offset_.x();
      copy->motion.y -= offset_.y();

      gdk_event_put(copy);
      gdk_event_free(copy);
    }
  }

 private:
  GdkWindow* src_;
  GdkWindow* dest_;
  bool initialized_;

  // Offset from src_'s origin to dest_'s origin.
  gfx::Point offset_;

  DISALLOW_COPY_AND_ASSIGN(MouseEventRelay);
};

// A event observer used to unlock the screen upon user's action
// without asking password. Used in BWSI and auto login mode.
// TODO(oshima): consolidate InputEventObserver and LockerInputEventObserver.
class InputEventObserver : public MessageLoopForUI::Observer {
 public:
  explicit InputEventObserver(ScreenLocker* screen_locker)
      : screen_locker_(screen_locker),
        activated_(false) {
  }

#if defined(TOUCH_UI)
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE {
    return base::EVENT_CONTINUE;
  }

  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE {
  }
#else
  virtual void WillProcessEvent(GdkEvent* event) OVERRIDE {
    if ((event->type == GDK_KEY_PRESS ||
         event->type == GDK_BUTTON_PRESS ||
         event->type == GDK_MOTION_NOTIFY) &&
        !activated_) {
      activated_ = true;
      std::string not_used_string;
      GaiaAuthConsumer::ClientLoginResult not_used;
      screen_locker_->OnLoginSuccess(not_used_string,
                                     not_used_string,
                                     not_used,
                                     false,
                                     false);
    }
  }

  virtual void DidProcessEvent(GdkEvent* event) OVERRIDE {
  }
#endif

 private:
  chromeos::ScreenLocker* screen_locker_;

  bool activated_;

  DISALLOW_COPY_AND_ASSIGN(InputEventObserver);
};

// A event observer used to show the screen locker upon
// user action: mouse or keyboard interactions.
// TODO(oshima): this has to be disabled while authenticating.
class LockerInputEventObserver : public MessageLoopForUI::Observer {
 public:
  explicit LockerInputEventObserver(ScreenLocker* screen_locker)
      : screen_locker_(screen_locker),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            timer_(FROM_HERE,
                   base::TimeDelta::FromSeconds(kScreenSaverIdleTimeout), this,
                   &LockerInputEventObserver::StartScreenSaver)) {
  }

#if defined(TOUCH_UI)
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE {
    return base::EVENT_CONTINUE;
  }

  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE {
  }
#else
  virtual void WillProcessEvent(GdkEvent* event) OVERRIDE {
    if ((event->type == GDK_KEY_PRESS ||
         event->type == GDK_BUTTON_PRESS ||
         event->type == GDK_MOTION_NOTIFY)) {
      timer_.Reset();
      screen_locker_->StopScreenSaver();
    }
  }

  virtual void DidProcessEvent(GdkEvent* event) OVERRIDE {
  }
#endif

 private:
  void StartScreenSaver() {
    screen_locker_->StartScreenSaver();
  }

  chromeos::ScreenLocker* screen_locker_;
  base::DelayTimer<LockerInputEventObserver> timer_;

  DISALLOW_COPY_AND_ASSIGN(LockerInputEventObserver);
};

//////////////////////////////////////////////////////////////////////////////
// ScreenLocker, public:

ScreenLocker::ScreenLocker(const UserManager::User& user)
    : lock_window_(NULL),
      lock_widget_(NULL),
      screen_lock_view_(NULL),
      captcha_view_(NULL),
      grab_container_(NULL),
      background_container_(NULL),
      user_(user),
      error_info_(NULL),
      drawn_(false),
      input_grabbed_(false),
      // TODO(oshima): support auto login mode (this is not implemented yet)
      // http://crosbug.com/1881
      unlock_on_input_(user_.email().empty()),
      locked_(false),
      start_time_(base::Time::Now()),
      login_status_consumer_(NULL) {
  DCHECK(!screen_locker_);
  screen_locker_ = this;
}

void ScreenLocker::Init() {
  static const GdkColor kGdkBlack = {0, 0, 0, 0};

  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);

  gfx::Point left_top(1, 1);
  gfx::Rect init_bounds(gfx::Screen::GetMonitorAreaNearestPoint(left_top));

  LockWindow* lock_window = new LockWindow();
  lock_window_ = lock_window->GetWidget();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = init_bounds;
  params.native_widget = lock_window;
  lock_window_->Init(params);
  gtk_widget_modify_bg(
      lock_window_->GetNativeView(), GTK_STATE_NORMAL, &kGdkBlack);

  g_signal_connect(lock_window_->GetNativeView(), "client-event",
                   G_CALLBACK(OnClientEventThunk), this);

  // GTK does not like zero width/height.
  if (!unlock_on_input_) {
    screen_lock_view_ = new ScreenLockView(this);
    screen_lock_view_->Init();
    screen_lock_view_->SetEnabled(false);
    screen_lock_view_->StartThrobber();
  } else {
    input_event_observer_.reset(new InputEventObserver(this));
    MessageLoopForUI::current()->AddObserver(input_event_observer_.get());
  }

  // Hang on to a cast version of the grab widget so we can call its
  // TryGrabAllInputs() method later.  (Nobody else needs to use it, so moving
  // its declaration to the header instead of keeping it in an anonymous
  // namespace feels a bit ugly.)
  GrabWidget* grab_widget = new GrabWidget(this);
  lock_widget_ = grab_widget->GetWidget();
  views::Widget::InitParams lock_params(
      views::Widget::InitParams::TYPE_CONTROL);
  lock_params.transparent = true;
  lock_params.parent_widget = lock_window_;
  lock_params.native_widget = grab_widget;
  lock_widget_->Init(lock_params);
  if (screen_lock_view_) {
    GrabWidgetRootView* root_view = new GrabWidgetRootView(screen_lock_view_);
    grab_container_ = root_view;
    lock_widget_->SetContentsView(root_view);
  }
  lock_widget_->Show();

  // Configuring the background url.
  std::string url_string =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kScreenSaverUrl);
  ScreenLockerBackgroundView* screen_lock_background_view_ =
      new ScreenLockerBackgroundView(lock_widget_, screen_lock_view_);
  background_container_ = screen_lock_background_view_;
  background_view_ = screen_lock_background_view_;
  background_view_->Init(GURL(url_string));

  // Gets user profile and sets default user 24hour flag since we don't
  // expose user profile in ScreenLockerBackgroundView.
  Profile* profile = ProfileManager::GetDefaultProfile();
  if (profile) {
    background_view_->SetDefaultUse24HourClock(
        profile->GetPrefs()->GetBoolean(prefs::kUse24HourClock));
  }

  if (background_view_->ScreenSaverEnabled())
    StartScreenSaver();

#if defined(TOOLKIT_USES_GTK)
  DCHECK(GTK_WIDGET_REALIZED(lock_window_->GetNativeView()));
  WmIpc::instance()->SetWindowType(
      lock_window_->GetNativeView(),
      WM_IPC_WINDOW_CHROME_SCREEN_LOCKER,
      NULL);
#endif

  lock_window_->SetContentsView(background_view_);
  lock_window_->Show();

  grab_widget->ClearGtkGrab();

  // Call this after lock_window_->Show(); otherwise the 1st invocation
  // of gdk_xxx_grab() will always fail.
  grab_widget->TryGrabAllInputs();

  // Add the window to its own group so that its grab won't be stolen if
  // gtk_grab_add() gets called on behalf on a non-screen-locker widget (e.g.
  // a modal dialog) -- see http://crosbug.com/8999.  We intentionally do this
  // after calling ClearGtkGrab(), as want to be in the default window group
  // then so we can break any existing GTK grabs.
  GtkWindowGroup* window_group = gtk_window_group_new();
  gtk_window_group_add_window(window_group,
                              GTK_WINDOW(lock_window_->GetNativeView()));
  g_object_unref(window_group);

  lock_window->set_toplevel_focus_widget(
      static_cast<views::NativeWidgetGtk*>(lock_widget_->native_widget())->
          window_contents());
}

void ScreenLocker::OnLoginFailure(const LoginFailure& error) {
  DVLOG(1) << "OnLoginFailure";
  UserMetrics::RecordAction(UserMetricsAction("ScreenLocker_OnLoginFailure"));
  if (authentication_start_time_.is_null()) {
    LOG(ERROR) << "authentication_start_time_ is not set";
  } else {
    base::TimeDelta delta = base::Time::Now() - authentication_start_time_;
    VLOG(1) << "Authentication failure time: " << delta.InSecondsF();
    UMA_HISTOGRAM_TIMES("ScreenLocker.AuthenticationFailureTime", delta);
  }

  EnableInput();
  // Don't enable signout button here as we're showing
  // MessageBubble.

  string16 msg = l10n_util::GetStringUTF16(IDS_LOGIN_ERROR_AUTHENTICATING);
  const std::string error_text = error.GetErrorString();
  // TODO(ivankr): use a format string instead of concatenation.
  if (!error_text.empty())
    msg += ASCIIToUTF16("\n") + ASCIIToUTF16(error_text);

  // Display a warning if Caps Lock is on.
  if (input_method::XKeyboard::CapsLockIsEnabled()) {
    msg += ASCIIToUTF16("\n") +
        l10n_util::GetStringUTF16(IDS_LOGIN_ERROR_CAPS_LOCK_HINT);
  }

  input_method::InputMethodManager* input_method_manager =
      input_method::InputMethodManager::GetInstance();
  if (input_method_manager->GetNumActiveInputMethods() > 1)
    msg += ASCIIToUTF16("\n") +
        l10n_util::GetStringUTF16(IDS_LOGIN_ERROR_KEYBOARD_SWITCH_HINT);

  ShowErrorBubble(UTF16ToWide(msg), views::BubbleBorder::BOTTOM_LEFT);

  if (login_status_consumer_)
    login_status_consumer_->OnLoginFailure(error);
}

void ScreenLocker::OnLoginSuccess(
    const std::string& username,
    const std::string& password,
    const GaiaAuthConsumer::ClientLoginResult& unused,
    bool pending_requests,
    bool using_oauth) {
  VLOG(1) << "OnLoginSuccess: Sending Unlock request.";
  if (authentication_start_time_.is_null()) {
    if (!username.empty())
      LOG(WARNING) << "authentication_start_time_ is not set";
  } else {
    base::TimeDelta delta = base::Time::Now() - authentication_start_time_;
    VLOG(1) << "Authentication success time: " << delta.InSecondsF();
    UMA_HISTOGRAM_TIMES("ScreenLocker.AuthenticationSuccessTime", delta);
  }

  Profile* profile = ProfileManager::GetDefaultProfile();
  if (profile) {
    ProfileSyncService* service = profile->GetProfileSyncService(username);
    if (service && !service->HasSyncSetupCompleted()) {
      // If sync has failed somehow, try setting the sync passphrase here.
      service->SetPassphrase(password, false);
    }
  }
  if (CrosLibrary::Get()->EnsureLoaded())
    CrosLibrary::Get()->GetScreenLockLibrary()->NotifyScreenUnlockRequested();

  if (login_status_consumer_)
    login_status_consumer_->OnLoginSuccess(username, password,
                                           unused, pending_requests,
                                           using_oauth);
}

void ScreenLocker::BubbleClosing(Bubble* bubble, bool closed_by_escape) {
  error_info_ = NULL;
  screen_lock_view_->SetSignoutEnabled(true);
  if (mouse_event_relay_.get()) {
    MessageLoopForUI::current()->RemoveObserver(mouse_event_relay_.get());
    mouse_event_relay_.reset();
  }
}

bool ScreenLocker::CloseOnEscape() {
  return true;
}

bool ScreenLocker::FadeInOnShow() {
  return false;
}

void ScreenLocker::OnLinkActivated(size_t index) {
}

void ScreenLocker::OnCaptchaEntered(const std::string& captcha) {
  // Captcha dialog is only shown when LoginPerformer instance exists,
  // i.e. blocking UI after password change is in place.
  DCHECK(LoginPerformer::default_performer());
  LoginPerformer::default_performer()->set_captcha(captcha);

  // ScreenLockView ownership is passed to grab_container_.
  // Need to save return value here so that compile
  // doesn't fail with "unused result" warning.
  views::View* view = secondary_view_.release();
  view = NULL;
  captcha_view_->SetVisible(false);
  grab_container_->SetScreenLockView(screen_lock_view_);
  background_container_->SetScreenLockView(screen_lock_view_);
  screen_lock_view_->SetVisible(true);
  screen_lock_view_->ClearAndSetFocusToPassword();

  // Take CaptchaView ownership now that it's removed from grab_container_.
  secondary_view_.reset(captcha_view_);
  ShowErrorMessage(postponed_error_message_, false);
  postponed_error_message_.clear();
}

void ScreenLocker::Authenticate(const string16& password) {
  authentication_start_time_ = base::Time::Now();
  screen_lock_view_->SetEnabled(false);
  screen_lock_view_->SetSignoutEnabled(false);
  screen_lock_view_->StartThrobber();

  // If LoginPerformer instance exists,
  // initial online login phase is still active.
  if (LoginPerformer::default_performer()) {
    DVLOG(1) << "Delegating authentication to LoginPerformer.";
    LoginPerformer::default_performer()->Login(user_.email(),
                                               UTF16ToUTF8(password));
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&Authenticator::AuthenticateToUnlock, authenticator_.get(),
                   user_.email(), UTF16ToUTF8(password)));
  }
}

void ScreenLocker::ClearErrors() {
  if (error_info_) {
    error_info_->Close();
    error_info_ = NULL;
  }
}

void ScreenLocker::EnableInput() {
  if (screen_lock_view_) {
    screen_lock_view_->SetEnabled(true);
    screen_lock_view_->ClearAndSetFocusToPassword();
    screen_lock_view_->StopThrobber();
  }
}

void ScreenLocker::Signout() {
  if (!error_info_) {
    UserMetrics::RecordAction(UserMetricsAction("ScreenLocker_Signout"));
#if defined(TOOLKIT_USES_GTK)
    WmIpc::instance()->NotifyAboutSignout();
#endif
    if (CrosLibrary::Get()->EnsureLoaded()) {
      CrosLibrary::Get()->GetLoginLibrary()->StopSession("");
    }

    // Don't hide yet the locker because the chrome screen may become visible
    // briefly.
  }
}

void ScreenLocker::ShowCaptchaAndErrorMessage(const GURL& captcha_url,
                                              const std::wstring& message) {
  postponed_error_message_ = message;
  if (captcha_view_) {
    captcha_view_->SetCaptchaURL(captcha_url);
  } else {
    captcha_view_ = new CaptchaView(captcha_url, true);
    captcha_view_->Init();
    captcha_view_->set_delegate(this);
  }
  // CaptchaView ownership is passed to grab_container_.
  views::View* view = secondary_view_.release();
  view = NULL;
  screen_lock_view_->SetVisible(false);
  grab_container_->SetScreenLockView(captcha_view_);
  background_container_->SetScreenLockView(captcha_view_);
  captcha_view_->SetVisible(true);
  // Take ScreenLockView ownership now that it's removed from grab_container_.
  secondary_view_.reset(screen_lock_view_);
}

void ScreenLocker::ShowErrorMessage(const std::wstring& message,
                                    bool sign_out_only) {
  if (sign_out_only) {
    screen_lock_view_->SetEnabled(false);
  } else {
    EnableInput();
  }
  screen_lock_view_->SetSignoutEnabled(sign_out_only);
  // Make sure that active Sign Out button is not hidden behind the bubble.
  ShowErrorBubble(
      message, sign_out_only ?
      views::BubbleBorder::BOTTOM_RIGHT : views::BubbleBorder::BOTTOM_LEFT);
}

void ScreenLocker::OnGrabInputs() {
  DVLOG(1) << "OnGrabInputs";
  input_grabbed_ = true;
  if (drawn_)
    ScreenLockReady();
}

views::View* ScreenLocker::GetViewByID(int id) {
  return lock_widget_->GetRootView()->GetViewByID(id);
}

void ScreenLocker::SetLoginStatusConsumer(
    chromeos::LoginStatusConsumer* consumer) {
  login_status_consumer_ = consumer;
}

// static
void ScreenLocker::Show() {
  VLOG(1) << "In ScreenLocker::Show";
  UserMetrics::RecordAction(UserMetricsAction("ScreenLocker_Show"));
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);

  // Exit fullscreen.
  Browser* browser = BrowserList::GetLastActive();
  // browser can be NULL if we receive a lock request before the first browser
  // window is shown.
  if (browser && browser->window()->IsFullscreen()) {
    browser->ToggleFullscreenMode(false);
  }

  if (!screen_locker_) {
    VLOG(1) << "Show: Locking screen";
    ScreenLocker* locker =
        new ScreenLocker(UserManager::Get()->logged_in_user());
#if defined(TOUCH_UI)
    // The screen locker does not reliably work on TOUCH_UI builds. In order
    // to effectively "lock" the screen we will sign out the user for now.
    // TODO(flackr): Implement lock screen in WebUI and remove this hack.
    locker->Signout();
#else
    locker->Init();
#endif
  } else {
    // PowerManager re-sends lock screen signal if it doesn't
    // receive the response within timeout. Just send complete
    // signal.
    VLOG(1) << "Show: locker already exists. Just sending completion event.";
    if (CrosLibrary::Get()->EnsureLoaded())
      CrosLibrary::Get()->GetScreenLockLibrary()->NotifyScreenLockCompleted();
  }
}

// static
void ScreenLocker::Hide() {
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
  DCHECK(screen_locker_);
  VLOG(1) << "Hide: Deleting ScreenLocker: " << screen_locker_;
  MessageLoopForUI::current()->DeleteSoon(FROM_HERE, screen_locker_);
}

// static
void ScreenLocker::UnlockScreenFailed() {
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
  if (screen_locker_) {
    // Power manager decided no to unlock the screen even if a user
    // typed in password, for example, when a user closed the lid
    // immediately after typing in the password.
    VLOG(1) << "UnlockScreenFailed: re-enabling screen locker.";
    screen_locker_->EnableInput();
  } else {
    // This can happen when a user requested unlock, but PowerManager
    // rejected because the computer is closed, then PowerManager unlocked
    // because it's open again and the above failure message arrives.
    // This'd be extremely rare, but may still happen.
    VLOG(1) << "UnlockScreenFailed: screen is already unlocked.";
  }
}

// static
void ScreenLocker::InitClass() {
  g_screen_lock_observer.Get();
}

////////////////////////////////////////////////////////////////////////////////
// ScreenLocker, private:

ScreenLocker::~ScreenLocker() {
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
  ClearErrors();
  if (input_event_observer_.get())
    MessageLoopForUI::current()->RemoveObserver(input_event_observer_.get());
  if (locker_input_event_observer_.get()) {
    lock_widget_->GetFocusManager()->UnregisterAccelerator(
        views::Accelerator(ui::VKEY_ESCAPE, false, false, false), this);
    MessageLoopForUI::current()->RemoveObserver(
        locker_input_event_observer_.get());
  }

  gdk_keyboard_ungrab(GDK_CURRENT_TIME);
  gdk_pointer_ungrab(GDK_CURRENT_TIME);

  DCHECK(lock_window_);
  VLOG(1) << "~ScreenLocker(): Closing ScreenLocker window.";
  lock_window_->Close();
  // lock_widget_ will be deleted by gtk's destroy signal.
  screen_locker_ = NULL;
  bool state = false;
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
      Source<ScreenLocker>(this),
      Details<bool>(&state));
  if (CrosLibrary::Get()->EnsureLoaded())
    CrosLibrary::Get()->GetScreenLockLibrary()->NotifyScreenUnlockCompleted();
}

void ScreenLocker::SetAuthenticator(Authenticator* authenticator) {
  authenticator_ = authenticator;
}

void ScreenLocker::ScreenLockReady() {
  VLOG(1) << "ScreenLockReady: sending completed signal to power manager.";
  locked_ = true;
  base::TimeDelta delta = base::Time::Now() - start_time_;
  VLOG(1) << "Screen lock time: " << delta.InSecondsF();
  UMA_HISTOGRAM_TIMES("ScreenLocker.ScreenLockTime", delta);

  if (background_view_->ScreenSaverEnabled()) {
    lock_widget_->GetFocusManager()->RegisterAccelerator(
        views::Accelerator(ui::VKEY_ESCAPE, false, false, false), this);
    locker_input_event_observer_.reset(new LockerInputEventObserver(this));
    MessageLoopForUI::current()->AddObserver(
        locker_input_event_observer_.get());
  } else {
    // Don't enable the password field until we grab all inputs.
    EnableInput();
  }

  bool state = true;
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
      Source<ScreenLocker>(this),
      Details<bool>(&state));
  if (CrosLibrary::Get()->EnsureLoaded())
    CrosLibrary::Get()->GetScreenLockLibrary()->NotifyScreenLockCompleted();
}

void ScreenLocker::OnClientEvent(GtkWidget* widge, GdkEventClient* event) {
#if defined(TOOLKIT_USES_GTK)
  WmIpc::Message msg;
  WmIpc::instance()->DecodeMessage(*event, &msg);
  if (msg.type() == WM_IPC_MESSAGE_CHROME_NOTIFY_SCREEN_REDRAWN_FOR_LOCK) {
    OnWindowManagerReady();
  }
#endif
}

void ScreenLocker::OnWindowManagerReady() {
  DVLOG(1) << "OnClientEvent: drawn for lock";
  drawn_ = true;
  if (input_grabbed_)
    ScreenLockReady();
}

void ScreenLocker::ShowErrorBubble(
    const std::wstring& message,
    views::BubbleBorder::ArrowLocation arrow_location) {
  if (error_info_)
    error_info_->Close();

  gfx::Rect rect = screen_lock_view_->GetPasswordBoundsRelativeTo(
      lock_widget_->GetRootView());
  gfx::Rect lock_widget_bounds = lock_widget_->GetClientAreaScreenBounds();
  rect.Offset(lock_widget_bounds.x(), lock_widget_bounds.y());
  error_info_ = MessageBubble::ShowNoGrab(
      lock_window_,
      rect,
      arrow_location,
      ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_WARNING),
      message,
      std::wstring(),  // TODO(nkostylev): Add help link.
      this);

#if !defined(TOUCH_UI)
  if (mouse_event_relay_.get())
    MessageLoopForUI::current()->RemoveObserver(mouse_event_relay_.get());
  mouse_event_relay_.reset(
      new MouseEventRelay(lock_widget_->GetNativeView()->window,
                          error_info_->GetNativeView()->window));
  MessageLoopForUI::current()->AddObserver(mouse_event_relay_.get());
#endif
}

void ScreenLocker::StopScreenSaver() {
  if (background_view_->IsScreenSaverVisible()) {
    VLOG(1) << "StopScreenSaver";
    if (screen_lock_view_) {
      screen_lock_view_->SetVisible(true);
      // Place the screen_lock_view_ to the center of the screen.
      // Must be called when the view is visible: crosbug.com/15213.
      background_view_->Layout();
      screen_lock_view_->RequestFocus();
    }
    background_view_->HideScreenSaver();
    EnableInput();
  }
}

void ScreenLocker::StartScreenSaver() {
  if (!background_view_->IsScreenSaverVisible()) {
    VLOG(1) << "StartScreenSaver";
    UserMetrics::RecordAction(
        UserMetricsAction("ScreenLocker_StartScreenSaver"));
    background_view_->ShowScreenSaver();
    if (screen_lock_view_) {
      screen_lock_view_->SetEnabled(false);
      screen_lock_view_->SetVisible(false);
    }
    ClearErrors();
  }
}

bool ScreenLocker::AcceleratorPressed(const views::Accelerator& accelerator) {
  if (!background_view_->IsScreenSaverVisible()) {
    StartScreenSaver();
    return true;
  }
  return false;
}

}  // namespace chromeos
