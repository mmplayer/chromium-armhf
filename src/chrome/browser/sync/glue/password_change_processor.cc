// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/password_change_processor.h"

#include <string>

#include "base/location.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_store_change.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/password_model_associator.h"
#include "chrome/browser/sync/internal_api/change_record.h"
#include "chrome/browser/sync/internal_api/read_node.h"
#include "chrome/browser/sync/internal_api/write_node.h"
#include "chrome/browser/sync/internal_api/write_transaction.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/password_specifics.pb.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "webkit/glue/password_form.h"

namespace browser_sync {

PasswordChangeProcessor::PasswordChangeProcessor(
    PasswordModelAssociator* model_associator,
    PasswordStore* password_store,
    UnrecoverableErrorHandler* error_handler)
    : ChangeProcessor(error_handler),
      model_associator_(model_associator),
      password_store_(password_store),
      observing_(false),
      expected_loop_(MessageLoop::current()) {
  DCHECK(model_associator);
  DCHECK(error_handler);
#if defined(OS_MACOSX)
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
#else
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
#endif
  StartObserving();
}

PasswordChangeProcessor::~PasswordChangeProcessor() {
  DCHECK(expected_loop_ == MessageLoop::current());
}

void PasswordChangeProcessor::Observe(int type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  DCHECK(expected_loop_ == MessageLoop::current());
  DCHECK(chrome::NOTIFICATION_LOGINS_CHANGED == type);
  if (!observing_)
    return;

  DCHECK(running());

  sync_api::WriteTransaction trans(FROM_HERE, share_handle());

  sync_api::ReadNode password_root(&trans);
  if (!password_root.InitByTagLookup(kPasswordTag)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "Server did not create the top-level password node. "
        "We might be running against an out-of-date server.");
    return;
  }

  PasswordStoreChangeList* changes =
      Details<PasswordStoreChangeList>(details).ptr();
  for (PasswordStoreChangeList::iterator change = changes->begin();
       change != changes->end(); ++change) {
    std::string tag = PasswordModelAssociator::MakeTag(change->form());
    switch (change->type()) {
      case PasswordStoreChange::ADD: {
        sync_api::WriteNode sync_node(&trans);
        if (!sync_node.InitUniqueByCreation(syncable::PASSWORDS,
                                            password_root, tag)) {
          error_handler()->OnUnrecoverableError(FROM_HERE,
              "Failed to create password sync node.");
          return;
        }

        PasswordModelAssociator::WriteToSyncNode(change->form(), &sync_node);
        model_associator_->Associate(&tag, sync_node.GetId());
        break;
      }
      case PasswordStoreChange::UPDATE: {
        sync_api::WriteNode sync_node(&trans);
        int64 sync_id = model_associator_->GetSyncIdFromChromeId(tag);
        if (sync_api::kInvalidId == sync_id) {
          error_handler()->OnUnrecoverableError(FROM_HERE,
              "Unexpected notification for: ");
          return;
        } else {
          if (!sync_node.InitByIdLookup(sync_id)) {
            error_handler()->OnUnrecoverableError(FROM_HERE,
                "Password node lookup failed.");
            return;
          }
        }

        PasswordModelAssociator::WriteToSyncNode(change->form(), &sync_node);
        break;
      }
      case PasswordStoreChange::REMOVE: {
        sync_api::WriteNode sync_node(&trans);
        int64 sync_id = model_associator_->GetSyncIdFromChromeId(tag);
        if (sync_api::kInvalidId == sync_id) {
          // We've been asked to remove a password that we don't know about.
          // That's weird, but apparently we were already in the requested
          // state, so it's not really an unrecoverable error. Just return.
          LOG(WARNING) << "Trying to delete nonexistent password sync node!";
          return;
        } else {
          if (!sync_node.InitByIdLookup(sync_id)) {
            error_handler()->OnUnrecoverableError(FROM_HERE,
                "Password node lookup failed.");
            return;
          }
          model_associator_->Disassociate(sync_node.GetId());
          sync_node.Remove();
        }
        break;
      }
    }
  }
}

void PasswordChangeProcessor::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction* trans,
    const sync_api::ImmutableChangeRecordList& changes) {
  DCHECK(expected_loop_ == MessageLoop::current());
  if (!running())
    return;

  sync_api::ReadNode password_root(trans);
  if (!password_root.InitByTagLookup(kPasswordTag)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "Password root node lookup failed.");
    return;
  }

  DCHECK(deleted_passwords_.empty() && new_passwords_.empty() &&
         updated_passwords_.empty());

  for (sync_api::ChangeRecordList::const_iterator it =
           changes.Get().begin(); it != changes.Get().end(); ++it) {
    if (sync_api::ChangeRecord::ACTION_DELETE ==
        it->action) {
      DCHECK(it->specifics.HasExtension(sync_pb::password))
          << "Password specifics data not present on delete!";
      DCHECK(it->extra.get());
      sync_api::ExtraPasswordChangeRecordData* extra =
          it->extra.get();
      const sync_pb::PasswordSpecificsData& password = extra->unencrypted();
      webkit_glue::PasswordForm form;
      PasswordModelAssociator::CopyPassword(password, &form);
      deleted_passwords_.push_back(form);
      model_associator_->Disassociate(it->id);
      continue;
    }

    sync_api::ReadNode sync_node(trans);
    if (!sync_node.InitByIdLookup(it->id)) {
      error_handler()->OnUnrecoverableError(FROM_HERE,
          "Password node lookup failed.");
      return;
    }

    // Check that the changed node is a child of the passwords folder.
    DCHECK_EQ(password_root.GetId(), sync_node.GetParentId());
    DCHECK_EQ(syncable::PASSWORDS, sync_node.GetModelType());

    const sync_pb::PasswordSpecificsData& password_data =
        sync_node.GetPasswordSpecifics();
    webkit_glue::PasswordForm password;
    PasswordModelAssociator::CopyPassword(password_data, &password);

    if (sync_api::ChangeRecord::ACTION_ADD == it->action) {
      std::string tag(PasswordModelAssociator::MakeTag(password));
      model_associator_->Associate(&tag, sync_node.GetId());
      new_passwords_.push_back(password);
    } else {
      DCHECK_EQ(sync_api::ChangeRecord::ACTION_UPDATE, it->action);
      updated_passwords_.push_back(password);
    }
  }
}

void PasswordChangeProcessor::CommitChangesFromSyncModel() {
  DCHECK(expected_loop_ == MessageLoop::current());
  if (!running())
    return;
  StopObserving();

  if (!model_associator_->WriteToPasswordStore(&new_passwords_,
                                               &updated_passwords_,
                                               &deleted_passwords_)) {
    error_handler()->OnUnrecoverableError(FROM_HERE, "Error writing passwords");
    return;
  }

  deleted_passwords_.clear();
  new_passwords_.clear();
  updated_passwords_.clear();

  StartObserving();
}

void PasswordChangeProcessor::StartImpl(Profile* profile) {
  DCHECK(expected_loop_ == MessageLoop::current());
  observing_ = true;
}

void PasswordChangeProcessor::StopImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observing_ = false;
}


void PasswordChangeProcessor::StartObserving() {
  DCHECK(expected_loop_ == MessageLoop::current());
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_LOGINS_CHANGED,
                              Source<PasswordStore>(password_store_));
}

void PasswordChangeProcessor::StopObserving() {
  DCHECK(expected_loop_ == MessageLoop::current());
  notification_registrar_.Remove(this,
                                 chrome::NOTIFICATION_LOGINS_CHANGED,
                                 Source<PasswordStore>(password_store_));
}

}  // namespace browser_sync
