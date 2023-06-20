// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_CODE_POINTER_INL_H_
#define V8_SANDBOX_CODE_POINTER_INL_H_

#include "include/v8-internal.h"
#include "src/base/atomic-utils.h"
#include "src/execution/isolate.h"
#include "src/sandbox/code-pointer-table-inl.h"
#include "src/sandbox/code-pointer.h"

namespace v8 {
namespace internal {

V8_INLINE void InitCodePointerField(Address field_address, Isolate* isolate,
                                    Address value) {
#ifdef V8_CODE_POINTER_SANDBOXING
  CodePointerHandle handle =
      GetProcessWideCodePointerTable()->AllocateAndInitializeEntry(value);
  // Use a Release_Store to ensure that the store of the pointer into the
  // table is not reordered after the store of the handle. Otherwise, other
  // threads may access an uninitialized table entry and crash.
  auto location = reinterpret_cast<ExternalPointerHandle*>(field_address);
  base::AsAtomic32::Release_Store(location, handle);
#else
  WriteCodePointerField(field_address, value);
#endif  // V8_CODE_POINTER_SANDBOXING
}

V8_INLINE Address ReadCodePointerField(Address field_address) {
#ifdef V8_CODE_POINTER_SANDBOXING
  // Handles may be written to objects from other threads so the handle needs
  // to be loaded atomically. We assume that the load from the table cannot
  // be reordered before the load of the handle due to the data dependency
  // between the two loads and therefore use relaxed memory ordering, but
  // technically we should use memory_order_consume here.
  auto location = reinterpret_cast<ExternalPointerHandle*>(field_address);
  CodePointerHandle handle = base::AsAtomic32::Relaxed_Load(location);
  return GetProcessWideCodePointerTable()->Get(handle);
#else
  return ReadMaybeUnalignedValue<Address>(field_address);
#endif  // V8_CODE_POINTER_SANDBOXING
}

V8_INLINE void WriteCodePointerField(Address field_address, Address value) {
#ifdef V8_CODE_POINTER_SANDBOXING
  // See comment above for why this is a Relaxed_Load.
  auto location = reinterpret_cast<ExternalPointerHandle*>(field_address);
  CodePointerHandle handle = base::AsAtomic32::Relaxed_Load(location);
  GetProcessWideCodePointerTable()->Set(handle, value);
#else
  WriteMaybeUnalignedValue<Address>(field_address, value);
#endif  // V8_CODE_POINTER_SANDBOXING
}

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_CODE_POINTER_INL_H_
