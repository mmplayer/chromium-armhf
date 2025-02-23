// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/scoped_pp_resource.h"

#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/tracker_base.h"

namespace ppapi {

ScopedPPResource::ScopedPPResource() : id_(0) {
}

ScopedPPResource::ScopedPPResource(PP_Resource resource) : id_(resource) {
  CallAddRef();
}

ScopedPPResource::ScopedPPResource(const PassRef&, PP_Resource resource)
    : id_(resource) {
}

ScopedPPResource::ScopedPPResource(Resource* resource)
    : id_(resource ? resource->GetReference() : 0) {
  // GetReference AddRef's for us.
}

ScopedPPResource::ScopedPPResource(const ScopedPPResource& other)
    : id_(other.id_) {
  CallAddRef();
}

ScopedPPResource::~ScopedPPResource() {
  CallRelease();
}

ScopedPPResource& ScopedPPResource::operator=(PP_Resource resource) {
  if (id_ == resource)
    return *this;  // Be careful about self-assignment.
  CallRelease();
  id_ = resource;
  CallAddRef();
  return *this;
}

ScopedPPResource& ScopedPPResource::operator=(
    const ScopedPPResource& resource) {
  if (id_ == resource.id_)
    return *this;  // Be careful about self-assignment.
  CallRelease();
  id_ = resource.id_;
  CallAddRef();
  return *this;
}

PP_Resource ScopedPPResource::Release() {
  CallRelease();

  PP_Resource ret = id_;
  id_ = 0;
  return ret;
}

void ScopedPPResource::CallAddRef() {
  if (id_)
    TrackerBase::Get()->GetResourceTracker()->AddRefResource(id_);
}

void ScopedPPResource::CallRelease() {
  if (id_)
    TrackerBase::Get()->GetResourceTracker()->ReleaseResource(id_);
}

}  // namespace ppapi
