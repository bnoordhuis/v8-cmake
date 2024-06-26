// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking-state-inl.h"
#include "src/heap/spaces.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-tester.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
namespace heap {

HEAP_TEST(WriteBarrier_Marking) {
  if (!v8_flags.incremental_marking) return;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope outer(isolate);
  Handle<FixedArray> objects = factory->NewFixedArray(3);
  v8::Global<Value> global_objects(CcTest::isolate(), Utils::ToLocal(objects));
  {
    // Make sure that these objects are not immediately reachable from
    // the roots to prevent them being marked grey at the start of marking.
    HandleScope inner(isolate);
    Handle<FixedArray> host = factory->NewFixedArray(1);
    Handle<HeapNumber> value1 = factory->NewHeapNumber(1.1);
    Handle<HeapNumber> value2 = factory->NewHeapNumber(1.2);
    objects->set(0, *host);
    objects->set(1, *value1);
    objects->set(2, *value2);
  }
  heap::SimulateIncrementalMarking(CcTest::heap(), false);
  FixedArray host = FixedArray::cast(objects->get(0));
  HeapObject value1 = HeapObject::cast(objects->get(1));
  HeapObject value2 = HeapObject::cast(objects->get(2));
  CHECK(heap->marking_state()->IsUnmarked(host));
  CHECK(heap->marking_state()->IsUnmarked(value1));
  // Trigger the barrier for the unmarked host and expect the bail out.
  WriteBarrier::Marking(host, host.RawFieldOfElementAt(0), value1);
  CHECK(!heap->marking_state()->IsMarked(value1));
  heap->marking_state()->TryMarkAndAccountLiveBytes(host);
  // Trigger the barrier for the marked host.
  WriteBarrier::Marking(host, host.RawFieldOfElementAt(0), value1);
  CHECK(heap->marking_state()->IsMarked(value1));

  CHECK(heap->marking_state()->IsUnmarked(value2));
  WriteBarrier::Marking(host, host.RawFieldOfElementAt(0), value2);
  CHECK(heap->marking_state()->IsMarked(value2));
  heap::SimulateIncrementalMarking(CcTest::heap(), true);
  CHECK(heap->marking_state()->IsMarked(host));
  CHECK(heap->marking_state()->IsMarked(value1));
  CHECK(heap->marking_state()->IsMarked(value2));
}

HEAP_TEST(WriteBarrier_MarkingExtension) {
  if (!v8_flags.incremental_marking) return;
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope outer(isolate);
  Handle<FixedArray> objects = factory->NewFixedArray(1);
  ArrayBufferExtension* extension;
  {
    HandleScope inner(isolate);
    Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(CcTest::isolate(), 100);
    Handle<JSArrayBuffer> host = v8::Utils::OpenHandle(*ab);
    extension = host->extension();
    objects->set(0, *host);
  }
  heap::SimulateIncrementalMarking(CcTest::heap(), false);
  JSArrayBuffer host = JSArrayBuffer::cast(objects->get(0));
  CHECK(heap->marking_state()->IsUnmarked(host));
  CHECK(!extension->IsMarked());
  WriteBarrier::Marking(host, extension);
  // Concurrent marking barrier should bail out for unmarked host.
  CHECK(!extension->IsMarked());
  heap->marking_state()->TryMarkAndAccountLiveBytes(host);
  WriteBarrier::Marking(host, extension);
  // Concurrent marking barrier should mark the value now.
  CHECK(extension->IsMarked());
  // Keep object alive using the global handle.
  v8::Global<ArrayBuffer> global_host(CcTest::isolate(),
                                      Utils::ToLocal(handle(host, isolate)));
  heap::SimulateIncrementalMarking(CcTest::heap(), true);
  CHECK(heap->marking_state()->IsMarked(host));
  CHECK(extension->IsMarked());
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
