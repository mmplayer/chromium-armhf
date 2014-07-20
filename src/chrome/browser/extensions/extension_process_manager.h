// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESS_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESS_MANAGER_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/public/common/view_types.h"

class Browser;
class BrowsingInstance;
class Extension;
class ExtensionHost;
class GURL;
class Profile;
class RenderProcessHost;
class SiteInstance;

// Manages dynamic state of running Chromium extensions. There is one instance
// of this class per Profile. OTR Profiles have a separate instance that keeps
// track of split-mode extensions only.
class ExtensionProcessManager : public NotificationObserver {
 public:
  static ExtensionProcessManager* Create(Profile* profile);
  virtual ~ExtensionProcessManager();

  // Creates a new ExtensionHost with its associated view, grouping it in the
  // appropriate SiteInstance (and therefore process) based on the URL and
  // profile.
  virtual ExtensionHost* CreateViewHost(const Extension* extension,
                                        const GURL& url,
                                        Browser* browser,
                                        content::ViewType view_type);
  ExtensionHost* CreateViewHost(const GURL& url,
                                Browser* browser,
                                content::ViewType view_type);
  ExtensionHost* CreatePopupHost(const Extension* extension,
                                 const GURL& url,
                                 Browser* browser);
  ExtensionHost* CreatePopupHost(const GURL& url, Browser* browser);
  ExtensionHost* CreateDialogHost(const GURL& url, Browser* browser);
  ExtensionHost* CreateInfobarHost(const Extension* extension,
                                   const GURL& url,
                                   Browser* browser);
  ExtensionHost* CreateInfobarHost(const GURL& url,
                                   Browser* browser);

  // Open the extension's options page.
  void OpenOptionsPage(const Extension* extension, Browser* browser);

  // Creates a new UI-less extension instance.  Like CreateViewHost, but not
  // displayed anywhere.
  virtual void CreateBackgroundHost(const Extension* extension,
                                    const GURL& url);

  // Gets the ExtensionHost for the background page for an extension, or NULL if
  // the extension isn't running or doesn't have a background page.
  ExtensionHost* GetBackgroundHostForExtension(const Extension* extension);

  // Returns the SiteInstance that the given URL belongs to.
  virtual SiteInstance* GetSiteInstanceForURL(const GURL& url);

  // Registers a SiteInstance as hosting a given extension.
  void RegisterExtensionSiteInstance(SiteInstance* site_instance,
                                     const Extension* extension);

  // Unregisters the extension associated with |site_instance|.
  void UnregisterExtensionSiteInstance(SiteInstance* site_instance);

  // True if this process host is hosting an extension.
  bool IsExtensionProcess(int render_process_id);

  // True if this process host is hosting an extension with extension bindings
  // enabled.
  bool AreBindingsEnabledForProcess(int render_process_id);

  // True if this process host is restricted to only access isolated storage.
  bool IsStorageIsolatedForProcess(int host_id);

  // Returns the extension process that |url| is associated with if it exists.
  // This is not valid for hosted apps without the background permission, since
  // such apps may have multiple processes.
  virtual RenderProcessHost* GetExtensionProcess(const GURL& url);

  // Returns the process that the extension with the given ID is running in.
  RenderProcessHost* GetExtensionProcess(const std::string& extension_id);

  // Returns the Extension associated with the given SiteInstance id, if any.
  virtual const Extension* GetExtensionForSiteInstance(int site_instance_id);

  // Returns true if |host| is managed by this process manager.
  bool HasExtensionHost(ExtensionHost* host) const;

  typedef std::set<ExtensionHost*> ExtensionHostSet;
  typedef ExtensionHostSet::const_iterator const_iterator;
  const_iterator begin() const { return all_hosts_.begin(); }
  const_iterator end() const { return all_hosts_.end(); }

 protected:
  explicit ExtensionProcessManager(Profile* profile);

  // Called just after |host| is created so it can be registered in our lists.
  void OnExtensionHostCreated(ExtensionHost* host, bool is_background);

  // Called on browser shutdown to close our extension hosts.
  void CloseBackgroundHosts();

  // NotificationObserver:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  NotificationRegistrar registrar_;

  // The set of all ExtensionHosts managed by this process manager.
  ExtensionHostSet all_hosts_;

  // The set of running viewless background extensions.
  ExtensionHostSet background_hosts_;

  // The BrowsingInstance shared by all extensions in this profile.  This
  // controls process grouping.
  scoped_refptr<BrowsingInstance> browsing_instance_;

  // A map of site instance ID to the ID of the extension it hosts.
  typedef std::map<int, std::string> SiteInstanceIDMap;
  SiteInstanceIDMap extension_ids_;

  // A map of process ID to site instance ID of the site instances it hosts.
  typedef std::map<int, std::set<int> > ProcessIDMap;
  ProcessIDMap process_ids_;

  // A set of all process IDs that are designated for hosting webapps with
  // isolated storage.
  typedef std::set<int> ProcessIDSet;
  ProcessIDSet isolated_storage_process_ids_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionProcessManager);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESS_MANAGER_H_
