// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_tab_controller.h"

#include <vector>

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_plugin_service_filter.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/print_preview_ui.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "webkit/plugins/webplugininfo.h"

namespace {

void EnableInternalPDFPluginForTab(TabContentsWrapper* preview_tab) {
  // Always enable the internal PDF plugin for the print preview page.
  ChromePluginServiceFilter::GetInstance()->OverridePluginForTab(
        preview_tab->render_view_host()->process()->id(),
        preview_tab->render_view_host()->routing_id(),
        GURL(),
        ASCIIToUTF16(chrome::ChromeContentClient::kPDFPluginName));
}

void ResetPreviewTabOverrideTitle(TabContentsWrapper* preview_tab) {
  preview_tab->print_view_manager()->ResetTitleOverride();
}

}  // namespace

namespace printing {

PrintPreviewTabController::PrintPreviewTabController()
    : waiting_for_new_preview_page_(false) {
}

PrintPreviewTabController::~PrintPreviewTabController() {}

// static
PrintPreviewTabController* PrintPreviewTabController::GetInstance() {
  if (!g_browser_process)
    return NULL;
  return g_browser_process->print_preview_tab_controller();
}

// static
void PrintPreviewTabController::PrintPreview(TabContentsWrapper* tab) {
  if (tab->tab_contents()->showing_interstitial_page())
    return;

  PrintPreviewTabController* tab_controller = GetInstance();
  if (!tab_controller)
    return;
  tab_controller->GetOrCreatePreviewTab(tab);
}

TabContentsWrapper* PrintPreviewTabController::GetOrCreatePreviewTab(
    TabContentsWrapper* initiator_tab) {
  DCHECK(initiator_tab);

  // Get the print preview tab for |initiator_tab|.
  TabContentsWrapper* preview_tab = GetPrintPreviewForTab(initiator_tab);
  if (!preview_tab)
    return CreatePrintPreviewTab(initiator_tab);

  // Show current preview tab.
  static_cast<RenderViewHostDelegate*>(preview_tab->tab_contents())->Activate();
  return preview_tab;
}

TabContentsWrapper* PrintPreviewTabController::GetPrintPreviewForTab(
    TabContentsWrapper* tab) const {
  // |preview_tab_map_| is keyed by the preview tab, so if find() succeeds, then
  // |tab| is the preview tab.
  PrintPreviewTabMap::const_iterator it = preview_tab_map_.find(tab);
  if (it != preview_tab_map_.end())
    return tab;

  for (it = preview_tab_map_.begin(); it != preview_tab_map_.end(); ++it) {
    // If |tab| is an initiator tab.
    if (tab == it->second) {
      // Return the associated preview tab.
      return it->first;
    }
  }
  return NULL;
}

void PrintPreviewTabController::Observe(int type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      OnRendererProcessClosed(Source<RenderProcessHost>(source).ptr());
      break;
    }
    case content::NOTIFICATION_TAB_CONTENTS_DESTROYED: {
      TabContents* tab = Source<TabContents>(source).ptr();
      TabContentsWrapper* wrapper =
          TabContentsWrapper::GetCurrentWrapperForContents(tab);
      OnTabContentsDestroyed(wrapper);
      break;
    }
    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      NavigationController* controller =
          Source<NavigationController>(source).ptr();
      TabContentsWrapper* wrapper =
          TabContentsWrapper::GetCurrentWrapperForContents(
              controller->tab_contents());
      content::LoadCommittedDetails* load_details =
          Details<content::LoadCommittedDetails>(details).ptr();
      OnNavEntryCommitted(wrapper, load_details);
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void PrintPreviewTabController::OnRendererProcessClosed(
    RenderProcessHost* rph) {
  for (PrintPreviewTabMap::iterator iter = preview_tab_map_.begin();
       iter != preview_tab_map_.end(); ++iter) {
    TabContentsWrapper* initiator_tab = iter->second;
    if (initiator_tab &&
        initiator_tab->render_view_host()->process() == rph) {
      TabContentsWrapper* preview_tab = iter->first;
      PrintPreviewUI* print_preview_ui =
          static_cast<PrintPreviewUI*>(preview_tab->web_ui());
      print_preview_ui->OnInitiatorTabCrashed();
    }
  }
}

void PrintPreviewTabController::OnTabContentsDestroyed(
    TabContentsWrapper* tab) {
  TabContentsWrapper* preview_tab = GetPrintPreviewForTab(tab);
  if (!preview_tab)
    return;

  if (tab == preview_tab) {
    // Remove the initiator tab's observers before erasing the mapping.
    TabContentsWrapper* initiator_tab = GetInitiatorTab(tab);
    if (initiator_tab)
      RemoveObservers(initiator_tab);

    // Print preview tab contents are destroyed. Notify |PrintPreviewUI| to
    // abort the initiator tab preview request.
    if (IsPrintPreviewTab(tab) && tab->web_ui()) {
      PrintPreviewUI* print_preview_ui =
          static_cast<PrintPreviewUI*>(tab->web_ui());
      print_preview_ui->OnTabDestroyed();
    }

    // Erase the map entry.
    preview_tab_map_.erase(tab);
  } else {
    // Initiator tab is closed. Disable the controls in preview tab.
    if (preview_tab->web_ui()) {
      PrintPreviewUI* print_preview_ui =
          static_cast<PrintPreviewUI*>(preview_tab->web_ui());
      print_preview_ui->OnInitiatorTabClosed();
    }

    // |tab| is an initiator tab, update the map entry and remove observers.
    preview_tab_map_[preview_tab] = NULL;
  }

  ResetPreviewTabOverrideTitle(preview_tab);
  RemoveObservers(tab);
}

void PrintPreviewTabController::OnNavEntryCommitted(
    TabContentsWrapper* tab, content::LoadCommittedDetails* details) {
  TabContentsWrapper* preview_tab = GetPrintPreviewForTab(tab);
  bool source_tab_is_preview_tab = (tab == preview_tab);
  if (details) {
    content::PageTransition transition_type = details->entry->transition_type();
    content::NavigationType nav_type = details->type;

    // Don't update/erase the map entry if the page has not changed.
    if (transition_type == content::PAGE_TRANSITION_RELOAD ||
        nav_type == content::NAVIGATION_TYPE_SAME_PAGE) {
      if (source_tab_is_preview_tab)
        SetInitiatorTabURLAndTitle(preview_tab);
      return;
    }

    // New |preview_tab| is created. Don't update/erase map entry.
    if (waiting_for_new_preview_page_ &&
        transition_type == content::PAGE_TRANSITION_LINK &&
        nav_type == content::NAVIGATION_TYPE_NEW_PAGE &&
        source_tab_is_preview_tab) {
      waiting_for_new_preview_page_ = false;
      SetInitiatorTabURLAndTitle(preview_tab);
      return;
    }

    // User navigated to a preview tab using forward/back button.
    if (source_tab_is_preview_tab &&
        transition_type == content::PAGE_TRANSITION_FORWARD_BACK &&
        nav_type == content::NAVIGATION_TYPE_EXISTING_PAGE) {
      return;
    }
  }

  RemoveObservers(tab);
  ResetPreviewTabOverrideTitle(preview_tab);
  if (source_tab_is_preview_tab) {
    // Remove the initiator tab's observers before erasing the mapping.
    TabContentsWrapper* initiator_tab = GetInitiatorTab(tab);
    if (initiator_tab)
      RemoveObservers(initiator_tab);
    preview_tab_map_.erase(tab);
  } else {
    preview_tab_map_[preview_tab] = NULL;

    // Initiator tab is closed. Disable the controls in preview tab.
    if (preview_tab->web_ui()) {
      PrintPreviewUI* print_preview_ui =
          static_cast<PrintPreviewUI*>(preview_tab->web_ui());
      print_preview_ui->OnInitiatorTabClosed();
    }
  }
}

// static
bool PrintPreviewTabController::IsPrintPreviewTab(TabContentsWrapper* tab) {
  return IsPrintPreviewURL(tab->tab_contents()->GetURL());
}

// static
bool PrintPreviewTabController::IsPrintPreviewURL(const GURL& url) {
  return (url.SchemeIs(chrome::kChromeUIScheme) &&
          url.host() == chrome::kChromeUIPrintHost);
}

void PrintPreviewTabController::EraseInitiatorTabInfo(
    TabContentsWrapper* preview_tab) {
  PrintPreviewTabMap::iterator it = preview_tab_map_.find(preview_tab);
  if (it == preview_tab_map_.end())
    return;

  RemoveObservers(it->second);
  preview_tab_map_[preview_tab] = NULL;
  ResetPreviewTabOverrideTitle(preview_tab);
}

TabContentsWrapper* PrintPreviewTabController::GetInitiatorTab(
    TabContentsWrapper* preview_tab) {
  PrintPreviewTabMap::iterator it = preview_tab_map_.find(preview_tab);
  if (it != preview_tab_map_.end())
    return preview_tab_map_[preview_tab];
  return NULL;
}

TabContentsWrapper* PrintPreviewTabController::CreatePrintPreviewTab(
    TabContentsWrapper* initiator_tab) {
  Browser* current_browser = BrowserList::FindBrowserWithID(
      initiator_tab->restore_tab_helper()->window_id().id());
  if (!current_browser) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeFrame)) {
      Profile* profile = Profile::FromBrowserContext(
          initiator_tab->tab_contents()->browser_context());
      current_browser = Browser::CreateForType(Browser::TYPE_POPUP, profile);
      if (!current_browser) {
        NOTREACHED() << "Failed to create popup browser window";
        return NULL;
      }
    } else {
      return NULL;
    }
  }

  // Add a new tab next to initiator tab.
  browser::NavigateParams params(current_browser,
                                 GURL(chrome::kChromeUIPrintURL),
                                 content::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeFrame))
    params.disposition = NEW_POPUP;

  // For normal tabs, set the position as immediately to the right,
  // otherwise let the tab strip decide.
  if (current_browser->is_type_tabbed()) {
    params.tabstrip_index = current_browser->tabstrip_model()->
        GetIndexOfTabContents(initiator_tab) + 1;
  }

  browser::Navigate(&params);
  TabContentsWrapper* preview_tab = params.target_contents;
  EnableInternalPDFPluginForTab(preview_tab);
  static_cast<RenderViewHostDelegate*>(preview_tab->tab_contents())->Activate();

  // Add an entry to the map.
  preview_tab_map_[preview_tab] = initiator_tab;
  waiting_for_new_preview_page_ = true;

  AddObservers(initiator_tab);
  AddObservers(preview_tab);

  return preview_tab;
}

