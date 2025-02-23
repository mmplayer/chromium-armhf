// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_INSTANCE_H_
#define CONTENT_BROWSER_BROWSING_INSTANCE_H_
#pragma once

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

class GURL;
class SiteInstance;

namespace content {
class BrowserContext;
}

///////////////////////////////////////////////////////////////////////////////
//
// BrowsingInstance class
//
// A browsing instance corresponds to the notion of a "unit of related browsing
// contexts" in the HTML 5 spec.  Intuitively, it represents a collection of
// tabs and frames that can have script connections to each other.  In that
// sense, it reflects the user interface, and not the contents of the tabs and
// frames.
//
// We further subdivide a BrowsingInstance into SiteInstances, which represent
// the documents within each BrowsingInstance that are from the same site and
// thus can have script access to each other.  Different SiteInstances can
// safely run in different processes, because their documents cannot access
// each other's contents (due to the same origin policy).
//
// It is important to only have one SiteInstance per site within a given
// BrowsingInstance.  This is because any two documents from the same site
// might be able to script each other if they are in the same BrowsingInstance.
// Thus, they must be rendered in the same process.
//
// If the process-per-site model is in use, then we ensure that there is only
// one SiteInstance per site for the entire browser context, not just for each
// BrowsingInstance.  This reduces the number of renderer processes we create.
// (This is currently only true if --process-per-site is specified at the
// command line.)
//
// A BrowsingInstance is live as long as any SiteInstance has a reference to
// it.  A SiteInstance is live as long as any NavigationEntry or RenderViewHost
// have references to it.  Because both classes are RefCounted, they do not
// need to be manually deleted.
//
// Currently, the BrowsingInstance class is not visible outside of the
// SiteInstance class.  To get a new SiteInstance that is part of the same
// BrowsingInstance, use SiteInstance::GetRelatedSiteInstance.  Because of
// this, BrowsingInstances and SiteInstances are tested together in
// site_instance_unittest.cc.
//
///////////////////////////////////////////////////////////////////////////////
class CONTENT_EXPORT BrowsingInstance
    : public base::RefCounted<BrowsingInstance> {
 public:
  // Create a new BrowsingInstance.
  explicit BrowsingInstance(content::BrowserContext* context);

  // Returns whether the process-per-site model is in use (globally or just for
  // the given url), in which case we should ensure there is only one
  // SiteInstance per site for the entire browser context, not just for this
  // BrowsingInstance.
  virtual bool ShouldUseProcessPerSite(const GURL& url);

  // Get the browser context to which this BrowsingInstance belongs.
  content::BrowserContext* browser_context() { return browser_context_; }

  // Returns whether this BrowsingInstance has registered a SiteInstance for
  // the site of the given URL.
  bool HasSiteInstance(const GURL& url);

  // Get the SiteInstance responsible for rendering the given URL.  Should
  // create a new one if necessary, but should not create more than one
  // SiteInstance per site.
  SiteInstance* GetSiteInstanceForURL(const GURL& url);

  // Adds the given SiteInstance to our map, to ensure that we do not create
  // another SiteInstance for the same site.
  void RegisterSiteInstance(SiteInstance* site_instance);

  // Removes the given SiteInstance from our map, after all references to it
  // have been deleted.  This means it is safe to create a new SiteInstance
  // if the user later visits a page from this site, within this
  // BrowsingInstance.
  void UnregisterSiteInstance(SiteInstance* site_instance);

 protected:
  friend class base::RefCounted<BrowsingInstance>;

  // Virtual to allow tests to extend it.
  virtual ~BrowsingInstance();

 private:
  // Map of site to SiteInstance, to ensure we only have one SiteInstance per
  typedef base::hash_map<std::string, SiteInstance*> SiteInstanceMap;

  // Map of BrowserContext to SiteInstanceMap, for use in the process-per-site
  // model.
  typedef base::hash_map<content::BrowserContext*, SiteInstanceMap>
      ContextSiteInstanceMap;

  // Returns a pointer to the relevant SiteInstanceMap for this object.  If the
  // process-per-site model is in use, or if process-per-site-instance is in
  // use and |url| matches a site for which we always use one process (e.g.,
  // the new tab page), then this returns the SiteInstanceMap for the entire
  // browser context.  If not, this returns the BrowsingInstance's own private
  // SiteInstanceMap.
  SiteInstanceMap* GetSiteInstanceMap(content::BrowserContext* browser_context,
                                      const GURL& url);

  // Utility routine which removes the passed SiteInstance from the passed
  // SiteInstanceMap.
  bool RemoveSiteInstanceFromMap(SiteInstanceMap* map, const std::string& site,
                                 SiteInstance* site_instance);

  // Common browser context to which all SiteInstances in this BrowsingInstance
  // must belong.
  content::BrowserContext* const browser_context_;

  // Map of site to SiteInstance, to ensure we only have one SiteInstance per
  // site.  The site string should be the possibly_invalid_spec() of a GURL
  // obtained with SiteInstance::GetSiteForURL.  Note that this map may not
  // contain every active SiteInstance, because a race exists where two
  // SiteInstances can be assigned to the same site.  This is ok in rare cases.
  // This field is only used if we are not using process-per-site.
  SiteInstanceMap site_instance_map_;

  // Global map of BrowserContext to SiteInstanceMap, for process-per-site.
  static ContextSiteInstanceMap context_site_instance_map_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingInstance);
};

#endif  // CONTENT_BROWSER_BROWSING_INSTANCE_H_
