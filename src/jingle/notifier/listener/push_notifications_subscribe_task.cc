// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/push_notifications_subscribe_task.h"

#include <string>

#include "base/logging.h"
#include "jingle/notifier/listener/notification_constants.h"
#include "jingle/notifier/listener/xml_element_util.h"
#include "talk/base/task.h"
#include "talk/xmllite/qname.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/xmppengine.h"

namespace notifier {

PushNotificationsSubscribeTask::PushNotificationsSubscribeTask(
    buzz::XmppTaskParentInterface* parent,
    const SubscriptionList& subscriptions,
    Delegate* delegate)
    : XmppTask(parent, buzz::XmppEngine::HL_SINGLE),
      subscriptions_(subscriptions), delegate_(delegate) {
}

PushNotificationsSubscribeTask::~PushNotificationsSubscribeTask() {
}

bool PushNotificationsSubscribeTask::HandleStanza(
    const buzz::XmlElement* stanza) {
  if (!MatchResponseIq(stanza, GetClient()->jid().BareJid(), task_id()))
    return false;
  QueueStanza(stanza);
  return true;
}

int PushNotificationsSubscribeTask::ProcessStart() {
  VLOG(1) << "Push notifications: Subscription task started.";
  scoped_ptr<buzz::XmlElement> iq_stanza(
      MakeSubscriptionMessage(subscriptions_, GetClient()->jid(),
                              task_id()));
  std::string stanza_str = XmlElementToString(*iq_stanza.get());
  VLOG(1) << "Push notifications: Subscription stanza: "
          << XmlElementToString(*iq_stanza.get());

  if (SendStanza(iq_stanza.get()) != buzz::XMPP_RETURN_OK) {
    if (delegate_)
      delegate_->OnSubscriptionError();
    return STATE_ERROR;
  }
  return STATE_RESPONSE;
}

int PushNotificationsSubscribeTask::ProcessResponse() {
  VLOG(1) << "Push notifications: Subscription response received.";
  const buzz::XmlElement* stanza = NextStanza();
  if (stanza == NULL) {
    return STATE_BLOCKED;
  }
  std::string stanza_str = XmlElementToString(*stanza);
  VLOG(1) << "Push notifications: Subscription response: "
          << XmlElementToString(*stanza);
  // We've receieved a response to our subscription request.
  if (stanza->HasAttr(buzz::QN_TYPE) &&
    stanza->Attr(buzz::QN_TYPE) == buzz::STR_RESULT) {
    if (delegate_)
      delegate_->OnSubscribed();
    return STATE_DONE;
  }
  // An error response was received.
  if (delegate_)
    delegate_->OnSubscriptionError();
  return STATE_ERROR;
}

buzz::XmlElement* PushNotificationsSubscribeTask::MakeSubscriptionMessage(
    const SubscriptionList& subscriptions,
    const buzz::Jid& jid, const std::string& task_id) {
  DCHECK(jid.IsFull());
  static const buzz::QName kQnSubscribe(
      kPushNotificationsNamespace, "subscribe");

  // Create the subscription stanza using the notifications protocol.
  // <iq from={full_jid} to={bare_jid} type=�set� id={id}>
  //  <subscribe xmlns=�google:push�>
  //    <item channel={channel_name} from={domain_name or bare_jid}/>
  //    <item channel={channel_name2} from={domain_name or bare_jid}/>
  //    <item channel={channel_name3} from={domain_name or bare_jid}/>
  //  </subscribe>
  // </iq>
  buzz::XmlElement* iq = MakeIq(buzz::STR_SET, jid.BareJid(), task_id);
  buzz::XmlElement* subscribe = new buzz::XmlElement(kQnSubscribe, true);
  iq->AddElement(subscribe);

  for (SubscriptionList::const_iterator iter =
           subscriptions.begin(); iter != subscriptions.end(); ++iter) {
    buzz::XmlElement* item = new buzz::XmlElement(
        buzz::QName(kPushNotificationsNamespace, "item"));
    item->AddAttr(buzz::QName(buzz::STR_EMPTY, "channel"),
                  iter->channel.c_str());
    item->AddAttr(buzz::QN_FROM, iter->from.c_str());
    subscribe->AddElement(item);
  }
  return iq;
}

}  // namespace notifier
