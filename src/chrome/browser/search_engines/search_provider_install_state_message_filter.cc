// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/search_provider_install_state_message_filter.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/render_messages.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/common/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "googleurl/src/gurl.h"

SearchProviderInstallStateMessageFilter::
SearchProviderInstallStateMessageFilter(
    int render_process_id,
    Profile* profile)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
        reply_with_provider_install_state_factory_(this)),
      provider_data_(profile->GetWebDataService(Profile::EXPLICIT_ACCESS),
                     content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                     Source<RenderProcessHost>(
                         RenderProcessHost::FromID(render_process_id))),
      is_off_the_record_(profile->IsOffTheRecord()) {
  // This is initialized by BrowserRenderProcessHost. Do not add any non-trivial
  // initialization here. Instead do it lazily when required.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

SearchProviderInstallStateMessageFilter::
~SearchProviderInstallStateMessageFilter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

bool SearchProviderInstallStateMessageFilter::OnMessageReceived(
    const IPC::Message& message,
    bool* message_was_ok) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(SearchProviderInstallStateMessageFilter, message,
                           *message_was_ok)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        ChromeViewHostMsg_GetSearchProviderInstallState,
        OnMsgGetSearchProviderInstallState)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

search_provider::InstallState
SearchProviderInstallStateMessageFilter::GetSearchProviderInstallState(
    const GURL& page_location,
    const GURL& requested_host) {
  GURL requested_origin = requested_host.GetOrigin();

  // Do the security check before any others to avoid information leaks.
  if (page_location.GetOrigin() != requested_origin)
    return search_provider::DENIED;

  // In incognito mode, no search information is exposed. (This check must be
  // done after the security check or else a web site can detect that the
  // user is in incognito mode just by doing a cross origin request.)
  if (is_off_the_record_)
      return search_provider::NOT_INSTALLED;

  switch (provider_data_.GetInstallState(requested_origin)) {
    case SearchProviderInstallData::NOT_INSTALLED:
      return search_provider::NOT_INSTALLED;

    case SearchProviderInstallData::INSTALLED_BUT_NOT_DEFAULT:
      return search_provider::INSTALLED_BUT_NOT_DEFAULT;

    case SearchProviderInstallData::INSTALLED_AS_DEFAULT:
      return search_provider::INSTALLED_AS_DEFAULT;
  }

  NOTREACHED();
  return search_provider::NOT_INSTALLED;
}

void
SearchProviderInstallStateMessageFilter::OnMsgGetSearchProviderInstallState(
    const GURL& page_location,
    const GURL& requested_host,
    IPC::Message* reply_msg) {
  provider_data_.CallWhenLoaded(
      reply_with_provider_install_state_factory_.NewRunnableMethod(
          &SearchProviderInstallStateMessageFilter::
          ReplyWithProviderInstallState,
          page_location,
          requested_host,
          reply_msg));
}

void SearchProviderInstallStateMessageFilter::ReplyWithProviderInstallState(
    const GURL& page_location,
    const GURL& requested_host,
    IPC::Message* reply_msg) {
  DCHECK(reply_msg);
  search_provider::InstallState install_state =
      GetSearchProviderInstallState(page_location, requested_host);

  ChromeViewHostMsg_GetSearchProviderInstallState::WriteReplyParams(
      reply_msg,
      install_state);
  Send(reply_msg);
}
