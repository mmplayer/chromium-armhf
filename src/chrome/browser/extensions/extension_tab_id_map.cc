// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tab_id_map.h"

#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_service.h"
#include "content/public/browser/notification_types.h"

// ExtensionTabIdMap is a Singleton, so it doesn't need refcounting.
DISABLE_RUNNABLE_METHOD_REFCOUNT(ExtensionTabIdMap);

//
// ExtensionTabIdMap::TabObserver
//

// This class listens for notifications about new and closed tabs on the UI
// thread, and notifies the ExtensionTabIdMap on the IO thread. It should only
// ever be accessed on the UI thread.
class ExtensionTabIdMap::TabObserver : public NotificationObserver {
 public:
  TabObserver();
  ~TabObserver();

 private:
  // NotificationObserver interface.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  NotificationRegistrar registrar_;
};

ExtensionTabIdMap::TabObserver::TabObserver() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB,
                 NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_DELETED,
                 NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_TAB_PARENTED,
                 NotificationService::AllBrowserContextsAndSources());
}

ExtensionTabIdMap::TabObserver::~TabObserver() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void ExtensionTabIdMap::TabObserver::Observe(
    int type, const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB: {
      TabContents* contents = Source<TabContents>(source).ptr();
      TabContentsWrapper* tab =
          TabContentsWrapper::GetCurrentWrapperForContents(contents);
      if (!tab)
        break;
      RenderViewHost* host = Details<RenderViewHost>(details).ptr();
      // TODO(mpcmoplete): How can we tell if window_id is bogus? It may not
      // have been set yet.
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(
              ExtensionTabIdMap::GetInstance(),
              &ExtensionTabIdMap::SetTabAndWindowId,
              host->process()->id(), host->routing_id(),
              tab->restore_tab_helper()->session_id().id(),
              tab->restore_tab_helper()->window_id().id()));
      break;
    }
    case content::NOTIFICATION_TAB_PARENTED: {
      TabContentsWrapper* tab = Source<TabContentsWrapper>(source).ptr();
      RenderViewHost* host = tab->render_view_host();
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(
              ExtensionTabIdMap::GetInstance(),
              &ExtensionTabIdMap::SetTabAndWindowId,
              host->process()->id(), host->routing_id(),
              tab->restore_tab_helper()->session_id().id(),
              tab->restore_tab_helper()->window_id().id()));
      break;
    }
    case content::NOTIFICATION_RENDER_VIEW_HOST_DELETED: {
      RenderViewHost* host = Source<RenderViewHost>(source).ptr();
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(
              ExtensionTabIdMap::GetInstance(),
              &ExtensionTabIdMap::ClearTabAndWindowId,
              host->process()->id(), host->routing_id()));
      break;
    }
    default:
      NOTREACHED();
      return;
  }
}

//
// ExtensionTabIdMap
//

ExtensionTabIdMap::ExtensionTabIdMap() : observer_(NULL) {
}

ExtensionTabIdMap::~ExtensionTabIdMap() {
}

// static
ExtensionTabIdMap* ExtensionTabIdMap::GetInstance() {
  return Singleton<ExtensionTabIdMap>::get();
}

void ExtensionTabIdMap::Init() {
  observer_ = new TabObserver;
}

void ExtensionTabIdMap::Shutdown() {
  delete observer_;
}

void ExtensionTabIdMap::SetTabAndWindowId(
    int render_process_host_id, int routing_id, int tab_id, int window_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  RenderId render_id(render_process_host_id, routing_id);
  map_[render_id] = TabAndWindowId(tab_id, window_id);
}

void ExtensionTabIdMap::ClearTabAndWindowId(
    int render_process_host_id, int routing_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  RenderId render_id(render_process_host_id, routing_id);
  map_.erase(render_id);
}

bool ExtensionTabIdMap::GetTabAndWindowId(
    int render_process_host_id, int routing_id, int* tab_id, int* window_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  RenderId render_id(render_process_host_id, routing_id);
  TabAndWindowIdMap::iterator iter = map_.find(render_id);
  if (iter != map_.end()) {
    *tab_id = iter->second.first;
    *window_id = iter->second.second;
    return true;
  }
  return false;
}
