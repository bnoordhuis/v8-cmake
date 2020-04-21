// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/third-party/heap-api.h"
#include "src/execution/isolate-utils-inl.h"
#include "src/heap/heap-inl.h"

// Work around Visual Studio linker errors when V8_ENABLE_THIRD_PARTY_HEAP
// is disabled.
#ifndef V8_ENABLE_THIRD_PARTY_HEAP

namespace v8 {
namespace internal {

Isolate* Heap::GetIsolateFromWritableObject(HeapObject object) {
  return GetHeapFromWritableObject(object)->isolate();
}

}  // namespace internal
}  // namespace v8

namespace v8 {
namespace internal {
namespace third_party_heap {

// static
std::unique_ptr<Heap> Heap::New(v8::Isolate*) { return nullptr; }

// static
v8::Isolate* Heap::GetIsolate(Address) { return nullptr; }

AllocationResult Heap::Allocate(size_t) {
  return AllocationResult();
}

AllocationResult Heap::AllocateCode(size_t) {
  return AllocationResult();
}

Address Heap::GetObjectFromInnerPointer(Address) { return 0; }

void Heap::CollectGarbage() {}

}  // namespace third_party_heap
}  // namespace internal
}  // namespace v8

#endif  // !defined(V8_ENABLE_THIRD_PARTY_HEAP)
