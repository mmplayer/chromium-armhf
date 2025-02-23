// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WM_IPC_H_
#define CHROME_BROWSER_CHROMEOS_WM_IPC_H_
#pragma once

#include <gtk/gtk.h>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"

typedef unsigned long Atom;
typedef unsigned long XID;

namespace base {
template <typename T> struct DefaultLazyInstanceTraits;
}
namespace gfx {
class Rect;
}

namespace chromeos {

class WmIpc {
 public:
  enum AtomType {
    ATOM_CHROME_LAYOUT_MODE = 0,
    ATOM_CHROME_LOGGED_IN,
    ATOM_CHROME_STATE,
    ATOM_CHROME_STATE_COLLAPSED_PANEL,
    ATOM_CHROME_STATE_STATUS_HIDDEN,
    ATOM_CHROME_STATUS_BOUNDS,
    ATOM_CHROME_WINDOW_TYPE,
    ATOM_CHROME_WM_MESSAGE,
    ATOM_MANAGER,
    ATOM_STRING,
    ATOM_UTF8_STRING,
    ATOM_WM_S0,
    kNumAtoms,
  };

  struct Message {
   public:
    Message() {
      Init(WM_IPC_MESSAGE_UNKNOWN);
    }
    // WmIpcMessageType is defined in chromeos_wm_ipc_enums.h.
    explicit Message(WmIpcMessageType type) {
      Init(type);
    }

    WmIpcMessageType type() const { return type_; }
    void set_type(WmIpcMessageType type) { type_ = type; }

    inline int max_params() const {
      return arraysize(params_);
    }
    long param(int index) const {
      DCHECK_GE(index, 0);
      DCHECK_LT(index, max_params());
      return params_[index];
    }
    void set_param(int index, long value) {
      DCHECK_GE(index, 0);
      DCHECK_LT(index, max_params());
      params_[index] = value;
    }

   private:
    // Common initialization code shared between constructors.
    void Init(WmIpcMessageType type) {
      set_type(type);
      for (int i = 0; i < max_params(); ++i) {
        set_param(i, 0);
      }
    }

    // Type of message that was sent.
    WmIpcMessageType type_;

    // Type-specific data.  This is bounded by the number of 32-bit values
    // that we can pack into a ClientMessageEvent -- it holds five, but we
    // use the first one to store the message type.
    long params_[4];
  };

  // Returns the single instance of WmIpc.
  static WmIpc* instance();

  WmIpcLayoutMode layout_mode() const { return layout_mode_; }

  // Gets or sets a property describing a window's type.
  // WmIpcMessageType is defined in chromeos_wm_ipc_enums.h.  Type-specific
  // parameters may also be supplied.  The caller is responsible for trapping
  // errors from the X server.
  bool SetWindowType(GtkWidget* widget,
                     WmIpcWindowType type,
                     const std::vector<int>* params);

  // Gets the string name of an atom from the AtomType enum.
  std::string GetAtomName(AtomType type) const;

  // Gets the type of the window, and any associated parameters. The
  // caller is responsible for trapping errors from the X server.  If
  // the parameters are not interesting to the caller, NULL may be
  // passed for |params|.
  WmIpcWindowType GetWindowType(GtkWidget* widget, std::vector<int>* params);

  // Gets the set of atoms contained in a window's _CHROME_STATE property.
  // Returns false if the property isn't set.
  bool GetWindowState(GtkWidget* widget, std::set<AtomType>* atom_types);

  // Sends a message to the WM.
  void SendMessage(const Message& msg);

  // If |event| is a valid Message it is decoded into |msg| and true is
  // returned. If false is returned, |event| is not a valid Message.
  bool DecodeMessage(const GdkEventClient& event, Message* msg);

  // Handles ClientMessage events that weren't decodable using DecodeMessage().
  // Specifically, this catches messages about the WM_S0 selection that get sent
  // when a window manager process starts (so that we can re-run InitWmInfo()).
  // See ICCCM 2.8 for more info about MANAGER selections.
  void HandleNonChromeClientMessageEvent(const GdkEventClient& event);

  // Handle an event reporting a property change on the root window.
  void HandleRootWindowPropertyEvent(const GdkEventProperty& event);

  // Sets a _CHROME_LOGGED_IN property on the root window describing whether
  // the user is currently logged in or not.
  void SetLoggedInProperty(bool logged_in);

  // Sets a _CHROME_STATUS_BOUNDS property on toplevel window |widget|
  // describing the status area's bounds within the window.
  void SetStatusBoundsProperty(GtkWidget* widget, const gfx::Rect& bounds);

  // Sends a message to the window manager notifying it that we're signing out.
  void NotifyAboutSignout();

 private:
  friend struct base::DefaultLazyInstanceTraits<WmIpc>;

  WmIpc();
  ~WmIpc();

  // Initializes 'wm_' and sends the window manager a message telling it the
  // version of the IPC protocol that we support.  This is called in our
  // constructor, but needs to be re-run if the window manager gets restarted.
  void InitWmInfo();

  // Updates |layout_mode_| based on the current value of the root window's
  // _CHROME_LAYOUT_MODE property.
  void FetchLayoutModeProperty();

  // Maps between our Atom enum and the X server's atom IDs and from the
  // server's IDs to atoms' string names.
  std::map<AtomType, Atom> type_to_atom_;
  std::map<Atom, AtomType> atom_to_type_;
  std::map<Atom, std::string> atom_to_string_;

  // Cached value of type_to_atom_[ATOM_CHROME_WM_MESSAGE].
  Atom wm_message_atom_;

  // Handle to the WM. Used for sending messages.
  XID wm_;

  // The current value of the root window's _CHROME_LAYOUT_MODE property.
  WmIpcLayoutMode layout_mode_;

  DISALLOW_COPY_AND_ASSIGN(WmIpc);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WM_IPC_H_
