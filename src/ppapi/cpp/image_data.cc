// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/image_data.h"

#include <string.h>  // Needed for memset.

#include <algorithm>

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_ImageData>() {
  return PPB_IMAGEDATA_INTERFACE;
}

}  // namespace

ImageData::ImageData() : data_(NULL) {
  memset(&desc_, 0, sizeof(PP_ImageDataDesc));
}

ImageData::ImageData(const ImageData& other)
    : Resource(other),
      desc_(other.desc_),
      data_(other.data_) {
}

ImageData::ImageData(PassRef, PP_Resource resource)
    : data_(NULL) {
  memset(&desc_, 0, sizeof(PP_ImageDataDesc));

  if (!has_interface<PPB_ImageData>())
    return;

  PassRefAndInitData(resource);
}

ImageData::ImageData(Instance* instance,
                     PP_ImageDataFormat format,
                     const Size& size,
                     bool init_to_zero)
    : data_(NULL) {
  memset(&desc_, 0, sizeof(PP_ImageDataDesc));

  if (!has_interface<PPB_ImageData>())
    return;

  PassRefAndInitData(get_interface<PPB_ImageData>()->Create(
      instance->pp_instance(), format, &size.pp_size(),
      PP_FromBool(init_to_zero)));
}

ImageData& ImageData::operator=(const ImageData& other) {
  Resource::operator=(other);
  desc_ = other.desc_;
  data_ = other.data_;
  return *this;
}

const uint32_t* ImageData::GetAddr32(const Point& coord) const {
  // Prefer evil const casts rather than evil code duplication.
  return const_cast<ImageData*>(this)->GetAddr32(coord);
}

uint32_t* ImageData::GetAddr32(const Point& coord) {
  // If we add more image format types that aren't 32-bit, we'd want to check
  // here and fail.
  return reinterpret_cast<uint32_t*>(
      &static_cast<char*>(data())[coord.y() * stride() + coord.x() * 4]);
}

// static
PP_ImageDataFormat ImageData::GetNativeImageDataFormat() {
  if (!has_interface<PPB_ImageData>())
    return PP_IMAGEDATAFORMAT_BGRA_PREMUL;  // Default to something on failure.
  return get_interface<PPB_ImageData>()->GetNativeImageDataFormat();
}

void ImageData::PassRefAndInitData(PP_Resource resource) {
  PassRefFromConstructor(resource);
  if (!get_interface<PPB_ImageData>()->Describe(pp_resource(), &desc_) ||
      !(data_ = get_interface<PPB_ImageData>()->Map(pp_resource())))
    *this = ImageData();
}

}  // namespace pp
