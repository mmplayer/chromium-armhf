// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_instance.h"

#include "content/browser/browsing_instance.h"
#include "content/browser/content_browser_client.h"
#include "content/browser/renderer_host/browser_render_process_host.h"
#include "content/browser/webui/web_ui_factory.h"
#include "content/common/content_client.h"
#include "content/common/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/url_constants.h"
#include "net/base/registry_controlled_domain.h"

static bool IsURLSameAsAnySiteInstance(const GURL& url) {
  if (!url.is_valid())
    return false;

  // We treat javascript: as the same site as any URL since it is actually
  // a modifier on existing pages.
  if (url.SchemeIs(chrome::kJavaScriptScheme))
    return true;

  return
      content::GetContentClient()->browser()->IsURLSameAsAnySiteInstance(url);
}

int32 SiteInstance::next_site_instance_id_ = 1;

SiteInstance::SiteInstance(BrowsingInstance* browsing_instance)
    : id_(next_site_instance_id_++),
      browsing_instance_(browsing_instance),
      render_process_host_factory_(NULL),
      process_(NULL),
      max_page_id_(-1),
      has_site_(false) {
  DCHECK(browsing_instance);

  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllBrowserContextsAndSources());
}

SiteInstance::~SiteInstance() {
  NotificationService::current()->Notify(
      content::NOTIFICATION_SITE_INSTANCE_DELETED,
      Source<SiteInstance>(this),
      NotificationService::NoDetails());

  // Now that no one is referencing us, we can safely remove ourselves from
  // the BrowsingInstance.  Any future visits to a page from this site
  // (within the same BrowsingInstance) can safely create a new SiteInstance.
  if (has_site_)
    browsing_instance_->UnregisterSiteInstance(this);
}

bool SiteInstance::HasProcess() const {
  return (process_ != NULL);
}

RenderProcessHost* SiteInstance::GetProcess() {
  // TODO(erikkay) It would be nice to ensure that the renderer type had been
  // properly set before we get here.  The default tab creation case winds up
  // with no site set at this point, so it will default to TYPE_NORMAL.  This
  // may not be correct, so we'll wind up potentially creating a process that
  // we then throw away, or worse sharing a process with the wrong process type.
  // See crbug.com/43448.

  // Create a new process if ours went away or was reused.
  if (!process_) {
    // See if we should reuse an old process
    if (RenderProcessHost::ShouldTryToUseExistingProcessHost())
      process_ = RenderProcessHost::GetExistingProcessHost(
          browsing_instance_->browser_context(), site_);

    // Otherwise (or if that fails), create a new one.
    if (!process_) {
      if (render_process_host_factory_) {
        process_ = render_process_host_factory_->CreateRenderProcessHost(
            browsing_instance_->browser_context());
      } else {
        process_ =
            new BrowserRenderProcessHost(browsing_instance_->browser_context());
      }
    }

    // Make sure the process starts at the right max_page_id, and ensure that
    // we send an update to the renderer process.
    process_->UpdateAndSendMaxPageID(max_page_id_);
  }
  DCHECK(process_);

  return process_;
}

void SiteInstance::SetSite(const GURL& url) {
  // A SiteInstance's site should not change.
  // TODO(creis): When following links or script navigations, we can currently
  // render pages from other sites in this SiteInstance.  This will eventually
  // be fixed, but until then, we should still not set the site of a
  // SiteInstance more than once.
  DCHECK(!has_site_);

  // Remember that this SiteInstance has been used to load a URL, even if the
  // URL is invalid.
  has_site_ = true;
  site_ = GetSiteForURL(browsing_instance_->browser_context(), url);

  // Now that we have a site, register it with the BrowsingInstance.  This
  // ensures that we won't create another SiteInstance for this site within
  // the same BrowsingInstance, because all same-site pages within a
  // BrowsingInstance can script each other.
  browsing_instance_->RegisterSiteInstance(this);
}

bool SiteInstance::HasRelatedSiteInstance(const GURL& url) {
  return browsing_instance_->HasSiteInstance(url);
}

