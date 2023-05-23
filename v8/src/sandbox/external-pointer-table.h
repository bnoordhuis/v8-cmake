// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_EXTERNAL_POINTER_TABLE_H_
#define V8_SANDBOX_EXTERNAL_POINTER_TABLE_H_

#include "include/v8config.h"
#include "src/base/atomicops.h"
#include "src/base/memory.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/sandbox/external-entity-table.h"

#ifdef V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

class Isolate;

/**
 * The entries of an ExternalPointerTable.
 *
 * Each entry consists of a single pointer-sized word containing the external
 * pointer, the marking bit, and a type tag. An entry can either be:
 *  - A "regular" entry, containing the external pointer together with a type
 *    tag and the marking bit in the unused upper bits, or
 *  - A freelist entry, tagged with the kExternalPointerFreeEntryTag and
 *    containing the index of the next free entry in the lower 32 bits, or
 *  - An evacuation entry, tagged with the kExternalPointerEvacuationEntryTag
 *    and containing the address of the ExternalPointerSlot referencing the
 *    entry that will be evacuated into this entry. See the compaction
 *    algorithm overview for more details about these entries.
 */
struct ExternalPointerTableEntry {
  // Make this entry an external pointer entry containing the given pointer
  // tagged with the given tag.
  inline void MakeExternalPointerEntry(Address value, ExternalPointerTag tag);

  // Load and untag the external pointer stored in this entry.
  // This entry must be an external pointer entry.
  // If the specified tag doesn't match the actual tag of this entry, the
  // resulting pointer will be invalid and cannot be dereferenced.
  inline Address GetExternalPointer(ExternalPointerTag tag) const;

  // Tag and store the given external pointer in this entry.
  // This entry must be an external pointer entry.
  inline void SetExternalPointer(Address value, ExternalPointerTag tag);

  // Exchanges the external pointer stored in this entry with the provided one.
  // Returns the old external pointer. This entry must be an external pointer
  // entry. If the provided tag doesn't match the tag of the old entry, the
  // returned pointer will be invalid.
  inline Address ExchangeExternalPointer(Address value, ExternalPointerTag tag);

  // Make this entry a freelist entry, containing the index of the next entry
  // on the freelist.
  inline void MakeFreelistEntry(uint32_t next_entry_index);

  // Get the index of the next entry on the freelist. This method may be
  // called even when the entry is not a freelist entry. However, the result
  // is only valid if this is a freelist entry. This behaviour is required
  // for efficient entry allocation, see TryAllocateEntryFromFreelist.
  inline uint32_t GetNextFreelistEntryIndex() const;

  // Make this entry an evacuation entry containing the address of the handle to
  // the entry being evacuated.
  inline void MakeEvacuationEntry(Address handle_location);

  // Move the content of this entry into the provided entry while also clearing
  // the marking bit. Used during table compaction. This invalidates the entry.
  inline void UnmarkAndMigrateInto(ExternalPointerTableEntry& other);

  // Mark this entry as alive during table garbage collection.
  inline void Mark();

 private:
  friend class ExternalPointerTable;

  // ExternalPointerTable entries consist of a single pointer-sized word
  // containing a tag and marking bit together with the actual content (e.g. an
  // external pointer).
  struct Payload {
    Payload(Address pointer, ExternalPointerTag tag)
        : encoded_word_(Tag(pointer, tag)) {}

    Address Untag(ExternalPointerTag tag) const { return encoded_word_ & ~tag; }

    static Address Tag(Address pointer, ExternalPointerTag tag) {
      return pointer | tag;
    }

    bool IsTaggedWith(ExternalPointerTag tag) const {
      return (encoded_word_ & kExternalPointerTagMask) == tag;
    }

    void SetMarkBit() { encoded_word_ |= kExternalPointerMarkBit; }

    void ClearMarkBit() { encoded_word_ &= ~kExternalPointerMarkBit; }

    bool HasMarkBitSet() const {
      return (encoded_word_ & kExternalPointerMarkBit) != 0;
    }

    bool ContainsFreelistLink() const {
      return IsTaggedWith(kExternalPointerFreeEntryTag);
    }

    uint32_t ExtractFreelistLink() const {
      return static_cast<uint32_t>(encoded_word_);
    }

