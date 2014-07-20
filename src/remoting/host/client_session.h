// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CLIENT_SESSION_H_
#define REMOTING_HOST_CLIENT_SESSION_H_

#include <list>
#include <set>

#include "base/time.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"
#include "third_party/skia/include/core/SkPoint.h"

namespace remoting {

class Capturer;
class UserAuthenticator;

// A ClientSession keeps a reference to a connection to a client, and maintains
// per-client state.
class ClientSession : public protocol::HostStub,
                      public protocol::InputStub,
                      public base::RefCountedThreadSafe<ClientSession> {
 public:
  // Callback interface for passing events to the ChromotingHost.
  class EventHandler {
   public:
    virtual ~EventHandler() {}

    // Called to signal that local login has succeeded and ChromotingHost can
    // proceed with the next step.
    virtual void LocalLoginSucceeded(
        scoped_refptr<protocol::ConnectionToClient> client) = 0;

    // Called to signal that local login has failed.
    virtual void LocalLoginFailed(
        scoped_refptr<protocol::ConnectionToClient> client) = 0;
  };

  // Takes ownership of |user_authenticator|. Does not take ownership of
  // |event_handler|, |input_stub| or |capturer|.
  ClientSession(EventHandler* event_handler,
                UserAuthenticator* user_authenticator,
                scoped_refptr<protocol::ConnectionToClient> connection,
                protocol::InputStub* input_stub,
                Capturer* capturer);

  // protocol::HostStub interface.
  virtual void BeginSessionRequest(
      const protocol::LocalLoginCredentials* credentials,
      const base::Closure& done);

  // protocol::InputStub interface.
  virtual void InjectKeyEvent(const protocol::KeyEvent& event);
  virtual void InjectMouseEvent(const protocol::MouseEvent& event);

  // Notifier called when the client is being disconnected.
  // This should only be called by ChromotingHost.
  void OnDisconnected();

  // Set the authenticated flag or log a failure message as appropriate.
  void OnAuthorizationComplete(bool success);

  protocol::ConnectionToClient* connection() const {
    return connection_.get();
  }

  bool authenticated() const {
    return authenticated_;
  }

  void set_awaiting_continue_approval(bool awaiting) {
    awaiting_continue_approval_ = awaiting;
  }

  const std::string& client_jid() { return client_jid_; }

  // Indicate that local mouse activity has been detected. This causes remote
  // inputs to be ignored for a short time so that the local user will always
  // have the upper hand in 'pointer wars'.
  void LocalMouseMoved(const SkIPoint& new_pos);

  bool ShouldIgnoreRemoteMouseInput(const protocol::MouseEvent& event) const;
  bool ShouldIgnoreRemoteKeyboardInput(const protocol::KeyEvent& event) const;

 private:
  friend class base::RefCountedThreadSafe<ClientSession>;
  friend class ClientSessionTest_RestoreEventState_Test;
  virtual ~ClientSession();

  // Keep track of input state so that we can clean up the event queue when
  // the user disconnects.
  void RecordKeyEvent(const protocol::KeyEvent& event);
  void RecordMouseButtonState(const protocol::MouseEvent& event);

  // Synthesize KeyUp and MouseUp events so that we can undo these events
  // when the user disconnects.
  void RestoreEventState();

  EventHandler* event_handler_;

  // A factory for user authenticators.
  scoped_ptr<UserAuthenticator> user_authenticator_;

  // The connection to the client.
  scoped_refptr<protocol::ConnectionToClient> connection_;

  std::string client_jid_;

  // The input stub to which this object delegates.
  protocol::InputStub* input_stub_;

  // Capturer, used to determine current screen size for ensuring injected
  // mouse events fall within the screen area.
  // TODO(lambroslambrou): Move floor-control logic, and clamping to screen
  // area, out of this class (crbug.com/96508).
  Capturer* capturer_;

  // Whether this client is authenticated.
  bool authenticated_;

  // Whether or not inputs from this client are blocked pending approval from
  // the host user to continue the connection.
  bool awaiting_continue_approval_;

  // State to control remote input blocking while the local pointer is in use.
  uint32 remote_mouse_button_state_;

  // Current location of the mouse pointer. This is used to provide appropriate
  // coordinates when we release the mouse buttons after a user disconnects.
  SkIPoint remote_mouse_pos_;

  // Queue of recently-injected mouse positions.  This is used to detect whether
  // mouse events from the local input monitor are echoes of injected positions,
  // or genuine mouse movements of a local input device.
  std::list<SkIPoint> injected_mouse_positions_;

  base::Time latest_local_input_time_;

  // Set of keys that are currently pressed down by the user. This is used so
  // we can release them if the user disconnects.
  std::set<int> pressed_keys_;

  DISALLOW_COPY_AND_ASSIGN(ClientSession);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CLIENT_SESSION_H_
