// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Methods for sending the update stanza to notify peers via xmpp.

#ifndef JINGLE_NOTIFIER_LISTENER_PUSH_NOTIFICATIONS_SEND_UPDATE_TASK_H_
#define JINGLE_NOTIFIER_LISTENER_PUSH_NOTIFICATIONS_SEND_UPDATE_TASK_H_

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "jingle/notifier/listener/notification_defines.h"
#include "talk/xmpp/xmpptask.h"

namespace buzz {
class Jid;
class XmlElement;
}  // namespace

namespace notifier {

class PushNotificationsSendUpdateTask : public buzz::XmppTask {
 public:
  PushNotificationsSendUpdateTask(
      buzz::XmppTaskParentInterface* parent, const Notification& notification);
  virtual ~PushNotificationsSendUpdateTask();

  // Overridden from buzz::XmppTask.
  virtual int ProcessStart();

 private:
  // Allocates and constructs an buzz::XmlElement containing the update stanza.
  static buzz::XmlElement* MakeUpdateMessage(
      const Notification& notification, const buzz::Jid& to_jid_bare);

  const Notification notification_;

  FRIEND_TEST_ALL_PREFIXES(PushNotificationsSendUpdateTaskTest,
                           MakeUpdateMessage);

  DISALLOW_COPY_AND_ASSIGN(PushNotificationsSendUpdateTask);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_PUSH_NOTIFICATIONS_SEND_UPDATE_TASK_H_