    bool ContainsEvacuationEntry() const {
      return IsTaggedWith(kExternalPointerEvacuationEntryTag);
    }

    Address ExtractEvacuationEntryHandleLocation() const {
      return Untag(kExternalPointerEvacuationEntryTag);
    }

    bool ContainsExternalPointer() const {
      return !ContainsFreelistLink() && !ContainsEvacuationEntry();
    }

    bool operator==(Payload other) const {
      return encoded_word_ == other.encoded_word_;
    }
    bool operator!=(Payload other) const {
      return encoded_word_ != other.encoded_word_;
    }

   private:
    Address encoded_word_;
  };

  inline Payload GetRawPayload() {
    return payload_.load(std::memory_order_relaxed);
  }
  inline void SetRawPayload(Payload new_payload) {
    return payload_.store(new_payload, std::memory_order_relaxed);
  }

  inline void MaybeUpdateRawPointerForLSan(Address value) {
#if defined(LEAK_SANITIZER)
    raw_pointer_for_lsan_ = value;
#endif  // LEAK_SANITIZER
  }

  std::atomic<Payload> payload_;

#if defined(LEAK_SANITIZER)
  //  When LSan is active, it must be able to detect live references to heap
  //  allocations from an external pointer table. It will, however, not be able
  //  to recognize the encoded pointers as they will have their top bits set. So
  //  instead, when LSan is active we use "fat" entries where the 2nd atomic
  //  words contains the unencoded raw pointer which LSan will be able to
  //  recognize as such.
  //  NOTE: THIS MODE IS NOT SECURE! Attackers are able to modify an
  //  ExternalPointerHandle to point to the raw pointer part, not the encoded
  //  part of an entry, thereby bypassing the type checks. If this mode is ever
  //  needed outside of testing environments, then the external pointer
  //  accessors (e.g. in the JIT) need to be made aware that entries are now 16
  //  bytes large so that all entry accesses are again guaranteed to access an
  //  encoded pointer.
  Address raw_pointer_for_lsan_;
#endif  // LEAK_SANITIZER
};

#if defined(LEAK_SANITIZER)
//  When LSan is active, we need "fat" entries, see above.
static_assert(sizeof(ExternalPointerTableEntry) == 16);
#else
//  We expect ExternalPointerTable entries to consist of a single 64-bit word.
static_assert(sizeof(ExternalPointerTableEntry) == 8);
#endif

/**
 * A table storing pointers to objects outside the V8 heap.
 *
 * When V8_ENABLE_SANDBOX, its primary use is for pointing to objects outside
 * the sandbox, as described below.
 *
 * An external pointer table provides the basic mechanisms to ensure
 * memory-safe access to objects located outside the sandbox, but referenced
 * from within it. When an external pointer table is used, objects located
 * inside the sandbox reference outside objects through indices into the table.
 *
 * Type safety can be ensured by using type-specific tags for the external
 * pointers. These tags will be ORed into the unused top bits of the pointer
 * when storing them and will be ANDed away when loading the pointer later
 * again. If a pointer of the wrong type is accessed, some of the top bits will
 * remain in place, rendering the pointer inaccessible.
 *
 * Temporal memory safety is achieved through garbage collection of the table,
 * which ensures that every entry is either an invalid pointer or a valid
 * pointer pointing to a live object.
 *
 * Spatial memory safety can, if necessary, be ensured by storing the size of a
 * referenced object together with the object itself outside the sandbox, and
 * referencing both through a single entry in the table.
 *
 * The garbage collection algorithm for the table works as follows:
 *  - One bit of every entry is reserved for the marking bit.
 *  - Every store to an entry automatically sets the marking bit when ORing
 *    with the tag. This avoids the need for write barriers.
 *  - Every load of an entry automatically removes the marking bit when ANDing
 *    with the inverted tag.
 *  - When the GC marking visitor finds a live object with an external pointer,
 *    it marks the corresponding entry as alive through Mark(), which sets the
 *    marking bit using an atomic CAS operation.
 *  - When marking is finished, SweepAndCompact() iterates of the table once
 *    while the mutator is stopped and builds a freelist from all dead entries
 *    while also removing the marking bit from any live entry.
 *
 * When V8_COMPRESS_POINTERS, external pointer tables are also used to ease
 * alignment requirements in heap object fields via indirection.
 */
