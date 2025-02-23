// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/inspectable_tab_proxy.h"

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/debugger/debugger_remote_service.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/debugger/devtools_client_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/devtools_messages.h"

DevToolsClientHostImpl::DevToolsClientHostImpl(
    int32 id,
    DebuggerRemoteService* service,
    InspectableTabProxy::IdToClientHostMap* map)
    : id_(id),
      service_(service),
      map_(map) {}

DevToolsClientHostImpl::~DevToolsClientHostImpl() {
  map_->erase(this->id_);
}

// The debugged tab has closed.
void DevToolsClientHostImpl::InspectedTabClosing() {
  TabClosed();
  delete this;
}

// The remote debugger has detached.
void DevToolsClientHostImpl::CloseImpl() {
  NotifyCloseListener();
  delete this;
}

void DevToolsClientHostImpl::SendMessageToClient(
    const IPC::Message& msg) {
  // TODO(prybin): Restore FrameNavigate.
  IPC_BEGIN_MESSAGE_MAP(DevToolsClientHostImpl, msg)
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_DebuggerOutput, OnDebuggerOutput);
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

void DevToolsClientHostImpl::TabReplaced(TabContents* new_tab) {
  map_->erase(id_);
  TabContentsWrapper* new_tab_wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(new_tab);
  DCHECK(new_tab_wrapper);
  if (!new_tab_wrapper)
    return;
  id_ = new_tab_wrapper->restore_tab_helper()->session_id().id();
  (*map_)[id_] = this;
}

void DevToolsClientHostImpl::OnDebuggerOutput(const std::string& data) {
  service_->DebuggerOutput(id_, data);
}

void DevToolsClientHostImpl::FrameNavigating(const std::string& url) {
  service_->FrameNavigate(id_, url);
}

void DevToolsClientHostImpl::TabClosed() {
  service_->TabClosed(id_);
}

InspectableTabProxy::InspectableTabProxy() {}

InspectableTabProxy::~InspectableTabProxy() {}

const InspectableTabProxy::TabMap& InspectableTabProxy::tab_map() {
  tab_map_.clear();
  for (BrowserList::const_iterator it = BrowserList::begin(),
       end = BrowserList::end(); it != end; ++it) {
    TabStripModel* model = (*it)->tabstrip_model();
    for (int i = 0, size = model->count(); i < size; ++i) {
      TabContentsWrapper* tab = model->GetTabContentsAt(i);
      tab_map_[tab->restore_tab_helper()->session_id().id()] = tab;
    }
  }
  return tab_map_;
}

DevToolsClientHostImpl* InspectableTabProxy::ClientHostForTabId(
    int32 id) {
  InspectableTabProxy::IdToClientHostMap::const_iterator it =
      id_to_client_host_map_.find(id);
  if (it == id_to_client_host_map_.end()) {
    return NULL;
  }
  return it->second;
}

DevToolsClientHost* InspectableTabProxy::NewClientHost(
    int32 id,
    DebuggerRemoteService* service) {
  DevToolsClientHostImpl* client_host =
      new DevToolsClientHostImpl(id, service, &id_to_client_host_map_);
  id_to_client_host_map_[id] = client_host;
  return client_host;
}

void InspectableTabProxy::OnRemoteDebuggerDetached() {
  while (!id_to_client_host_map_.empty()) {
    IdToClientHostMap::iterator it = id_to_client_host_map_.begin();
    it->second->debugger_remote_service()->DetachFromTab(
        base::IntToString(it->first), NULL);
  }
}
