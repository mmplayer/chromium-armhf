// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_MOUNT_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_MOUNT_LIBRARY_H_
#pragma once

#include <string>

#include "base/observer_list.h"
#include "base/time.h"
#include "chrome/browser/chromeos/cros/mount_library.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros/chromeos_mount.h"

namespace chromeos {

class MockMountLibrary : public MountLibrary {
 public:
  MockMountLibrary();
  virtual ~MockMountLibrary();

  MOCK_METHOD0(Init, void(void));
  MOCK_METHOD1(AddObserver, void(MountLibrary::Observer*));
  MOCK_METHOD1(RemoveObserver, void(MountLibrary::Observer*));
  MOCK_CONST_METHOD0(disks, const MountLibrary::DiskMap&(void));
  MOCK_CONST_METHOD0(mount_points, const MountLibrary::MountPointMap&(void));

  MOCK_METHOD0(RequestMountInfoRefresh, void(void));
  MOCK_METHOD3(MountPath, void(const char*, MountType,
                               const MountPathOptions&));
  MOCK_METHOD1(UnmountPath, void(const char*));
  MOCK_METHOD3(GetSizeStatsOnFileThread, void(const char*, size_t*, size_t*));
  MOCK_METHOD1(FormatUnmountedDevice, void(const char*));
  MOCK_METHOD1(FormatMountedDevice, void(const char*));
  MOCK_METHOD3(UnmountDeviceRecursive, void(const char*,
      MountLibrary::UnmountDeviceRecursiveCallbackType, void*));

  void FireDeviceInsertEvents();
  void FireDeviceRemoveEvents();

  void SetupDefaultReplies();

 private:
  void AddObserverInternal(MountLibrary::Observer* observer);
  void RemoveObserverInternal(MountLibrary::Observer* observer);
  const MountLibrary::DiskMap& disksInternal() const { return disks_; }


  void UpdateDeviceChanged(MountLibraryEventType evt,
                           const std::string& path);
  void UpdateDiskChanged(MountLibraryEventType evt,
                         const MountLibrary::Disk* disk);

  ObserverList<MountLibrary::Observer> observers_;

  // The list of disks found.
  MountLibrary::DiskMap disks_;

  DISALLOW_COPY_AND_ASSIGN(MockMountLibrary);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_MOUNT_LIBRARY_H_
