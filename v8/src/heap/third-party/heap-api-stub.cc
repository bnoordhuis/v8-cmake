// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/third-party/heap-api.h"

namespace v8 {
namespace internal {
namespace third_party_heap {

// Work around Visual Studio linker errors when V8_ENABLE_THIRD_PARTY_HEAP
// is disabled.
#ifndef V8_ENABLE_THIRD_PARTY_HEAP

// static
std::unique_ptr<Heap> Heap::New(v8::internal::Isolate*) { return nullptr; }

// static
v8::internal::Isolate* Heap::GetIsolate(Address) { return nullptr; }

AllocationResult Heap::Allocate(size_t, AllocationType, AllocationAlignment) {
  return AllocationResult();
}

Address Heap::GetObjectFromInnerPointer(Address) { return 0; }

const base::AddressRegion& Heap::GetCodeRange() {
  static const base::AddressRegion no_region(0, 0);
  return no_region;
}

// static
bool Heap::InCodeSpace(Address) { return false; }

// static
bool Heap::InReadOnlySpace(Address) { return false; }

// static
bool Heap::IsValidHeapObject(HeapObject) { return false; }

bool Heap::CollectGarbage() { return false; }

#endif  // !defined(V8_ENABLE_THIRD_PARTY_HEAP)

}  // namespace third_party_heap
}  // namespace internal
}  // namespace v8
