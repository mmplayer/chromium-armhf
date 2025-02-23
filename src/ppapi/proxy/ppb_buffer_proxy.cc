// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_buffer_proxy.h"

#include <vector>

#include "base/logging.h"
#include "build/build_config.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_buffer_trusted_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

Buffer::Buffer(const HostResource& resource,
               const base::SharedMemoryHandle& shm_handle,
               uint32_t size)
    : Resource(resource),
      shm_(shm_handle, false),
      size_(size),
      mapped_data_(NULL),
      map_count_(0) {
}

Buffer::~Buffer() {
  Unmap();
}

thunk::PPB_Buffer_API* Buffer::AsPPB_Buffer_API() {
  return this;
}

PP_Bool Buffer::Describe(uint32_t* size_in_bytes) {
  *size_in_bytes = size_;
  return PP_TRUE;
}

PP_Bool Buffer::IsMapped() {
  return PP_FromBool(!!mapped_data_);
}

void* Buffer::Map() {
  if (map_count_++ == 0)
    shm_.Map(size_);
  return shm_.memory();
}

void Buffer::Unmap() {
  if (--map_count_ == 0)
    shm_.Unmap();
}

PPB_Buffer_Proxy::PPB_Buffer_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_Buffer_Proxy::~PPB_Buffer_Proxy() {
}

// static
PP_Resource PPB_Buffer_Proxy::CreateProxyResource(PP_Instance instance,
                                                  uint32_t size) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  HostResource result;
  base::SharedMemoryHandle shm_handle = base::SharedMemory::NULLHandle();
  dispatcher->Send(new PpapiHostMsg_PPBBuffer_Create(
      INTERFACE_ID_PPB_BUFFER, instance, size,
      &result, &shm_handle));
  if (result.is_null() || !base::SharedMemory::IsHandleValid(shm_handle))
    return 0;

  return AddProxyResource(result, shm_handle, size);
}

// static
PP_Resource PPB_Buffer_Proxy::AddProxyResource(
    const HostResource& resource,
    base::SharedMemoryHandle shm_handle,
    uint32_t size) {
  return (new Buffer(resource, shm_handle, size))->GetReference();
}

bool PPB_Buffer_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Buffer_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBBuffer_Create, OnMsgCreate)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

void PPB_Buffer_Proxy::OnMsgCreate(
    PP_Instance instance,
    uint32_t size,
    HostResource* result_resource,
    base::SharedMemoryHandle* result_shm_handle) {
  // Overwritten below on success.
  *result_shm_handle = base::SharedMemory::NULLHandle();
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;

  thunk::EnterResourceCreation enter(instance);
  if (enter.failed())
    return;
  PP_Resource local_buffer_resource = enter.functions()->CreateBuffer(instance,
                                                                      size);
  if (local_buffer_resource == 0)
    return;

  thunk::EnterResourceNoLock<thunk::PPB_BufferTrusted_API> trusted_buffer(
      local_buffer_resource, false);
  if (trusted_buffer.failed())
    return;
  int local_fd;
  if (trusted_buffer.object()->GetSharedMemory(&local_fd) != PP_OK)
    return;

  result_resource->SetHostResource(instance, local_buffer_resource);

  // TODO(piman/brettw): Change trusted interface to return a PP_FileHandle,
  // those casts are ugly.
  base::PlatformFile platform_file =
#if defined(OS_WIN)
      reinterpret_cast<HANDLE>(static_cast<intptr_t>(local_fd));
#elif defined(OS_POSIX)
      local_fd;
#else
  #error Not implemented.
#endif
  *result_shm_handle = dispatcher->ShareHandleWithRemote(platform_file, false);
}

}  // namespace proxy
}  // namespace ppapi
