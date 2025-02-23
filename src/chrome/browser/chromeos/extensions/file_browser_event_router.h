// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_EVENT_ROUTER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_EVENT_ROUTER_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/files/file_path_watcher.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/cros/mount_library.h"

class FileBrowserNotifications;
class Profile;

// Used to monitor disk mount changes and signal when new mounted usb device is
// found.
class ExtensionFileBrowserEventRouter
    : public chromeos::MountLibrary::Observer {
 public:
  explicit ExtensionFileBrowserEventRouter(Profile* profile);
  virtual ~ExtensionFileBrowserEventRouter();
  // Starts observing file system change events. Currently only
  // MountLibrary events are being observed.
  void ObserveFileSystemEvents();

  // File watch setup routines.
  bool AddFileWatch(const FilePath& file_path,
                    const FilePath& virtual_path,
                    const std::string& extension_id);
  void RemoveFileWatch(const FilePath& file_path,
                       const std::string& extension_id);

  // MountLibrary::Observer overrides.
  virtual void DiskChanged(chromeos::MountLibraryEventType event,
                           const chromeos::MountLibrary::Disk* disk) OVERRIDE;
  virtual void DeviceChanged(chromeos::MountLibraryEventType event,
                             const std::string& device_path) OVERRIDE;
  virtual void MountCompleted(chromeos::MountLibrary::MountEvent event_type,
      chromeos::MountError error_code,
      const chromeos::MountLibrary::MountPointInfo& mount_info) OVERRIDE;

 private:
  typedef struct FileWatcherExtensions {
    FileWatcherExtensions(const FilePath& path,
                          const std::string& extension_id) {
      file_watcher.reset(new base::files::FilePathWatcher());
      virtual_path = path;
      extensions.insert(extension_id);
    }
    ~FileWatcherExtensions() {}
    linked_ptr<base::files::FilePathWatcher> file_watcher;
    FilePath local_path;
    FilePath virtual_path;
    std::set<std::string> extensions;
  } FileWatcherProcess;
  typedef std::map<FilePath, FileWatcherExtensions*> WatcherMap;

  // Helper class for passing through file watch notification events.
  class FileWatcherDelegate : public base::files::FilePathWatcher::Delegate {
   public:
    explicit FileWatcherDelegate(ExtensionFileBrowserEventRouter* router);

   private:
    // base::files::FilePathWatcher::Delegate overrides.
    virtual void OnFilePathChanged(const FilePath& path) OVERRIDE;
    virtual void OnFilePathError(const FilePath& path) OVERRIDE;

    void HandleFileWatchOnUIThread(const FilePath& local_path, bool got_error);

    ExtensionFileBrowserEventRouter* router_;
  };

  // USB mount event handlers.
  void OnDiskAdded(const chromeos::MountLibrary::Disk* disk);
  void OnDiskRemoved(const chromeos::MountLibrary::Disk* disk);
  void OnDiskMounted(const chromeos::MountLibrary::Disk* disk);
  void OnDiskUnmounted(const chromeos::MountLibrary::Disk* disk);
  void OnDeviceAdded(const std::string& device_path);
  void OnDeviceRemoved(const std::string& device_path);
  void OnDeviceScanned(const std::string& device_path);
  void OnFormattingStarted(const std::string& device_path, bool success);
  void OnFormattingFinished(const std::string& device_path, bool success);

  // Process file watch notifications.
  void HandleFileWatchNotification(const FilePath& path,
                                   bool got_error);

  // Sends folder change event.
  void DispatchFolderChangeEvent(const FilePath& path, bool error,
                                 const std::set<std::string>& extensions);

  // Sends filesystem changed extension message to all renderers.
  void DispatchDiskEvent(const chromeos::MountLibrary::Disk* disk, bool added);

  void DispatchMountCompletedEvent(chromeos::MountLibrary::MountEvent event,
      chromeos::MountError error_code,
      const chromeos::MountLibrary::MountPointInfo& mount_info);

  void RemoveBrowserFromVector(const std::string& path);

  // Used to create a window of a standard size, and add it to a list
  // of tracked browser windows in case that device goes away.
  void OpenFileBrowse(const std::string& url,
                      const std::string& device_path,
                      bool small);

  scoped_refptr<FileWatcherDelegate> delegate_;
  WatcherMap file_watchers_;
  scoped_ptr<FileBrowserNotifications> notifications_;
  Profile* profile_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionFileBrowserEventRouter);
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_BROWSER_EVENT_ROUTER_H_
