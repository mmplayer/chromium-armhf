// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class listens for notifications from the Google Push notifications
// service, and signals when they arrive.  It checks all incoming stanzas to
// see if they look like notifications, and filters out those which are not
// valid.
//
// The task is deleted automatically by the buzz::XmppClient. This occurs in the
// destructor of TaskRunner, which is a superclass of buzz::XmppClient.

#ifndef JINGLE_NOTIFIER_PUSH_NOTIFICATIONS_LISTENER_LISTEN_TASK_H_
#define JINGLE_NOTIFIER_PUSH_NOTIFICATIONS_LISTENER_LISTEN_TASK_H_

#include "talk/xmpp/xmpptask.h"

namespace buzz {
class XmlElement;
}

namespace notifier {

struct Notification;

class PushNotificationsListenTask : public buzz::XmppTask {
 public:
  class Delegate {
   public:
     virtual ~Delegate() {}
     virtual void OnNotificationReceived(
        const Notification& notification) = 0;
  };

  PushNotificationsListenTask(buzz::XmppTaskParentInterface* parent,
                              Delegate* delegate);
  virtual ~PushNotificationsListenTask();

  // Overriden from buzz::XmppTask.
  virtual int ProcessStart();
  virtual int ProcessResponse();
  virtual bool HandleStanza(const buzz::XmlElement* stanza);

 private:
  bool IsValidNotification(const buzz::XmlElement* stanza);

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(PushNotificationsListenTask);
};

typedef PushNotificationsListenTask::Delegate
    PushNotificationsListenTaskDelegate;

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_PUSH_NOTIFICATIONS_LISTENER_LISTEN_TASK_H_