SiteInstance* SiteInstance::GetRelatedSiteInstance(const GURL& url) {
  return browsing_instance_->GetSiteInstanceForURL(url);
}

bool SiteInstance::HasWrongProcessForURL(const GURL& url) const {
  // Having no process isn't a problem, since we'll assign it correctly.
  if (!HasProcess())
    return false;

  // If the site URL is an extension (e.g., for hosted apps) but the
  // process is not (or vice versa), make sure we notice and fix it.
  GURL site_url = GetSiteForURL(browsing_instance_->browser_context(), url);
  content::ContentBrowserClient* browser =
      content::GetContentClient()->browser();
  return !browser->IsSuitableHost(process_, site_url);
}

/*static*/
SiteInstance* SiteInstance::CreateSiteInstance(
    content::BrowserContext* browser_context) {
  return new SiteInstance(new BrowsingInstance(browser_context));
}

/*static*/
SiteInstance* SiteInstance::CreateSiteInstanceForURL(
    content::BrowserContext* browser_context, const GURL& url) {
  // This BrowsingInstance may be deleted if it returns an existing
  // SiteInstance.
  scoped_refptr<BrowsingInstance> instance(
      new BrowsingInstance(browser_context));
  return instance->GetSiteInstanceForURL(url);
}

/*static*/
GURL SiteInstance::GetSiteForURL(content::BrowserContext* browser_context,
                                 const GURL& real_url) {
  GURL url = GetEffectiveURL(browser_context, real_url);

  // URLs with no host should have an empty site.
  GURL site;

  // TODO(creis): For many protocols, we should just treat the scheme as the
  // site, since there is no host.  e.g., file:, about:, chrome:

  // If the url has a host, then determine the site.
  if (url.has_host()) {
    // Only keep the scheme and registered domain as given by GetOrigin.  This
    // may also include a port, which we need to drop.
    site = url.GetOrigin();

    // Remove port, if any.
    if (site.has_port()) {
      GURL::Replacements rep;
      rep.ClearPort();
      site = site.ReplaceComponents(rep);
    }

    // If this URL has a registered domain, we only want to remember that part.
    std::string domain =
        net::RegistryControlledDomainService::GetDomainAndRegistry(url);
    if (!domain.empty()) {
      GURL::Replacements rep;
      rep.SetHostStr(domain);
      site = site.ReplaceComponents(rep);
    }
  }
  return site;
}

/*static*/
bool SiteInstance::IsSameWebSite(content::BrowserContext* browser_context,
                                 const GURL& real_url1, const GURL& real_url2) {
  GURL url1 = GetEffectiveURL(browser_context, real_url1);
  GURL url2 = GetEffectiveURL(browser_context, real_url2);

  // We infer web site boundaries based on the registered domain name of the
  // top-level page and the scheme.  We do not pay attention to the port if
  // one is present, because pages served from different ports can still
  // access each other if they change their document.domain variable.

  // Some special URLs will match the site instance of any other URL. This is
  // done before checking both of them for validity, since we want these URLs
  // to have the same site instance as even an invalid one.
  if (IsURLSameAsAnySiteInstance(url1) || IsURLSameAsAnySiteInstance(url2))
    return true;

  // If either URL is invalid, they aren't part of the same site.
  if (!url1.is_valid() || !url2.is_valid())
    return false;

  // If the schemes differ, they aren't part of the same site.
  if (url1.scheme() != url2.scheme())
    return false;

  return net::RegistryControlledDomainService::SameDomainOrHost(url1, url2);
}

/*static*/
GURL SiteInstance::GetEffectiveURL(content::BrowserContext* browser_context,
                                   const GURL& url) {
  return content::GetContentClient()->browser()->
      GetEffectiveURL(browser_context, url);
}

void SiteInstance::Observe(int type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_RENDERER_PROCESS_TERMINATED);
  RenderProcessHost* rph = Source<RenderProcessHost>(source).ptr();
  if (rph == process_)
    process_ = NULL;
}
