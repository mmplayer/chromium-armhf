// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_RESOURCE_TRACKER_H__
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_RESOURCE_TRACKER_H__
#pragma once

#include <map>

#include "base/basictypes.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "ipc/ipc_message.h"

// Template trick so that AutomationResourceTracker can be used with non-pointer
// types.
template <class T>
struct AutomationResourceTraits {
  typedef T ValueType;
};

template <class T>
struct AutomationResourceTraits<T*> {
  typedef T ValueType;
};

// This class exists for the sole purpose of allowing some of the implementation
// of AutomationResourceTracker to live in a .cc file.
class AutomationResourceTrackerImpl {
 public:
  explicit AutomationResourceTrackerImpl(IPC::Message::Sender* sender);
  virtual ~AutomationResourceTrackerImpl();

 protected:
  // These need to be implemented in AutomationResourceTracker,
  // since it needs to call the subclass's type-specific notification
  // registration functions.
  virtual void AddObserverTypeProxy(const void* resource) = 0;
  virtual void RemoveObserverTypeProxy(const void* resource) = 0;

  int AddImpl(const void* resource);
  void RemoveImpl(const void* resource);
  int GenerateHandle();
  bool ContainsResourceImpl(const void* resource);
  bool ContainsHandleImpl(int handle);
  const void* GetResourceImpl(int handle);
  int GetHandleImpl(const void* resource);
  void HandleCloseNotification(const void* resource);

 private:
  typedef std::map<const void*, int> ResourceToHandleMap;
  typedef std::map<int, const void*> HandleToResourceMap;

  ResourceToHandleMap resource_to_handle_;
  HandleToResourceMap handle_to_resource_;

  IPC::Message::Sender* sender_;

  DISALLOW_COPY_AND_ASSIGN(AutomationResourceTrackerImpl);
};

// This template defines a superclass for an object that wants to track
// a particular kind of application resource (like windows or tabs) for
// automation purposes.  The only things that a subclass should need to
// define are AddObserver and RemoveObserver for the given resource's
// close notifications.
template <class T>
class AutomationResourceTracker : public AutomationResourceTrackerImpl,
                                  public NotificationObserver {
 public:
  explicit AutomationResourceTracker(IPC::Message::Sender* automation)
    : AutomationResourceTrackerImpl(automation) {}

  // The implementations for these should call the NotificationService
  // to add and remove this object as an observer for the appropriate
  // resource closing notification.
  virtual void AddObserver(T resource) = 0;
  virtual void RemoveObserver(T resource) = 0;

  // Adds the given resource to this tracker, and returns a handle that
  // can be used to refer to that resource.  If the resource is already
  // being tracked, the handle may be the same as one returned previously.
  int Add(T resource) {
    return AddImpl(resource);
  }

  // Removes the given resource from this tracker.  If the resource is not
  // currently present in the tracker, this is a no-op.
  void Remove(T resource) {
    RemoveImpl(resource);
  }

  // Returns true if this tracker currently tracks the resource pointed to
  // by the parameter.
  bool ContainsResource(T resource) {
    return ContainsResourceImpl(resource);
  }

  // Returns true if this tracker currently tracks the given handle.
  bool ContainsHandle(int handle) {
    return ContainsHandleImpl(handle);
  }

  // Returns the resource pointer associated with a given handle, or NULL
  // if that handle is not present in the mapping.
  // The casts here allow this to compile with both T = Foo and T = const Foo.
  T GetResource(int handle) {
    return static_cast<T>(const_cast<void*>(GetResourceImpl(handle)));
  }

  // Returns the handle associated with a given resource pointer, or 0 if
  // the resource is not currently in the mapping.
  int GetHandle(T resource) {
    return GetHandleImpl(resource);
  }

  // NotificationObserver implementation--the only thing that this tracker
  // does in response to notifications is to tell the AutomationProxy
  // that the associated handle is now invalid.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
     T resource =
        Source<typename AutomationResourceTraits<T>::ValueType>(source).ptr();

     CloseResource(resource);
  }

 protected:
  // Removes |resource| from the tracker, and handles sending the close
  // notification back to the client. This typically should not be called
  // directly, unless there is no appropriate notification available
  // for the resource type.
  void CloseResource(T resource) {
    HandleCloseNotification(resource);
  }

  // These proxy calls from the base Impl class to the template's subclss.
  // The casts here allow this to compile with both T = Foo and T = const Foo.
  virtual void AddObserverTypeProxy(const void* resource) {
    AddObserver(static_cast<T>(const_cast<void*>(resource)));
  }
  virtual void RemoveObserverTypeProxy(const void* resource) {
    RemoveObserver(static_cast<T>(const_cast<void*>(resource)));
  }

  NotificationRegistrar registrar_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutomationResourceTracker);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_RESOURCE_TRACKER_H__