class V8_EXPORT_PRIVATE ExternalPointerTable
    : public ExternalEntityTable<ExternalPointerTableEntry> {
 public:
  // Size of an ExternalPointerTable, for layout computation in IsolateData.
  // Asserted to be equal to the actual size in external-pointer-table.cc.
  static int constexpr kSize = 4 * kSystemPointerSize;

  ExternalPointerTable() = default;

  ExternalPointerTable(const ExternalPointerTable&) = delete;
  ExternalPointerTable& operator=(const ExternalPointerTable&) = delete;

  // Initializes this external pointer table by reserving the backing memory
  // and initializing the freelist.
  void Init(Isolate* isolate);

  // Resets this external pointer table and deletes all associated memory.
  void TearDown();

  // Retrieves the entry referenced by the given handle.
  //
  // This method is atomic and can be called from background threads.
  inline Address Get(ExternalPointerHandle handle,
                     ExternalPointerTag tag) const;

  // Sets the entry referenced by the given handle.
  //
  // This method is atomic and can be called from background threads.
  inline void Set(ExternalPointerHandle handle, Address value,
                  ExternalPointerTag tag);

  // Exchanges the entry referenced by the given handle with the given value,
  // returning the previous value. The same tag is applied both to decode the
  // previous value and encode the given value.
  //
  // This method is atomic and can be called from background threads.
  inline Address Exchange(ExternalPointerHandle handle, Address value,
                          ExternalPointerTag tag);

  // Allocates a new entry in the external pointer table. The caller must
  // provide the initial value and tag.
  //
  // This method is atomic and can be called from background threads.
  inline ExternalPointerHandle AllocateAndInitializeEntry(
      Isolate* isolate, Address initial_value, ExternalPointerTag tag);

  // Marks the specified entry as alive.
  //
  // If the table is currently being compacted, this may also mark the entry
  // for evacuation for which the location of the handle is required. See the
  // comments about table compaction below for more details.
  //
  // This method is atomic and can be called from background threads.
  inline void Mark(ExternalPointerHandle handle, Address handle_location);

  // Table compaction.
  //
  // The table is to some degree self-compacting: since the freelist is
  // sorted in ascending order (see SweepAndCompact()), empty slots at the start
  // of the table will usually quickly be filled. Further, empty blocks at the
  // end of the table will be decommitted to reduce memory usage. However, live
  // entries at the end of the table can prevent this decommitting and cause
  // fragmentation. The following simple algorithm is therefore used to
  // compact the table if that is deemed necessary:
  //  - At the start of the GC marking phase, determine if the table needs to be
  //    compacted. This decisiont is mostly based on the absolute and relative
  //    size of the freelist.
  //  - If compaction is needed, this algorithm attempts to shrink the table by
  //    FreelistSize/2 entries during compaction by moving all live entries out
  //    of the evacuation area (the last FreelistSize/2 entries of the table),
  //    then decommitting those blocks at the end of SweepAndCompact().
  //  - During marking, whenever a live entry inside the evacuation area is
  //    found, a new "evacuation entry" is allocated from the freelist (which is
  //    assumed to have enough free slots) and the address of the handle is
  //    written into it.
  //  - During sweeping, these evacuation entries are resolved: the content of
  //    the old entry is copied into the new entry and the handle in the object
  //    is updated to point to the new entry.
  //
  // When compacting, it is expected that the evacuation area contains few live
  // entries and that the freelist will be able to serve all evacuation entry
  // allocations. In that case, compaction is essentially free (very little
  // marking overhead, no memory overhead). However, it can happen that the
  // application allocates a large number of entries from the table during
  // marking, in which case the freelist would no longer be able to serve all
  // allocation without growing. If that situation is detected, compaction is
  // aborted during marking.
  //
  // This algorithm assumes that table entries (except for the null entry) are
  // never shared between multiple objects. Otherwise, the following could
  // happen: object A initially has handle H1 and is scanned during incremental
  // marking. Next, object B with handle H2 is scanned and marked for
  // evacuation. Afterwards, object A copies the handle H2 from object B.
  // During sweeping, only object B's handle will be updated to point to the
  // new entry while object A's handle is now dangling. If shared entries ever
  // become necessary, setting external pointer handles would have to be
  // guarded by write barriers to avoid this scenario.

  // Frees unmarked entries and finishes table compaction (if running).
  //
  // This method must only be called while mutator threads are stopped as it is
  // not safe to allocate table entries while the table is being swept.
  //
  // Returns the number of live entries after sweeping.
  uint32_t SweepAndCompact(Isolate* isolate);

  // Determine if table compaction is needed and if so start the compaction.
  // This is expected to be called at the start of the GC marking phase.
  void StartCompactingIfNeeded();

 private:
  // Required for Isolate::CheckIsolateLayout().
  friend class Isolate;

  inline uint32_t HandleToIndex(ExternalPointerHandle handle) const;
  inline ExternalPointerHandle IndexToHandle(uint32_t index) const;

  inline void MaybeCreateEvacuationEntry(uint32_t index,
                                         Address handle_location);

  void ResolveEvacuationEntryDuringSweeping(
      uint32_t index, ExternalPointerHandle* handle_location,
      uint32_t start_of_evacuation_area);

  inline bool IsCompacting();

  inline void StartCompacting(uint32_t start_of_evacuation_area);

  inline void StopCompacting();

  inline void AbortCompacting(uint32_t start_of_evacuation_area);

  inline bool CompactingWasAbortedDuringMarking();

#ifdef DEBUG
  // In debug builds during GC marking, this value is ORed into
  // ExternalPointerHandles whose entries are marked for evacuation. During
  // sweeping, the Handles for evacuated entries are checked to have this
  // marker value. This allows detecting re-initialized entries, which are
  // problematic for table compaction. This is only possible for entries marked
  // for evacuation as the location of the Handle is only known for those.
  static constexpr uint32_t kVisitedHandleMarker = 0x1;
  static_assert(kExternalPointerIndexShift >= 1);

  inline bool HandleWasVisitedDuringMarking(ExternalPointerHandle handle) {
    return (handle & kVisitedHandleMarker) == kVisitedHandleMarker;
  }
#endif  // DEBUG

  // When compacting the table, the extra_ field in our parent class contains
  // the index of the first entry in the evacuation area. The evacuation area is
  // the region at the end of the table from which entries are moved out of so
  // that the underyling memory pages can be freed after sweeping. The field can
  // have the following values:
  // - kNotCompactingMarker: compaction is not currently running.
  // - A kEntriesPerBlock aligned value within (0, capacity): table compaction
  //   is running and all entries after this value should be evacuated.
  // - A value that has kCompactionAbortedMarker in its top bits: table
  //   compaction has been aborted during marking. The original start of the
  //   evacuation area is still contained in the lower bits.
  //
  // This value indicates that the table is not currently being compacted. It
  // is set to uint32_t max so that determining whether an entry should be
  // evacuated becomes a single comparison:
  // `bool should_be_evacuated = index >= start_of_evacuation_area`.
  static constexpr uint32_t kNotCompactingMarker =
      std::numeric_limits<uint32_t>::max();

  // This value may be ORed into the start of evacuation area threshold (stored
  // in the extra_ field) during the GC marking phase to indicate that table
  // compaction has been aborted because the freelist grew to short and so
  // evacuation entry allocation is no longer possible. This will prevent any
  // further evacuation attempts as entries will be evacuated if their index is
  // at or above the start of the evacuation area, which is now a huge value.
  static constexpr uint32_t kCompactionAbortedMarker = 0xf0000000;

  // Outcome of external pointer table compaction to use for the
  // ExternalPointerTableCompactionOutcome histogram.
  enum class TableCompactionOutcome {
    // Table compaction was successful.
    kSuccess = 0,
    // Table compaction was partially successful: marking finished successfully,
    // but not all blocks that we wanted to free could be freed because some new
    // entries had already been allocated in them again.
    kPartialSuccess = 1,
    // Table compaction was aborted during marking because the freelist grew to
    // short.
    kAbortedDuringMarking = 2,
  };
};

static_assert(sizeof(ExternalPointerTable) == ExternalPointerTable::kSize);

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPRESS_POINTERS

#endif  // V8_SANDBOX_EXTERNAL_POINTER_TABLE_H_