void PrintPreviewTabController::SetInitiatorTabURLAndTitle(
    TabContentsWrapper* preview_tab) {
  TabContentsWrapper* initiator_tab = GetInitiatorTab(preview_tab);
  if (initiator_tab && preview_tab->web_ui()) {
    PrintPreviewUI* print_preview_ui =
        static_cast<PrintPreviewUI*>(preview_tab->web_ui());
    print_preview_ui->SetInitiatorTabURLAndTitle(
        initiator_tab->tab_contents()->GetURL().spec(),
        initiator_tab->print_view_manager()->RenderSourceName());
  }
}

void PrintPreviewTabController::AddObservers(TabContentsWrapper* tab) {
  TabContents* contents = tab->tab_contents();
  registrar_.Add(this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(contents));
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 Source<NavigationController>(&contents->controller()));

  // Multiple sites may share the same RenderProcessHost, so check if this
  // notification has already been added.
  RenderProcessHost* rph = tab->render_view_host()->process();
  if (!registrar_.IsRegistered(this,
                               content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                               Source<RenderProcessHost>(rph))) {
    registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                   Source<RenderProcessHost>(rph));
  }
}

void PrintPreviewTabController::RemoveObservers(TabContentsWrapper* tab) {
  TabContents* contents = tab->tab_contents();
  registrar_.Remove(this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                    Source<TabContents>(contents));
  registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                    Source<NavigationController>(&contents->controller()));

  // Multiple sites may share the same RenderProcessHost, so check if this
  // notification has already been added.
  RenderProcessHost* rph = tab->render_view_host()->process();
  if (registrar_.IsRegistered(this,
                              content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                              Source<RenderProcessHost>(rph))) {
    registrar_.Remove(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                      Source<RenderProcessHost>(rph));
  }
}

}  // namespace printing
