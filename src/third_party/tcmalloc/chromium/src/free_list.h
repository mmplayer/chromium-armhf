// Copyright (c) 2011, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// --- 
// Author: Rebecca Shapiro <bxx@google.com>
//
// This file contains declarations of functions that implement doubly
// linked lists and definitions of functions that implement singly
// linked lists.  It also contains macros to tell the SizeMap class
// how much space a node in the freelist needs so that SizeMap can
// create large enough size classes.

#ifndef TCMALLOC_FREE_LIST_H_
#define TCMALLOC_FREE_LIST_H_

#include <stddef.h>
#include "internal_logging.h"  // For CRASH() macro.
#include "linked_list.h"

// Remove to enable singly linked lists (the default for open source tcmalloc).
#define TCMALLOC_USE_DOUBLYLINKED_FREELIST

namespace tcmalloc {

#if defined(TCMALLOC_USE_DOUBLYLINKED_FREELIST)

// size class information for common.h.
static const bool kSupportsDoublyLinkedList = true;

void *FL_Next(void *t);
void FL_Init(void *t);
void FL_Push(void **list, void *element);
void *FL_Pop(void **list);
void FL_PopRange(void **head, int n, void **start, void **end);
void FL_PushRange(void **head, void *start, void *end);
size_t FL_Size(void *head);

#else // TCMALLOC_USE_DOUBLYLINKED_FREELIST not defined
static const bool kSupportsDoublyLinkedList = false;

inline void *FL_Next(void *t) {
  return SLL_Next(t);
}

inline void FL_Init(void *t) {
  SLL_SetNext(t, NULL);
}

inline void FL_Push(void **list, void *element) {
  if(*list != element) {
    SLL_Push(list,element);
    return;
  }
  CRASH("Double Free of %p detected", element);
}

inline void *FL_Pop(void **list) {
  return SLL_Pop(list);
}

// Removes |N| elements from a linked list to which |head| points.
// |head| will be modified to point to the new |head|.  |start| and
// |end| will point to the first and last nodes of the range.  Note
// that |end| will point to NULL after this function is called.
inline void FL_PopRange(void **head, int n, void **start, void **end) {
  SLL_PopRange(head, n, start, end);
}

inline void FL_PushRange(void **head, void *start, void *end) {
  SLL_PushRange(head,start,end);
}

inline size_t FL_Size(void *head) {
  return SLL_Size(head);
}

#endif // TCMALLOC_USE_DOUBLYLINKED_FREELIST

} // namespace tcmalloc

#endif // TCMALLOC_FREE_LIST_H_
