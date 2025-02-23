// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/compiler_specific.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/tracker_base.h"

namespace ppapi {

namespace {

int mock_resource_alive_count = 0;
int last_plugin_ref_was_deleted_count = 0;
int instance_was_deleted_count = 0;

class MyMockResource : public Resource {
 public:
  MyMockResource(PP_Instance instance) : Resource(instance) {
    mock_resource_alive_count++;
  }
  virtual ~MyMockResource() {
    mock_resource_alive_count--;
  }

  virtual void LastPluginRefWasDeleted() OVERRIDE {
    Resource::LastPluginRefWasDeleted();
    last_plugin_ref_was_deleted_count++;
  }
  virtual void InstanceWasDeleted() OVERRIDE {
    Resource::InstanceWasDeleted();
    instance_was_deleted_count++;
  }
};

// Global singleton used by the TrackerBase.
TrackerBase* my_tracker_base = NULL;
TrackerBase* GetMyTrackerBase() {
  return my_tracker_base;
}

}  // namespace

class ResourceTrackerTest : public testing::Test, public TrackerBase {
 public:
  ResourceTrackerTest() {}

  // Test implementation.
  virtual void SetUp() OVERRIDE {
    my_tracker_base = this;
    TrackerBase::Init(&GetMyTrackerBase);

    ASSERT_EQ(0, mock_resource_alive_count);
    last_plugin_ref_was_deleted_count = 0;
    instance_was_deleted_count = 0;
  }
  virtual void TearDown() OVERRIDE {
    my_tracker_base = NULL;
    TrackerBase::Init(NULL);
  }

  // TrackerBase implementation.
  virtual FunctionGroupBase* GetFunctionAPI(
      PP_Instance inst,
      ppapi::proxy::InterfaceID id) OVERRIDE {
    return NULL;
  }
  virtual VarTracker* GetVarTracker() OVERRIDE {
    return NULL;
  }
  virtual ResourceTracker* GetResourceTracker() OVERRIDE {
    return &resource_tracker_;
  }
  virtual PP_Module GetModuleForInstance(PP_Instance /* instance */) OVERRIDE {
    return 0;
  }

  ResourceTracker& resource_tracker() { return resource_tracker_; }

 private:
  ResourceTracker resource_tracker_;
};

// Test that LastPluginRefWasDeleted is called when the last plugin ref was
// deleted but the object lives on.
TEST_F(ResourceTrackerTest, LastPluginRef) {
  PP_Instance instance = 0x1234567;
  resource_tracker().DidCreateInstance(instance);

  scoped_refptr<MyMockResource> resource(new MyMockResource(instance));
  PP_Resource pp_resource = resource->GetReference();
  EXPECT_TRUE(resource_tracker().GetResource(pp_resource));

  // Releasing it should keep the object (because we have a ref) but fire the
  // "last plugin ref" message.
  resource_tracker().ReleaseResource(pp_resource);
  EXPECT_EQ(1, last_plugin_ref_was_deleted_count);
  EXPECT_EQ(1, mock_resource_alive_count);

  resource_tracker().DidDeleteInstance(instance);
  resource = NULL;
  EXPECT_FALSE(resource_tracker().GetResource(pp_resource));
}

// Tests when the plugin is holding a ref to a resource when the instance is
// deleted.
TEST_F(ResourceTrackerTest, InstanceDeletedWithPluginRef) {
  // Make a resource with one ref held by the plugin, and delete the instance.
  PP_Instance instance = 0x2345678;
  resource_tracker().DidCreateInstance(instance);
  MyMockResource* resource = new MyMockResource(instance);
  resource->GetReference();
  EXPECT_EQ(1, mock_resource_alive_count);
  resource_tracker().DidDeleteInstance(instance);

  // The resource should have been deleted, and before it was, it should have
  // received a "last plugin ref was deleted" notification.
  EXPECT_EQ(0, mock_resource_alive_count);
  EXPECT_EQ(1, last_plugin_ref_was_deleted_count);
  EXPECT_EQ(0, instance_was_deleted_count);
}

// Test when the plugin and the internal implementation (via scoped_refptr) is
// holding a ref to a resource when the instance is deleted.
TEST_F(ResourceTrackerTest, InstanceDeletedWithBothRefed) {
  // Create a new instance.
  PP_Instance instance = 0x3456789;

  // Make a resource with one ref held by the plugin and one ref held by us
  // (outlives the plugin), and delete the instance.
  resource_tracker().DidCreateInstance(instance);
  scoped_refptr<MyMockResource> resource = new MyMockResource(instance);
  resource->GetReference();
  EXPECT_EQ(1, mock_resource_alive_count);
  resource_tracker().DidDeleteInstance(instance);

  // The resource should NOT have been deleted, and it should have received both
  // a "last plugin ref was deleted" and a "instance was deleted" notification.
  EXPECT_EQ(1, mock_resource_alive_count);
  EXPECT_EQ(1, last_plugin_ref_was_deleted_count);
  EXPECT_EQ(1, instance_was_deleted_count);
  EXPECT_EQ(0, resource->pp_instance());

  resource = NULL;
  EXPECT_EQ(0, mock_resource_alive_count);
}

}  // namespace ppapi
