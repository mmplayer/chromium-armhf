/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPB_MEMORY_DEV_H_
#define PPAPI_C_DEV_PPB_MEMORY_DEV_H_

#include "ppapi/c/pp_stdint.h"

#define PPB_MEMORY_DEV_INTERFACE_0_1 "PPB_Memory(Dev);0.1"
#define PPB_MEMORY_DEV_INTERFACE PPB_MEMORY_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the PPB_Memory interface defined by the browser and
 * and containing pointers to functions related to memory management.
 */

/**
 * @addtogroup Interfaces
 * @{
 */

/**
 * The PPB_Memory_Dev interface contains pointers to functions related to memory
 * management.
 *
 */
struct PPB_Memory_Dev {
  /**
   * MemAlloc is a pointer to a function that allocate memory.
   *
   * @param[in] num_bytes A number of bytes to allocate.
   * @return A pointer to the memory if successful, NULL If the
   * allocation fails.
   */
  void* (*MemAlloc)(uint32_t num_bytes);

  /**
   * MemFree is a pointer to a function that deallocates memory.
   *
   * @param[in] ptr A pointer to the memory to deallocate. It is safe to
   * pass NULL to this function.
   */
  void (*MemFree)(void* ptr);
};
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_MEMORY_DEV_H_ */
