// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/builtins/builtins-constructor.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/interface-descriptors.h"
#include "src/codegen/maglev-safepoint-table.h"
#include "src/codegen/register.h"
#include "src/codegen/reglist.h"
#include "src/codegen/x64/assembler-x64.h"
#include "src/codegen/x64/register-x64.h"
#include "src/common/globals.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/feedback-source.h"
#include "src/compiler/js-heap-broker.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/ic/handler-configuration.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/maglev/maglev-assembler-inl.h"
#include "src/maglev/maglev-code-gen-state.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "src/objects/elements-kind.h"
#include "src/objects/instance-type.h"
#include "src/objects/js-array-buffer.h"

namespace v8 {
namespace internal {
namespace maglev {

#define __ masm->

constexpr Condition ConditionForFloat64(Operation operation) {
  switch (operation) {
    case Operation::kEqual:
    case Operation::kStrictEqual:
      return equal;
    case Operation::kLessThan:
      return below;
    case Operation::kLessThanOrEqual:
      return below_equal;
    case Operation::kGreaterThan:
      return above;
    case Operation::kGreaterThanOrEqual:
      return above_equal;
    default:
      UNREACHABLE();
  }
}

// ---
// Nodes
// ---

int GeneratorStore::MaxCallStackArgs() const {
  return WriteBarrierDescriptor::GetStackParameterCount();
}
void GeneratorStore::SetValueLocationConstraints() {
  UseAny(context_input());
  UseRegister(generator_input());
  for (int i = 0; i < num_parameters_and_registers(); i++) {
    UseAny(parameters_and_registers(i));
  }
  RequireSpecificTemporary(WriteBarrierDescriptor::ObjectRegister());
  RequireSpecificTemporary(WriteBarrierDescriptor::SlotAddressRegister());
}
void GeneratorStore::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  Register generator = ToRegister(generator_input());
  Register array = WriteBarrierDescriptor::ObjectRegister();
  __ LoadTaggedPointerField(
      array, FieldOperand(generator,
                          JSGeneratorObject::kParametersAndRegistersOffset));

  for (int i = 0; i < num_parameters_and_registers(); i++) {
    // Use WriteBarrierDescriptor::SlotAddressRegister() as the scratch
    // register since it's a known temporary, and the write barrier slow path
    // generates better code when value == scratch. Can't use kScratchRegister
    // because CheckPageFlag uses it.
    Register value =
        __ FromAnyToRegister(parameters_and_registers(i),
                             WriteBarrierDescriptor::SlotAddressRegister());

    ZoneLabelRef done(masm);
    DeferredCodeInfo* deferred_write_barrier = __ PushDeferredCode(
        [](MaglevAssembler* masm, ZoneLabelRef done, Register value,
           Register array, GeneratorStore* node, int32_t offset) {
          ASM_CODE_COMMENT_STRING(masm, "Write barrier slow path");
          // Use WriteBarrierDescriptor::SlotAddressRegister() as the scratch
          // register, see comment above.
          __ CheckPageFlag(
              value, WriteBarrierDescriptor::SlotAddressRegister(),
              MemoryChunk::kPointersToHereAreInterestingOrInSharedHeapMask,
              zero, *done);

          Register slot_reg = WriteBarrierDescriptor::SlotAddressRegister();

          __ leaq(slot_reg, FieldOperand(array, offset));

          // TODO(leszeks): Add an interface for flushing all double registers
          // before this Node, to avoid needing to save them here.
          SaveFPRegsMode const save_fp_mode =
              !node->register_snapshot().live_double_registers.is_empty()
                  ? SaveFPRegsMode::kSave
                  : SaveFPRegsMode::kIgnore;

          __ CallRecordWriteStub(array, slot_reg, save_fp_mode);

          __ jmp(*done);
        },
        done, value, array, this, FixedArray::OffsetOfElementAt(i));

    __ StoreTaggedField(FieldOperand(array, FixedArray::OffsetOfElementAt(i)),
                        value);
    __ JumpIfSmi(value, *done, Label::kNear);
    // TODO(leszeks): This will stay either false or true throughout this loop.
    // Consider hoisting the check out of the loop and duplicating the loop into
    // with and without write barrier.
    __ CheckPageFlag(array, kScratchRegister,
                     MemoryChunk::kPointersFromHereAreInterestingMask, not_zero,
                     &deferred_write_barrier->deferred_code_label);

    __ bind(*done);
  }

  // Use WriteBarrierDescriptor::SlotAddressRegister() as the scratch
  // register, see comment above.
  Register context = __ FromAnyToRegister(
      context_input(), WriteBarrierDescriptor::SlotAddressRegister());

  ZoneLabelRef done(masm);
  DeferredCodeInfo* deferred_context_write_barrier = __ PushDeferredCode(
      [](MaglevAssembler* masm, ZoneLabelRef done, Register context,
         Register generator, GeneratorStore* node) {
        ASM_CODE_COMMENT_STRING(masm, "Write barrier slow path");
        // Use WriteBarrierDescriptor::SlotAddressRegister() as the scratch
        // register, see comment above.
        // TODO(leszeks): The context is almost always going to be in
        // old-space, consider moving this check to the fast path, maybe even
        // as the first bailout.
        __ CheckPageFlag(
            context, WriteBarrierDescriptor::SlotAddressRegister(),
            MemoryChunk::kPointersToHereAreInterestingOrInSharedHeapMask, zero,
            *done);

        __ Move(WriteBarrierDescriptor::ObjectRegister(), generator);
        generator = WriteBarrierDescriptor::ObjectRegister();
        Register slot_reg = WriteBarrierDescriptor::SlotAddressRegister();

        __ leaq(slot_reg,
                FieldOperand(generator, JSGeneratorObject::kContextOffset));

        // TODO(leszeks): Add an interface for flushing all double registers
        // before this Node, to avoid needing to save them here.
        SaveFPRegsMode const save_fp_mode =
            !node->register_snapshot().live_double_registers.is_empty()
                ? SaveFPRegsMode::kSave
                : SaveFPRegsMode::kIgnore;

        __ CallRecordWriteStub(generator, slot_reg, save_fp_mode);

        __ jmp(*done);
      },
      done, context, generator, this);
  __ StoreTaggedField(
      FieldOperand(generator, JSGeneratorObject::kContextOffset), context);
  __ AssertNotSmi(context);
  __ CheckPageFlag(generator, kScratchRegister,
                   MemoryChunk::kPointersFromHereAreInterestingMask, not_zero,
                   &deferred_context_write_barrier->deferred_code_label);
  __ bind(*done);

  __ StoreTaggedSignedField(
      FieldOperand(generator, JSGeneratorObject::kContinuationOffset),
      Smi::FromInt(suspend_id()));
  __ StoreTaggedSignedField(
      FieldOperand(generator, JSGeneratorObject::kInputOrDebugPosOffset),
      Smi::FromInt(bytecode_offset()));
}

int CreateEmptyObjectLiteral::MaxCallStackArgs() const {
  return AllocateDescriptor::GetStackParameterCount();
}
void CreateEmptyObjectLiteral::SetValueLocationConstraints() {
  DefineAsRegister(this);
}
void CreateEmptyObjectLiteral::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
  Register object = ToRegister(result());
  RegisterSnapshot save_registers = register_snapshot();
  __ Allocate(save_registers, object, map().instance_size());
  __ Move(kScratchRegister, map().object());
  __ StoreTaggedField(FieldOperand(object, HeapObject::kMapOffset),
                      kScratchRegister);
  __ LoadRoot(kScratchRegister, RootIndex::kEmptyFixedArray);
  __ StoreTaggedField(FieldOperand(object, JSObject::kPropertiesOrHashOffset),
                      kScratchRegister);
  __ StoreTaggedField(FieldOperand(object, JSObject::kElementsOffset),
                      kScratchRegister);
  __ LoadRoot(kScratchRegister, RootIndex::kUndefinedValue);
  for (int i = 0; i < map().GetInObjectProperties(); i++) {
    int offset = map().GetInObjectPropertyOffset(i);
    __ StoreTaggedField(FieldOperand(object, offset), kScratchRegister);
  }
}

void AssertInt32::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
}
void AssertInt32::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  __ cmpl(ToRegister(left_input()), ToRegister(right_input()));
  __ Check(ToCondition(condition_), reason_);
}

void CheckMaps::SetValueLocationConstraints() { UseRegister(receiver_input()); }
void CheckMaps::GenerateCode(MaglevAssembler* masm,
                             const ProcessingState& state) {
  Register object = ToRegister(receiver_input());

  // TODO(victorgomes): This can happen, because we do not emit an unconditional
  // deopt when we intersect the map sets.
  if (maps().is_empty()) {
    __ EmitEagerDeopt(this, DeoptimizeReason::kWrongMap);
    return;
  }

  bool maps_include_heap_number = AnyMapIsHeapNumber(maps());

  Label done;
  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    if (maps_include_heap_number) {
      // Smis count as matching the HeapNumber map, so we're done.
      __ j(is_smi, &done);
    } else {
      __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kWrongMap, this);
    }
  }

  size_t map_count = maps().size();
  for (size_t i = 0; i < map_count - 1; ++i) {
    Handle<Map> map = maps().at(i);
    __ Cmp(FieldOperand(object, HeapObject::kMapOffset), map);
    __ j(equal, &done);
  }
  Handle<Map> last_map = maps().at(map_count - 1);
  __ Cmp(FieldOperand(object, HeapObject::kMapOffset), last_map);
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kWrongMap, this);
  __ bind(&done);
}

void CheckNumber::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckNumber::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  Label done;
  Register value = ToRegister(receiver_input());
  // If {value} is a Smi or a HeapNumber, we're done.
  __ JumpIfSmi(value, &done);
  __ CompareRoot(FieldOperand(value, HeapObject::kMapOffset),
                 RootIndex::kHeapNumberMap);
  if (mode() == Object::Conversion::kToNumeric) {
    // Jump to done if it is a HeapNumber.
    __ j(equal, &done);
    // Check if it is a BigInt.
    __ LoadMap(kScratchRegister, value);
    __ cmpw(FieldOperand(kScratchRegister, Map::kInstanceTypeOffset),
            Immediate(BIGINT_TYPE));
  }
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotANumber, this);
  __ bind(&done);
}

void CheckSymbol::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckSymbol::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kNotASymbol, this);
  }
  __ LoadMap(kScratchRegister, object);
  __ CmpInstanceType(kScratchRegister, SYMBOL_TYPE);
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotASymbol, this);
}

void CheckInstanceType::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckInstanceType::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kWrongInstanceType, this);
  }
  __ LoadMap(kScratchRegister, object);
  __ CmpInstanceType(kScratchRegister, instance_type());
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kWrongInstanceType, this);
}

void CheckString::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckString::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kNotAString, this);
  }
  __ LoadMap(kScratchRegister, object);
  __ CmpInstanceTypeRange(kScratchRegister, kScratchRegister, FIRST_STRING_TYPE,
                          LAST_STRING_TYPE);
  __ EmitEagerDeoptIf(above, DeoptimizeReason::kNotAString, this);
}

int CheckMapsWithMigration::MaxCallStackArgs() const {
  DCHECK_EQ(Runtime::FunctionForId(Runtime::kTryMigrateInstance)->nargs, 1);
  return 1;
}
void CheckMapsWithMigration::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckMapsWithMigration::GenerateCode(MaglevAssembler* masm,
                                          const ProcessingState& state) {
  // TODO(victorgomes): This can happen, because we do not emit an unconditional
  // deopt when we intersect the map sets.
  if (maps().is_empty()) {
    __ EmitEagerDeopt(this, DeoptimizeReason::kWrongMap);
    return;
  }

  Register object = ToRegister(receiver_input());

  bool maps_include_heap_number = AnyMapIsHeapNumber(maps());

  ZoneLabelRef done(masm);
  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    if (maps_include_heap_number) {
      // Smis count as matching the HeapNumber map, so we're done.
      __ j(is_smi, *done);
    } else {
      __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kWrongMap, this);
    }
  }

  size_t map_count = maps().size();
  for (size_t i = 0; i < map_count; ++i) {
    ZoneLabelRef continue_label(masm);
    Handle<Map> map = maps().at(i);
    __ Cmp(FieldOperand(object, HeapObject::kMapOffset), map);

    bool last_map = (i == map_count - 1);
    if (map->is_migration_target()) {
      __ JumpToDeferredIf(
          not_equal,
          [](MaglevAssembler* masm, ZoneLabelRef continue_label,
             ZoneLabelRef done, Register object, int map_index,
             CheckMapsWithMigration* node) {
            // Reload the map to avoid needing to save it on a temporary in the
            // fast path.
            __ LoadMap(kScratchRegister, object);
            // If the map is not deprecated, we fail the map check, continue to
            // the next one.
            __ movl(kScratchRegister,
                    FieldOperand(kScratchRegister, Map::kBitField3Offset));
            __ testl(kScratchRegister,
                     Immediate(Map::Bits3::IsDeprecatedBit::kMask));
            __ j(zero, *continue_label);

            // Otherwise, try migrating the object. If the migration
            // returns Smi zero, then it failed the migration.
            Register return_val = Register::no_reg();
            {
              RegisterSnapshot register_snapshot = node->register_snapshot();
              // We can eager deopt after the snapshot, so make sure the nodes
              // used by the deopt are included in it.
              // TODO(leszeks): This is a bit of a footgun -- we likely want the
              // snapshot to always include eager deopt input registers.
              AddDeoptRegistersToSnapshot(&register_snapshot,
                                          node->eager_deopt_info());
              SaveRegisterStateForCall save_register_state(masm,
                                                           register_snapshot);

              __ Push(object);
              __ Move(kContextRegister, masm->native_context().object());
              __ CallRuntime(Runtime::kTryMigrateInstance);
              save_register_state.DefineSafepoint();

              // Make sure the return value is preserved across the live
              // register restoring pop all.
              return_val = kReturnRegister0;
              if (register_snapshot.live_registers.has(return_val)) {
                DCHECK(!register_snapshot.live_registers.has(kScratchRegister));
                __ movq(kScratchRegister, return_val);
                return_val = kScratchRegister;
              }
            }

            // On failure, the returned value is zero
            __ cmpl(return_val, Immediate(0));
            __ j(equal, *continue_label);

            // The migrated object is returned on success, retry the map check.
            __ Move(object, return_val);
            // Manually load the map pointer without uncompressing it.
            __ Cmp(FieldOperand(object, HeapObject::kMapOffset),
                   node->maps().at(map_index));
            __ j(equal, *done);
            __ jmp(*continue_label);
          },
          // If this is the last map to check, we should deopt if we fail.
          // This is safe to do, since {eager_deopt_info} is ZoneAllocated.
          (last_map ? ZoneLabelRef::UnsafeFromLabelPointer(masm->GetDeoptLabel(
                          this, DeoptimizeReason::kWrongMap))
                    : continue_label),
          done, object, i, this);
    } else if (last_map) {
      // If it is the last map and it is not a migration target, we should deopt
      // if the check fails.
      __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kWrongMap, this);
    }

    if (!last_map) {
      // We don't need to bind the label for the last map.
      __ j(equal, *done);
      __ bind(*continue_label);
    }
  }

  __ bind(*done);
}

void CheckJSArrayBounds::SetValueLocationConstraints() {
  UseRegister(receiver_input());
  UseRegister(index_input());
}
void CheckJSArrayBounds::GenerateCode(MaglevAssembler* masm,
                                      const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  Register index = ToRegister(index_input());
  __ AssertNotSmi(object);

  if (v8_flags.debug_code) {
    __ CmpObjectType(object, JS_ARRAY_TYPE, kScratchRegister);
    __ Assert(equal, AbortReason::kUnexpectedValue);
  }
  __ SmiUntagField(kScratchRegister,
                   FieldOperand(object, JSArray::kLengthOffset));
  __ cmpl(index, kScratchRegister);
  __ EmitEagerDeoptIf(above_equal, DeoptimizeReason::kOutOfBounds, this);
}

void CheckJSTypedArrayBounds::SetValueLocationConstraints() {
  UseRegister(receiver_input());
  if (ElementsKindSize(elements_kind_) == 1) {
    UseRegister(index_input());
  } else {
    UseAndClobberRegister(index_input());
  }
}
void CheckJSTypedArrayBounds::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  Register index = ToRegister(index_input());
  Register byte_length = kScratchRegister;
  if (v8_flags.debug_code) {
    __ AssertNotSmi(object);
    __ CmpObjectType(object, JS_TYPED_ARRAY_TYPE, kScratchRegister);
    __ Assert(equal, AbortReason::kUnexpectedValue);
  }
  __ LoadBoundedSizeFromObject(byte_length, object,
                               JSTypedArray::kRawByteLengthOffset);
  int element_size = ElementsKindSize(elements_kind_);
  if (element_size > 1) {
    DCHECK(element_size == 2 || element_size == 4 || element_size == 8);
    __ shlq(index, Immediate(base::bits::CountTrailingZeros(element_size)));
  }
  __ cmpq(index, byte_length);
  // We use {above_equal} which does an unsigned comparison to handle negative
  // indices as well.
  __ EmitEagerDeoptIf(above_equal, DeoptimizeReason::kOutOfBounds, this);
}

int CheckJSDataViewBounds::MaxCallStackArgs() const { return 1; }
void CheckJSDataViewBounds::SetValueLocationConstraints() {
  UseRegister(receiver_input());
  UseRegister(index_input());
}
void CheckJSDataViewBounds::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  Register index = ToRegister(index_input());
  Register byte_length = kScratchRegister;
  if (v8_flags.debug_code) {
    __ AssertNotSmi(object);
    __ CmpObjectType(object, JS_DATA_VIEW_TYPE, kScratchRegister);
    __ Assert(equal, AbortReason::kUnexpectedValue);
  }

  ZoneLabelRef done_byte_length(masm);
  __ movl(kScratchRegister, FieldOperand(object, JSDataView::kBitFieldOffset));
  __ testb(kScratchRegister, kScratchRegister);
  __ JumpToDeferredIf(
      not_zero,
      [](MaglevAssembler* masm, CheckJSDataViewBounds* node, ZoneLabelRef done,
         Register object, Register index, Register byte_length) {
        RegisterSnapshot snapshot = node->register_snapshot();
        AddDeoptRegistersToSnapshot(&snapshot, node->eager_deopt_info());
        snapshot.live_registers.set(index);  // Make sure index is saved.
        DCHECK(!snapshot.live_registers.has(byte_length));
        {
          using D = CallInterfaceDescriptorFor<
              Builtin::kDataViewGetVariableLength>::type;
          SaveRegisterStateForCall save_register_state(masm, snapshot);
          __ Move(D::GetRegisterParameter(D::kDataView), object);
          __ Move(kContextRegister, masm->native_context().object());
          __ CallBuiltin(Builtin::kDataViewGetVariableLength);
          __ Move(byte_length, kReturnRegister0);
        }
        __ cmpq(byte_length, Immediate(0));
        // The reason might not be OOB, but because array was detached.
        // Unfortunately we can only add one reason type in Maglev.
        __ EmitEagerDeoptIf(less, DeoptimizeReason::kOutOfBounds, node);
        __ jmp(*done);
      },
      this, done_byte_length, object, index, byte_length);

  // Normal DataView (backed by AB / SAB) or non-length tracking backed by GSAB.
  __ LoadBoundedSizeFromObject(byte_length, object,
                               JSDataView::kRawByteLengthOffset);
  __ bind(*done_byte_length);

  int element_size = ExternalArrayElementSize(element_type_);
  if (element_size > 1) {
    __ subq(byte_length, Immediate(element_size - 1));
    __ EmitEagerDeoptIf(negative, DeoptimizeReason::kOutOfBounds, this);
  }
  __ cmpl(index, byte_length);
  __ EmitEagerDeoptIf(above_equal, DeoptimizeReason::kOutOfBounds, this);
}

void CheckJSObjectElementsBounds::SetValueLocationConstraints() {
  UseRegister(receiver_input());
  UseRegister(index_input());
}
void CheckJSObjectElementsBounds::GenerateCode(MaglevAssembler* masm,
                                               const ProcessingState& state) {
  Register object = ToRegister(receiver_input());
  Register index = ToRegister(index_input());
  __ AssertNotSmi(object);

  if (v8_flags.debug_code) {
    __ CmpObjectType(object, FIRST_JS_OBJECT_TYPE, kScratchRegister);
    __ Assert(greater_equal, AbortReason::kUnexpectedValue);
  }
  __ LoadAnyTaggedField(kScratchRegister,
                        FieldOperand(object, JSObject::kElementsOffset));
  if (v8_flags.debug_code) {
    __ AssertNotSmi(kScratchRegister);
  }
  __ SmiUntagField(kScratchRegister,
                   FieldOperand(kScratchRegister, FixedArray::kLengthOffset));
  __ cmpl(index, kScratchRegister);
  __ EmitEagerDeoptIf(above_equal, DeoptimizeReason::kOutOfBounds, this);
}

void CheckedInternalizedString::SetValueLocationConstraints() {
  UseRegister(object_input());
  set_temporaries_needed(1);
  DefineSameAsFirst(this);
}
void CheckedInternalizedString::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register map_tmp = temps.Acquire();
  Register object = ToRegister(object_input());

  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kWrongMap, this);
  }

  __ LoadMap(map_tmp, object);
  __ RecordComment("Test IsInternalizedString");
  // Go to the slow path if this is a non-string, or a non-internalised string.
  __ testw(FieldOperand(map_tmp, Map::kInstanceTypeOffset),
           Immediate(kIsNotStringMask | kIsNotInternalizedMask));
  static_assert((kStringTag | kInternalizedTag) == 0);
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(
      not_zero,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register object,
         CheckedInternalizedString* node, EagerDeoptInfo* deopt_info,
         Register map_tmp) {
        __ RecordComment("Deferred Test IsThinString");
        __ movw(map_tmp, FieldOperand(map_tmp, Map::kInstanceTypeOffset));
        static_assert(kThinStringTagBit > 0);
        // Deopt if this isn't a string.
        __ testw(map_tmp, Immediate(kIsNotStringMask));
        __ EmitEagerDeoptIf(not_zero, DeoptimizeReason::kWrongMap, node);
        // Deopt if this isn't a thin string.
        __ testb(map_tmp, Immediate(kThinStringTagBit));
        __ EmitEagerDeoptIf(zero, DeoptimizeReason::kWrongMap, node);
        __ LoadTaggedPointerField(
            object, FieldOperand(object, ThinString::kActualOffset));
        if (v8_flags.debug_code) {
          __ RecordComment("DCHECK IsInternalizedString");
          __ LoadMap(map_tmp, object);
          __ testw(FieldOperand(map_tmp, Map::kInstanceTypeOffset),
                   Immediate(kIsNotStringMask | kIsNotInternalizedMask));
          static_assert((kStringTag | kInternalizedTag) == 0);
          __ Check(zero, AbortReason::kUnexpectedValue);
        }
        __ jmp(*done);
      },
      done, object, this, eager_deopt_info(), map_tmp);
  __ bind(*done);
}

int CheckedObjectToIndex::MaxCallStackArgs() const {
  return MaglevAssembler::ArgumentStackSlotsForCFunctionCall(1);
}
void CheckedObjectToIndex::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineAsRegister(this);
  set_double_temporaries_needed(1);
}
void CheckedObjectToIndex::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register result_reg = ToRegister(result());

  ZoneLabelRef done(masm);
  Condition is_smi = __ CheckSmi(object);
  __ JumpToDeferredIf(
      NegateCondition(is_smi),
      [](MaglevAssembler* masm, Register object, Register result_reg,
         ZoneLabelRef done, CheckedObjectToIndex* node) {
        Label is_string;
        __ LoadMap(kScratchRegister, object);
        __ CmpInstanceTypeRange(kScratchRegister, kScratchRegister,
                                FIRST_STRING_TYPE, LAST_STRING_TYPE);
        __ j(below_equal, &is_string);

        __ cmpw(kScratchRegister, Immediate(HEAP_NUMBER_TYPE));
        // The IC will go generic if it encounters something other than a
        // Number or String key.
        __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotInt32, node);

        // Heap Number.
        {
          MaglevAssembler::ScratchRegisterScope temps(masm);
          DoubleRegister number_value = temps.AcquireDouble();
          DoubleRegister converted_back = kScratchDoubleReg;
          // Load the heap number value into a double register.
          __ Movsd(number_value,
                   FieldOperand(object, HeapNumber::kValueOffset));
          // Convert the input float64 value to int32.
          __ Cvttsd2si(result_reg, number_value);
          // Convert that int32 value back to float64.
          __ Cvtlsi2sd(converted_back, result_reg);
          // Check that the result of the float64->int32->float64 is equal to
          // the input (i.e. that the conversion didn't truncate.
          __ Ucomisd(number_value, converted_back);
          __ EmitEagerDeoptIf(parity_even, DeoptimizeReason::kNotInt32, node);
          __ j(equal, *done);
          __ EmitEagerDeopt(node, DeoptimizeReason::kNotInt32);
        }

        // String.
        __ bind(&is_string);
        {
          RegisterSnapshot snapshot = node->register_snapshot();
          snapshot.live_registers.clear(result_reg);
          DCHECK(!snapshot.live_tagged_registers.has(result_reg));
          {
            SaveRegisterStateForCall save_register_state(masm, snapshot);
            AllowExternalCallThatCantCauseGC scope(masm);
            __ PrepareCallCFunction(1);
            __ Move(arg_reg_1, object);
            __ CallCFunction(
                ExternalReference::string_to_array_index_function(), 1);
            // No need for safepoint since this is a fast C call.
            __ Move(result_reg, kReturnRegister0);
          }
          __ cmpl(result_reg, Immediate(0));
          __ j(greater_equal, *done);
          __ EmitEagerDeopt(node, DeoptimizeReason::kNotInt32);
        }
      },
      object, result_reg, done, this);

  // If we didn't enter the deferred block, we're a Smi.
  if (result_reg == object) {
    __ SmiToInt32(result_reg);
  } else {
    __ SmiToInt32(result_reg, object);
  }

  __ bind(*done);
}

int BuiltinStringFromCharCode::MaxCallStackArgs() const {
  return AllocateDescriptor::GetStackParameterCount();
}
void BuiltinStringFromCharCode::SetValueLocationConstraints() {
  if (code_input().node()->Is<Int32Constant>()) {
    UseAny(code_input());
  } else {
    UseAndClobberRegister(code_input());
    set_temporaries_needed(1);
  }
  DefineAsRegister(this);
}
void BuiltinStringFromCharCode::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register result_string = ToRegister(result());
  if (Int32Constant* constant = code_input().node()->TryCast<Int32Constant>()) {
    int32_t char_code = constant->value();
    if (0 <= char_code && char_code < String::kMaxOneByteCharCode) {
      __ LoadSingleCharacterString(result_string, char_code);
    } else {
      __ AllocateTwoByteString(register_snapshot(), result_string, 1);
      __ movw(FieldOperand(result_string, SeqTwoByteString::kHeaderSize),
              Immediate(char_code & 0xFFFF));
    }
  } else {
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    Register char_code = ToRegister(code_input());
    __ StringFromCharCode(register_snapshot(), nullptr, result_string,
                          char_code, scratch);
  }
}

int BuiltinStringPrototypeCharCodeAt::MaxCallStackArgs() const {
  DCHECK_EQ(Runtime::FunctionForId(Runtime::kStringCharCodeAt)->nargs, 2);
  return 2;
}
void BuiltinStringPrototypeCharCodeAt::SetValueLocationConstraints() {
  UseAndClobberRegister(string_input());
  UseAndClobberRegister(index_input());
  DefineAsRegister(this);
  set_temporaries_needed(1);
}

void BuiltinStringPrototypeCharCodeAt::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  Register string = ToRegister(string_input());
  Register index = ToRegister(index_input());
  ZoneLabelRef done(masm);
  RegisterSnapshot save_registers = register_snapshot();
  __ StringCharCodeAt(save_registers, ToRegister(result()), string, index,
                      scratch, *done);
  __ bind(*done);
}

void LoadDoubleField::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineAsRegister(this);
  set_temporaries_needed(1);
}
void LoadDoubleField::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register tmp = temps.Acquire();
  Register object = ToRegister(object_input());
  __ AssertNotSmi(object);
  __ DecompressAnyTagged(tmp, FieldOperand(object, offset()));
  __ AssertNotSmi(tmp);
  __ Movsd(ToDoubleRegister(result()),
           FieldOperand(tmp, HeapNumber::kValueOffset));
}

void LoadFixedArrayElement::SetValueLocationConstraints() {
  UseRegister(elements_input());
  UseRegister(index_input());
  DefineAsRegister(this);
}
void LoadFixedArrayElement::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  Register elements = ToRegister(elements_input());
  Register index = ToRegister(index_input());
  Register result_reg = ToRegister(result());
  if (v8_flags.debug_code) {
    __ AssertNotSmi(elements);
    __ CmpObjectType(elements, FIXED_ARRAY_TYPE, kScratchRegister);
    __ Assert(equal, AbortReason::kUnexpectedValue);
    __ cmpq(index, Immediate(0));
    __ Assert(above_equal, AbortReason::kUnexpectedNegativeValue);
  }
  __ DecompressAnyTagged(result_reg,
                         FieldOperand(elements, index, times_tagged_size,
                                      FixedArray::kHeaderSize));
}

void LoadFixedDoubleArrayElement::SetValueLocationConstraints() {
  UseRegister(elements_input());
  UseRegister(index_input());
  DefineAsRegister(this);
}
void LoadFixedDoubleArrayElement::GenerateCode(MaglevAssembler* masm,
                                               const ProcessingState& state) {
  Register elements = ToRegister(elements_input());
  Register index = ToRegister(index_input());
  DoubleRegister result_reg = ToDoubleRegister(result());
  if (v8_flags.debug_code) {
    __ AssertNotSmi(elements);
    __ CmpObjectType(elements, FIXED_DOUBLE_ARRAY_TYPE, kScratchRegister);
    __ Assert(equal, AbortReason::kUnexpectedValue);
    __ cmpq(index, Immediate(0));
    __ Assert(above_equal, AbortReason::kUnexpectedNegativeValue);
  }
  __ Movsd(result_reg, FieldOperand(elements, index, times_8,
                                    FixedDoubleArray::kHeaderSize));
}

void LoadSignedIntDataViewElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  if (is_little_endian_constant() ||
      type_ == ExternalArrayType::kExternalInt8Array) {
    UseAny(is_little_endian_input());
  } else {
    UseRegister(is_little_endian_input());
  }
  set_temporaries_needed(1);
  DefineAsRegister(this);
}
void LoadSignedIntDataViewElement::GenerateCode(MaglevAssembler* masm,
                                                const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  Register result_reg = ToRegister(result());
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register data_pointer = temps.Acquire();

  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    __ CmpObjectType(object, JS_DATA_VIEW_TYPE, kScratchRegister);
    __ Assert(above_equal, AbortReason::kUnexpectedValue);
  }

  // Load data pointer.
  __ LoadExternalPointerField(
      data_pointer, FieldOperand(object, JSDataView::kDataPointerOffset));

  int element_size = ExternalArrayElementSize(type_);
  __ LoadSignedField(result_reg, Operand(data_pointer, index, times_1, 0),
                     element_size);

  // We ignore little endian argument if type is a byte size.
  if (type_ != ExternalArrayType::kExternalInt8Array) {
    if (is_little_endian_constant()) {
      if (!FromConstantToBool(masm, is_little_endian_input().node())) {
        __ ReverseByteOrder(result_reg, element_size);
      }
    } else {
      ZoneLabelRef is_little_endian(masm), is_big_endian(masm);
      __ ToBoolean(ToRegister(is_little_endian_input()), is_little_endian,
                   is_big_endian, false);
      __ bind(*is_big_endian);
      __ ReverseByteOrder(result_reg, element_size);
      __ bind(*is_little_endian);
      // x64 is little endian.
      static_assert(V8_TARGET_LITTLE_ENDIAN == 1);
    }
  }
}

void StoreSignedIntDataViewElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  if (ExternalArrayElementSize(type_) > 1) {
    UseAndClobberRegister(value_input());
  } else {
    UseRegister(value_input());
  }
  if (is_little_endian_constant() ||
      type_ == ExternalArrayType::kExternalInt8Array) {
    UseAny(is_little_endian_input());
  } else {
    UseRegister(is_little_endian_input());
  }
  set_temporaries_needed(1);
}
void StoreSignedIntDataViewElement::GenerateCode(MaglevAssembler* masm,
                                                 const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  Register value = ToRegister(value_input());
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register data_pointer = temps.Acquire();

  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    __ CmpObjectType(object, JS_DATA_VIEW_TYPE, kScratchRegister);
    __ Assert(above_equal, AbortReason::kUnexpectedValue);
  }

  // Load data pointer.
  __ LoadExternalPointerField(
      data_pointer, FieldOperand(object, JSDataView::kDataPointerOffset));

  int element_size = ExternalArrayElementSize(type_);

  // We ignore little endian argument if type is a byte size.
  if (element_size > 1) {
    if (is_little_endian_constant()) {
      if (!FromConstantToBool(masm, is_little_endian_input().node())) {
        __ ReverseByteOrder(value, element_size);
      }
    } else {
      ZoneLabelRef is_little_endian(masm), is_big_endian(masm);
      __ ToBoolean(ToRegister(is_little_endian_input()), is_little_endian,
                   is_big_endian, false);
      __ bind(*is_big_endian);
      __ ReverseByteOrder(value, element_size);
      __ bind(*is_little_endian);
      // x64 is little endian.
      static_assert(V8_TARGET_LITTLE_ENDIAN == 1);
    }
  }

  __ StoreField(Operand(data_pointer, index, times_1, 0), value, element_size);
}

void LoadDoubleDataViewElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  if (is_little_endian_constant()) {
    UseAny(is_little_endian_input());
  } else {
    UseRegister(is_little_endian_input());
  }
  set_temporaries_needed(1);
  DefineAsRegister(this);
}
void LoadDoubleDataViewElement::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  DoubleRegister result_reg = ToDoubleRegister(result());
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register data_pointer = temps.Acquire();

  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    __ CmpObjectType(object, JS_DATA_VIEW_TYPE, kScratchRegister);
    __ Assert(above_equal, AbortReason::kUnexpectedValue);
  }

  // Load data pointer.
  __ LoadExternalPointerField(
      data_pointer, FieldOperand(object, JSDataView::kDataPointerOffset));

  if (is_little_endian_constant()) {
    if (FromConstantToBool(masm, is_little_endian_input().node())) {
      __ Movsd(result_reg, Operand(data_pointer, index, times_1, 0));
    } else {
      __ movq(kScratchRegister, Operand(data_pointer, index, times_1, 0));
      __ bswapq(kScratchRegister);
      __ Movq(result_reg, kScratchRegister);
    }
  } else {
    Label done;
    ZoneLabelRef is_little_endian(masm), is_big_endian(masm);
    // TODO(leszeks): We're likely to be calling this on an existing boolean --
    // maybe that's a case we should fast-path here and re-use that boolean
    // value?
    __ ToBoolean(ToRegister(is_little_endian_input()), is_little_endian,
                 is_big_endian, true);
    // x64 is little endian.
    static_assert(V8_TARGET_LITTLE_ENDIAN == 1);
    __ bind(*is_little_endian);
    __ Movsd(result_reg, Operand(data_pointer, index, times_1, 0));
    __ jmp(&done);
    // We should swap the bytes if big endian.
    __ bind(*is_big_endian);
    __ movq(kScratchRegister, Operand(data_pointer, index, times_1, 0));
    __ bswapq(kScratchRegister);
    __ Movq(result_reg, kScratchRegister);
    __ bind(&done);
  }
}

void StoreDoubleDataViewElement::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(index_input());
  UseRegister(value_input());
  if (is_little_endian_constant()) {
    UseAny(is_little_endian_input());
  } else {
    UseRegister(is_little_endian_input());
  }
  set_temporaries_needed(1);
}
void StoreDoubleDataViewElement::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  DoubleRegister value = ToDoubleRegister(value_input());
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register data_pointer = temps.Acquire();

  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    __ CmpObjectType(object, JS_DATA_VIEW_TYPE, kScratchRegister);
    __ Assert(above_equal, AbortReason::kUnexpectedValue);
  }

  // Load data pointer.
  __ LoadExternalPointerField(
      data_pointer, FieldOperand(object, JSDataView::kDataPointerOffset));

  if (is_little_endian_constant()) {
    if (FromConstantToBool(masm, is_little_endian_input().node())) {
      __ Movsd(Operand(data_pointer, index, times_1, 0), value);
    } else {
      __ Movq(kScratchRegister, value);
      __ bswapq(kScratchRegister);
      __ movq(Operand(data_pointer, index, times_1, 0), kScratchRegister);
    }
  } else {
    Label done;
    ZoneLabelRef is_little_endian(masm), is_big_endian(masm);
    // TODO(leszeks): We're likely to be calling this on an existing boolean --
    // maybe that's a case we should fast-path here and re-use that boolean
    // value?
    __ ToBoolean(ToRegister(is_little_endian_input()), is_little_endian,
                 is_big_endian, true);
    // x64 is little endian.
    static_assert(V8_TARGET_LITTLE_ENDIAN == 1);
    __ bind(*is_little_endian);
    __ Movsd(Operand(data_pointer, index, times_1, 0), value);
    __ jmp(&done);
    // We should swap the bytes if big endian.
    __ bind(*is_big_endian);
    __ Movq(kScratchRegister, value);
    __ bswapq(kScratchRegister);
    __ movq(Operand(data_pointer, index, times_1, 0), kScratchRegister);
    __ bind(&done);
  }
}

namespace {

ScaleFactor ScaleFactorFromInt(int n) {
  switch (n) {
    case 1:
      return times_1;
    case 2:
      return times_2;
    case 4:
      return times_4;
    default:
      UNREACHABLE();
  }
}

template <bool check_detached, typename ResultReg, typename NodeT>
void GenerateTypedArrayLoad(MaglevAssembler* masm, NodeT* node, Register object,
                            Register index, ResultReg result_reg,
                            Register scratch, ElementsKind kind) {
  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    __ CmpObjectType(object, JS_TYPED_ARRAY_TYPE, kScratchRegister);
    __ Assert(equal, AbortReason::kUnexpectedValue);
  }

  if constexpr (check_detached) {
    __ DeoptIfBufferDetached(object, scratch, node);
  }

  Register data_pointer = scratch;
  __ BuildTypedArrayDataPointer(data_pointer, object);

  if constexpr (std::is_same_v<ResultReg, Register>) {
    if (IsSignedIntTypedArrayElementsKind(kind)) {
      int element_size = ElementsKindSize(kind);
      __ LoadSignedField(
          result_reg,
          Operand(data_pointer, index, ScaleFactorFromInt(element_size), 0),
          element_size);
    } else {
      DCHECK(IsUnsignedIntTypedArrayElementsKind(kind));
      int element_size = ElementsKindSize(kind);
      __ LoadUnsignedField(
          result_reg,
          Operand(data_pointer, index, ScaleFactorFromInt(element_size), 0),
          element_size);
    }
  } else {
#ifdef DEBUG
    bool result_reg_is_double = std::is_same_v<ResultReg, DoubleRegister>;
    DCHECK(result_reg_is_double);
    DCHECK(IsFloatTypedArrayElementsKind(kind));
#endif
    switch (kind) {
      case FLOAT32_ELEMENTS:
        __ Movss(result_reg, Operand(data_pointer, index, times_4, 0));
        __ Cvtss2sd(result_reg, result_reg);
        break;
      case FLOAT64_ELEMENTS:
        __ Movsd(result_reg, Operand(data_pointer, index, times_8, 0));
        break;
      default:
        UNREACHABLE();
    }
  }
}

}  // namespace

#define DEF_OPERATION(Name, ResultReg, ToResultReg, check_detached)      \
  void Name::SetValueLocationConstraints() {                             \
    UseRegister(object_input());                                         \
    UseRegister(index_input());                                          \
    DefineAsRegister(this);                                              \
    set_temporaries_needed(1);                                           \
  }                                                                      \
  void Name::GenerateCode(MaglevAssembler* masm,                         \
                          const ProcessingState& state) {                \
    Register object = ToRegister(object_input());                        \
    Register index = ToRegister(index_input());                          \
    ResultReg result_reg = ToResultReg(result());                        \
    MaglevAssembler::ScratchRegisterScope temps(masm);                   \
    Register scratch = temps.Acquire();                                  \
                                                                         \
    GenerateTypedArrayLoad<check_detached>(                              \
        masm, this, object, index, result_reg, scratch, elements_kind_); \
  }

DEF_OPERATION(LoadSignedIntTypedArrayElement, Register, ToRegister,
              /*check_detached*/ true)
DEF_OPERATION(LoadSignedIntTypedArrayElementNoDeopt, Register, ToRegister,
              /*check_detached*/ false)

DEF_OPERATION(LoadUnsignedIntTypedArrayElement, Register, ToRegister,
              /*check_detached*/ true)
DEF_OPERATION(LoadUnsignedIntTypedArrayElementNoDeopt, Register, ToRegister,
              /*check_detached*/ false)

DEF_OPERATION(LoadDoubleTypedArrayElement, DoubleRegister, ToDoubleRegister,
              /*check_detached*/ true)
DEF_OPERATION(LoadDoubleTypedArrayElementNoDeopt, DoubleRegister,
              ToDoubleRegister, /*check_detached*/ false)
#undef DEF_OPERATION

void StoreDoubleField::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(value_input());
  set_temporaries_needed(1);
}
void StoreDoubleField::GenerateCode(MaglevAssembler* masm,
                                    const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register tmp = temps.Acquire();
  Register object = ToRegister(object_input());
  DoubleRegister value = ToDoubleRegister(value_input());

  __ AssertNotSmi(object);
  __ DecompressAnyTagged(tmp, FieldOperand(object, offset()));
  __ AssertNotSmi(tmp);
  __ Movsd(FieldOperand(tmp, HeapNumber::kValueOffset), value);
}

int StoreMap::MaxCallStackArgs() const {
  return WriteBarrierDescriptor::GetStackParameterCount();
}
void StoreMap::SetValueLocationConstraints() {
  UseFixed(object_input(), WriteBarrierDescriptor::ObjectRegister());
}
void StoreMap::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
  // TODO(leszeks): Consider making this an arbitrary register and push/popping
  // in the deferred path.
  Register object = WriteBarrierDescriptor::ObjectRegister();
  DCHECK_EQ(object, ToRegister(object_input()));

  __ AssertNotSmi(object);
  Register value = kScratchRegister;
  __ Move(value, map_.object());
  __ StoreTaggedField(FieldOperand(object, HeapObject::kMapOffset),
                      kScratchRegister);

  ZoneLabelRef done(masm);
  DeferredCodeInfo* deferred_write_barrier = __ PushDeferredCode(
      [](MaglevAssembler* masm, ZoneLabelRef done, Register value,
         Register object, StoreMap* node) {
        ASM_CODE_COMMENT_STRING(masm, "Write barrier slow path");
        __ CheckPageFlag(
            value, kScratchRegister,
            MemoryChunk::kPointersToHereAreInterestingOrInSharedHeapMask, zero,
            *done);

        Register slot_reg = WriteBarrierDescriptor::SlotAddressRegister();
        RegList saved;
        if (node->register_snapshot().live_registers.has(slot_reg)) {
          saved.set(slot_reg);
        }

        __ PushAll(saved);
        __ leaq(slot_reg, FieldOperand(object, HeapObject::kMapOffset));

        SaveFPRegsMode const save_fp_mode =
            !node->register_snapshot().live_double_registers.is_empty()
                ? SaveFPRegsMode::kSave
                : SaveFPRegsMode::kIgnore;

        __ CallRecordWriteStub(object, slot_reg, save_fp_mode);

        __ PopAll(saved);
        __ jmp(*done);
      },
      done, value, object, this);

  __ JumpIfSmi(value, *done);
  __ CheckPageFlag(object, kScratchRegister,
                   MemoryChunk::kPointersFromHereAreInterestingMask, not_zero,
                   &deferred_write_barrier->deferred_code_label);
  __ bind(*done);
}

int StoreTaggedFieldWithWriteBarrier::MaxCallStackArgs() const {
  return WriteBarrierDescriptor::GetStackParameterCount();
}
void StoreTaggedFieldWithWriteBarrier::SetValueLocationConstraints() {
  UseFixed(object_input(), WriteBarrierDescriptor::ObjectRegister());
  UseRegister(value_input());
}
void StoreTaggedFieldWithWriteBarrier::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  // TODO(leszeks): Consider making this an arbitrary register and push/popping
  // in the deferred path.
  Register object = WriteBarrierDescriptor::ObjectRegister();
  DCHECK_EQ(object, ToRegister(object_input()));

  Register value = ToRegister(value_input());

  __ AssertNotSmi(object);
  __ StoreTaggedField(FieldOperand(object, offset()), value);

  ZoneLabelRef done(masm);
  DeferredCodeInfo* deferred_write_barrier = __ PushDeferredCode(
      [](MaglevAssembler* masm, ZoneLabelRef done, Register value,
         Register object, StoreTaggedFieldWithWriteBarrier* node) {
        ASM_CODE_COMMENT_STRING(masm, "Write barrier slow path");
        __ CheckPageFlag(
            value, kScratchRegister,
            MemoryChunk::kPointersToHereAreInterestingOrInSharedHeapMask, zero,
            *done);

        Register slot_reg = WriteBarrierDescriptor::SlotAddressRegister();
        RegList saved;
        if (node->register_snapshot().live_registers.has(slot_reg)) {
          saved.set(slot_reg);
        }

        __ PushAll(saved);
        __ leaq(slot_reg, FieldOperand(object, node->offset()));

        SaveFPRegsMode const save_fp_mode =
            !node->register_snapshot().live_double_registers.is_empty()
                ? SaveFPRegsMode::kSave
                : SaveFPRegsMode::kIgnore;

        __ CallRecordWriteStub(object, slot_reg, save_fp_mode);

        __ PopAll(saved);
        __ jmp(*done);
      },
      done, value, object, this);

  __ JumpIfSmi(value, *done);
  __ CheckPageFlag(object, kScratchRegister,
                   MemoryChunk::kPointersFromHereAreInterestingMask, not_zero,
                   &deferred_write_barrier->deferred_code_label);
  __ bind(*done);
}

void StringLength::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineAsRegister(this);
}
void StringLength::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  Register object = ToRegister(object_input());
  if (v8_flags.debug_code) {
    // Use return register as temporary. Push it in case it aliases the object
    // register.
    Register tmp = ToRegister(result());
    __ Push(tmp);
    // Check if {object} is a string.
    __ AssertNotSmi(object);
    __ LoadMap(tmp, object);
    __ CmpInstanceTypeRange(tmp, tmp, FIRST_STRING_TYPE, LAST_STRING_TYPE);
    __ Check(below_equal, AbortReason::kUnexpectedValue);
    __ Pop(tmp);
  }
  __ movl(ToRegister(result()), FieldOperand(object, String::kLengthOffset));
}

void Int32AddWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
}

void Int32AddWithOverflow::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ addl(left, right);
  // None of the mutated input registers should be a register input into the
  // eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{left} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void Int32SubtractWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
}

void Int32SubtractWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ subl(left, right);
  // None of the mutated input registers should be a register input into the
  // eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{left} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void Int32MultiplyWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
  set_temporaries_needed(1);
}

void Int32MultiplyWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register result = ToRegister(this->result());
  Register right = ToRegister(right_input());
  DCHECK_EQ(result, ToRegister(left_input()));

  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register saved_left = temps.Acquire();
  __ movl(saved_left, result);
  // TODO(leszeks): peephole optimise multiplication by a constant.
  __ imull(result, right);
  // None of the mutated input registers should be a register input into the
  // eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{saved_left, result} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);

  // If the result is zero, check if either lhs or rhs is negative.
  Label end;
  __ cmpl(result, Immediate(0));
  __ j(not_zero, &end);
  {
    __ orl(saved_left, right);
    __ cmpl(saved_left, Immediate(0));
    // If one of them is negative, we must have a -0 result, which is non-int32,
    // so deopt.
    // TODO(leszeks): Consider splitting these deopts to have distinct deopt
    // reasons. Otherwise, the reason has to match the above.
    __ EmitEagerDeoptIf(less, DeoptimizeReason::kOverflow, this);
  }
  __ bind(&end);
}

void Int32ModulusWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseAndClobberRegister(right_input());
  DefineAsFixed(this, rdx);
  // rax,rdx are clobbered by div.
  RequireSpecificTemporary(rax);
  RequireSpecificTemporary(rdx);
}

void Int32ModulusWithOverflow::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
  // If AreAliased(lhs, rhs):
  //   deopt if lhs < 0  // Minus zero.
  //   0
  //
  // Otherwise, use the same algorithm as in EffectControlLinearizer:
  //   if rhs <= 0 then
  //     rhs = -rhs
  //     deopt if rhs == 0
  //   if lhs < 0 then
  //     let lhs_abs = -lhs in
  //     let res = lhs_abs % rhs in
  //     deopt if res == 0
  //     -res
  //   else
  //     let msk = rhs - 1 in
  //     if rhs & msk == 0 then
  //       lhs & msk
  //     else
  //       lhs % rhs

  Register lhs = ToRegister(left_input());
  Register rhs = ToRegister(right_input());

  static constexpr DeoptimizeReason deopt_reason =
      DeoptimizeReason::kDivisionByZero;

  if (lhs == rhs) {
    // For the modulus algorithm described above, lhs and rhs must not alias
    // each other.
    __ testl(lhs, lhs);
    // TODO(victorgomes): This ideally should be kMinusZero, but Maglev only
    // allows one deopt reason per IR.
    __ EmitEagerDeoptIf(negative, deopt_reason, this);
    __ Move(ToRegister(result()), 0);
    return;
  }

  DCHECK(!AreAliased(lhs, rhs, rax, rdx));

  ZoneLabelRef done(masm);
  ZoneLabelRef rhs_checked(masm);

  __ cmpl(rhs, Immediate(0));
  __ JumpToDeferredIf(
      less_equal,
      [](MaglevAssembler* masm, ZoneLabelRef rhs_checked, Register rhs,
         Int32ModulusWithOverflow* node) {
        __ negl(rhs);
        __ EmitEagerDeoptIf(zero, deopt_reason, node);
        __ jmp(*rhs_checked);
      },
      rhs_checked, rhs, this);
  __ bind(*rhs_checked);

  __ cmpl(lhs, Immediate(0));
  __ JumpToDeferredIf(
      less,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register lhs, Register rhs,
         Int32ModulusWithOverflow* node) {
        // `divl(divisor)` divides rdx:rax by the divisor and stores the
        // quotient in rax, the remainder in rdx.
        __ movl(rax, lhs);
        __ negl(rax);
        __ xorl(rdx, rdx);
        __ divl(rhs);
        __ testl(rdx, rdx);
        // TODO(victorgomes): This ideally should be kMinusZero, but Maglev only
        // allows one deopt reason per IR.
        __ EmitEagerDeoptIf(equal, deopt_reason, node);
        __ negl(rdx);
        __ jmp(*done);
      },
      done, lhs, rhs, this);

  Label rhs_not_power_of_2;
  Register mask = rax;
  __ leal(mask, Operand(rhs, -1));
  __ testl(rhs, mask);
  __ j(not_zero, &rhs_not_power_of_2, Label::kNear);

  // {rhs} is power of 2.
  __ andl(mask, lhs);
  __ movl(ToRegister(result()), mask);
  __ jmp(*done, Label::kNear);

  __ bind(&rhs_not_power_of_2);
  // `divl(divisor)` divides rdx:rax by the divisor and stores the
  // quotient in rax, the remainder in rdx.
  __ movl(rax, lhs);
  __ xorl(rdx, rdx);
  __ divl(rhs);
  // Result is implicitly written to rdx.
  DCHECK_EQ(ToRegister(result()), rdx);

  __ bind(*done);
}

void Int32DivideWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsFixed(this, rax);
  // rax,rdx are clobbered by idiv.
  RequireSpecificTemporary(rax);
  RequireSpecificTemporary(rdx);
}

void Int32DivideWithOverflow::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ movl(rax, left);

  // TODO(leszeks): peephole optimise division by a constant.

  // Sign extend eax into edx.
  __ cdq();

  // Pre-check for overflow, since idiv throws a division exception on overflow
  // rather than setting the overflow flag. Logic copied from
  // effect-control-linearizer.cc

  // Check if {right} is positive (and not zero).
  __ cmpl(right, Immediate(0));
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(
      less_equal,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register right,
         Int32DivideWithOverflow* node) {
        // {right} is negative or zero.

        // Check if {right} is zero.
        // We've already done the compare and flags won't be cleared yet.
        // TODO(leszeks): Using kNotInt32 here, but kDivisionByZero would be
        // better. Right now all eager deopts in a node have to be the same --
        // we should allow a node to emit multiple eager deopts with different
        // reasons.
        __ EmitEagerDeoptIf(equal, DeoptimizeReason::kNotInt32, node);

        // Check if {left} is zero, as that would produce minus zero. Left is in
        // rax already.
        __ cmpl(rax, Immediate(0));
        // TODO(leszeks): Better DeoptimizeReason = kMinusZero.
        __ EmitEagerDeoptIf(equal, DeoptimizeReason::kNotInt32, node);

        // Check if {left} is kMinInt and {right} is -1, in which case we'd have
        // to return -kMinInt, which is not representable as Int32.
        __ cmpl(rax, Immediate(kMinInt));
        __ j(not_equal, *done);
        __ cmpl(right, Immediate(-1));
        __ j(not_equal, *done);
        // TODO(leszeks): Better DeoptimizeReason = kOverflow, but
        // eager_deopt_info is already configured as kNotInt32.
        __ EmitEagerDeopt(node, DeoptimizeReason::kNotInt32);
      },
      done, right, this);
  __ bind(*done);

  // Perform the actual integer division.
  __ idivl(right);

  // Check that the remainder is zero.
  __ cmpl(rdx, Immediate(0));
  // None of the mutated input registers should be a register input into the
  // eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{rax, rdx} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotInt32, this);
  DCHECK_EQ(ToRegister(result()), rax);
}

void Int32BitwiseAnd::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
}

void Int32BitwiseAnd::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ andl(left, right);
}

void Int32BitwiseOr::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
}

void Int32BitwiseOr::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ orl(left, right);
}

void Int32BitwiseXor::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
}

void Int32BitwiseXor::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  __ xorl(left, right);
}

void Int32ShiftLeft::SetValueLocationConstraints() {
  UseRegister(left_input());
  // Use the "shift by cl" variant of shl.
  // TODO(leszeks): peephole optimise shifts by a constant.
  UseFixed(right_input(), rcx);
  DefineSameAsFirst(this);
}

void Int32ShiftLeft::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  Register left = ToRegister(left_input());
  DCHECK_EQ(rcx, ToRegister(right_input()));
  __ shll_cl(left);
}

void Int32ShiftRight::SetValueLocationConstraints() {
  UseRegister(left_input());
  // Use the "shift by cl" variant of sar.
  // TODO(leszeks): peephole optimise shifts by a constant.
  UseFixed(right_input(), rcx);
  DefineSameAsFirst(this);
}

void Int32ShiftRight::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register left = ToRegister(left_input());
  DCHECK_EQ(rcx, ToRegister(right_input()));
  __ sarl_cl(left);
}

void Int32ShiftRightLogical::SetValueLocationConstraints() {
  UseRegister(left_input());
  // Use the "shift by cl" variant of shr.
  // TODO(leszeks): peephole optimise shifts by a constant.
  UseFixed(right_input(), rcx);
  DefineSameAsFirst(this);
}

void Int32ShiftRightLogical::GenerateCode(MaglevAssembler* masm,
                                          const ProcessingState& state) {
  Register left = ToRegister(left_input());
  DCHECK_EQ(rcx, ToRegister(right_input()));
  __ shrl_cl(left);
}

void Int32IncrementWithOverflow::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineSameAsFirst(this);
}

void Int32IncrementWithOverflow::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register value = ToRegister(value_input());
  __ incl(value);
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void Int32DecrementWithOverflow::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineSameAsFirst(this);
}

void Int32DecrementWithOverflow::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register value = ToRegister(value_input());
  __ decl(value);
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void Int32NegateWithOverflow::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineSameAsFirst(this);
}

void Int32NegateWithOverflow::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register value = ToRegister(value_input());
  // Deopt when the result would be -0.
  __ testl(value, value);
  __ EmitEagerDeoptIf(zero, DeoptimizeReason::kOverflow, this);

  __ negl(value);
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void Int32BitwiseNot::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineSameAsFirst(this);
}

void Int32BitwiseNot::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register value = ToRegister(value_input());
  __ notl(value);
}

template <class Derived, Operation kOperation>
void Int32CompareNode<Derived, kOperation>::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

template <class Derived, Operation kOperation>
void Int32CompareNode<Derived, kOperation>::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  Register left = ToRegister(left_input());
  Register right = ToRegister(right_input());
  Register result = ToRegister(this->result());
  Label is_true, end;
  __ cmpl(left, right);
  // TODO(leszeks): Investigate using cmov here.
  __ j(ConditionFor(kOperation), &is_true);
  // TODO(leszeks): Investigate loading existing materialisations of roots here,
  // if available.
  __ LoadRoot(result, RootIndex::kFalseValue);
  __ jmp(&end);
  {
    __ bind(&is_true);
    __ LoadRoot(result, RootIndex::kTrueValue);
  }
  __ bind(&end);
}

#define DEF_OPERATION(Name)                               \
  void Name::SetValueLocationConstraints() {              \
    Base::SetValueLocationConstraints();                  \
  }                                                       \
  void Name::GenerateCode(MaglevAssembler* masm,          \
                          const ProcessingState& state) { \
    Base::GenerateCode(masm, state);                      \
  }
DEF_OPERATION(Int32Equal)
DEF_OPERATION(Int32StrictEqual)
DEF_OPERATION(Int32LessThan)
DEF_OPERATION(Int32LessThanOrEqual)
DEF_OPERATION(Int32GreaterThan)
DEF_OPERATION(Int32GreaterThanOrEqual)
#undef DEF_OPERATION

void Float64Add::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
}

void Float64Add::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  __ Addsd(left, right);
}

void Float64Subtract::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
}

void Float64Subtract::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  __ Subsd(left, right);
}

void Float64Multiply::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
}

void Float64Multiply::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  __ Mulsd(left, right);
}

void Float64Divide::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineSameAsFirst(this);
}

void Float64Divide::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  __ Divsd(left, right);
}

void Float64Modulus::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  RequireSpecificTemporary(rax);
  DefineAsRegister(this);
}

void Float64Modulus::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  // Approach copied from code-generator-x64.cc
  // Allocate space to use fld to move the value to the FPU stack.
  __ AllocateStackSpace(kDoubleSize);
  Operand scratch_stack_space = Operand(rsp, 0);
  __ Movsd(scratch_stack_space, ToDoubleRegister(right_input()));
  __ fld_d(scratch_stack_space);
  __ Movsd(scratch_stack_space, ToDoubleRegister(left_input()));
  __ fld_d(scratch_stack_space);
  // Loop while fprem isn't done.
  Label mod_loop;
  __ bind(&mod_loop);
  // This instructions traps on all kinds inputs, but we are assuming the
  // floating point control word is set to ignore them all.
  __ fprem();
  // The following 2 instruction implicitly use rax.
  __ fnstsw_ax();
  if (CpuFeatures::IsSupported(SAHF)) {
    CpuFeatureScope sahf_scope(masm, SAHF);
    __ sahf();
  } else {
    __ shrl(rax, Immediate(8));
    __ andl(rax, Immediate(0xFF));
    __ pushq(rax);
    __ popfq();
  }
  __ j(parity_even, &mod_loop);
  // Move output to stack and clean up.
  __ fstp(1);
  __ fstp_d(scratch_stack_space);
  __ Movsd(ToDoubleRegister(result()), scratch_stack_space);
  __ addq(rsp, Immediate(kDoubleSize));
}

void Float64Negate::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}

void Float64Negate::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(input());
  __ Negpd(value, value, kScratchRegister);
}

int Float64Exponentiate::MaxCallStackArgs() const {
  return MaglevAssembler::ArgumentStackSlotsForCFunctionCall(2);
}
void Float64Exponentiate::SetValueLocationConstraints() {
  UseFixed(left_input(), xmm0);
  UseFixed(right_input(), xmm1);
  DefineSameAsFirst(this);
}
void Float64Exponentiate::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(2);
  __ CallCFunction(ExternalReference::ieee754_pow_function(), 2);
}

int Float64Ieee754Unary::MaxCallStackArgs() const {
  return MaglevAssembler::ArgumentStackSlotsForCFunctionCall(1);
}
void Float64Ieee754Unary::SetValueLocationConstraints() {
  UseFixed(input(), xmm0);
  DefineSameAsFirst(this);
}
void Float64Ieee754Unary::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(1);
  __ CallCFunction(ieee_function_, 1);
}

template <class Derived, Operation kOperation>
void Float64CompareNode<Derived, kOperation>::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

template <class Derived, Operation kOperation>
void Float64CompareNode<Derived, kOperation>::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  Register result = ToRegister(this->result());
  Label is_false, end;
  __ Ucomisd(left, right);
  // Ucomisd sets these flags accordingly:
  //   UNORDERED(one of the operands is a NaN): ZF,PF,CF := 111;
  //   GREATER_THAN: ZF,PF,CF := 000;
  //   LESS_THAN: ZF,PF,CF := 001;
  //   EQUAL: ZF,PF,CF := 100;
  // Since ZF can be set by NaN or EQUAL, we check for NaN first.
  __ j(parity_even, &is_false);
  __ j(NegateCondition(ConditionForFloat64(kOperation)), &is_false);
  // TODO(leszeks): Investigate loading existing materialisations of roots here,
  // if available.
  __ LoadRoot(result, RootIndex::kTrueValue);
  __ jmp(&end);
  {
    __ bind(&is_false);
    __ LoadRoot(result, RootIndex::kFalseValue);
  }
  __ bind(&end);
}

#define DEF_OPERATION(Name)                               \
  void Name::SetValueLocationConstraints() {              \
    Base::SetValueLocationConstraints();                  \
  }                                                       \
  void Name::GenerateCode(MaglevAssembler* masm,          \
                          const ProcessingState& state) { \
    Base::GenerateCode(masm, state);                      \
  }
DEF_OPERATION(Float64Equal)
DEF_OPERATION(Float64StrictEqual)
DEF_OPERATION(Float64LessThan)
DEF_OPERATION(Float64LessThanOrEqual)
DEF_OPERATION(Float64GreaterThan)
DEF_OPERATION(Float64GreaterThanOrEqual)
#undef DEF_OPERATION

void CheckInt32IsSmi::SetValueLocationConstraints() { UseRegister(input()); }
void CheckInt32IsSmi::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  // TODO(leszeks): This basically does a SmiTag and throws the result away.
  // Don't throw the result away if we want to actually use it.
  Register reg = ToRegister(input());
  __ movl(kScratchRegister, reg);
  __ addl(kScratchRegister, kScratchRegister);
  DCHECK_REGLIST_EMPTY(RegList{reg} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kNotASmi, this);
}

void CheckedSmiTagInt32::SetValueLocationConstraints() {
  UseAndClobberRegister(input());
  DefineSameAsFirst(this);
}
void CheckedSmiTagInt32::GenerateCode(MaglevAssembler* masm,
                                      const ProcessingState& state) {
  Register reg = ToRegister(input());
  __ addl(reg, reg);
  // None of the mutated input registers should be a register input into the
  // eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{reg} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}

void CheckedSmiTagUint32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void CheckedSmiTagUint32::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  Register reg = ToRegister(input());
  // Perform an unsigned comparison against Smi::kMaxValue.
  __ cmpl(reg, Immediate(Smi::kMaxValue));
  __ EmitEagerDeoptIf(above, DeoptimizeReason::kOverflow, this);
  __ addl(reg, reg);
  __ Assert(no_overflow, AbortReason::kInputDoesNotFitSmi);
}

void UnsafeSmiTag::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void UnsafeSmiTag::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  Register reg = ToRegister(input());
  if (v8_flags.debug_code) {
    if (input().node()->properties().value_representation() ==
        ValueRepresentation::kUint32) {
      __ cmpl(reg, Immediate(Smi::kMaxValue));
      __ Check(below_equal, AbortReason::kInputDoesNotFitSmi);
    }
  }
  __ addl(reg, reg);
  if (v8_flags.debug_code) {
    __ Check(no_overflow, AbortReason::kInputDoesNotFitSmi);
  }
}

void Int32ToNumber::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void Int32ToNumber::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  ZoneLabelRef done(masm);
  Register value = ToRegister(input());
  Register object = ToRegister(result());
  __ movl(kScratchRegister, value);
  __ addl(kScratchRegister, kScratchRegister);
  __ JumpToDeferredIf(
      overflow,
      [](MaglevAssembler* masm, Register object, Register value,
         ZoneLabelRef done, Int32ToNumber* node) {
        DoubleRegister double_value = kScratchDoubleReg;
        __ Cvtlsi2sd(double_value, value);
        __ AllocateHeapNumber(node->register_snapshot(), object, double_value);
        __ jmp(*done);
      },
      object, value, done, this);
  __ Move(object, kScratchRegister);
  __ bind(*done);
}

void Uint32ToNumber::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void Uint32ToNumber::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  ZoneLabelRef done(masm);
  Register value = ToRegister(input());
  Register object = ToRegister(result());
  __ cmpl(value, Immediate(Smi::kMaxValue));
  __ JumpToDeferredIf(
      above,
      [](MaglevAssembler* masm, Register object, Register value,
         ZoneLabelRef done, Uint32ToNumber* node) {
        DoubleRegister double_value = kScratchDoubleReg;
        __ Cvtlui2sd(double_value, value);
        __ AllocateHeapNumber(node->register_snapshot(), object, double_value);
        __ jmp(*done);
      },
      object, value, done, this);
  __ addl(value, value);
  DCHECK_EQ(object, value);
  __ bind(*done);
}

void CheckedFloat64Unbox::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void CheckedFloat64Unbox::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  Register value = ToRegister(input());
  Label is_not_smi, done;
  // Check if Smi.
  __ JumpIfNotSmi(value, &is_not_smi, Label::kNear);
  // If Smi, convert to Float64.
  __ SmiToInt32(value);
  __ Cvtlsi2sd(ToDoubleRegister(result()), value);
  // TODO(v8:7700): Add a constraint to the register allocator to indicate that
  // the value in the input register is "trashed" by this node. Currently we
  // have the invariant that the input register should not be mutated when it is
  // not the same as the output register or the function does not call a
  // builtin. So, we recover the Smi value here.
  __ SmiTag(value);
  __ jmp(&done, Label::kNear);
  __ bind(&is_not_smi);
  // Check if HeapNumber, deopt otherwise.
  __ CompareRoot(FieldOperand(value, HeapObject::kMapOffset),
                 RootIndex::kHeapNumberMap);
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotANumber, this);
  __ Movsd(ToDoubleRegister(result()),
           FieldOperand(value, HeapNumber::kValueOffset));
  __ bind(&done);
}

namespace {

void EmitTruncateNumberToInt32(MaglevAssembler* masm, Register value,
                               Register result_reg, Label* not_a_number) {
  Label is_not_smi, done;
  // Check if Smi.
  __ JumpIfNotSmi(value, &is_not_smi, Label::kNear);
  // If Smi, convert to Int32.
  __ SmiToInt32(value);
  __ jmp(&done, Label::kNear);
  __ bind(&is_not_smi);
  if (not_a_number != nullptr) {
    // Check if HeapNumber, deopt otherwise.
    __ CompareRoot(FieldOperand(value, HeapObject::kMapOffset),
                   RootIndex::kHeapNumberMap);
    __ RecordComment("-- Jump to eager deopt");
    __ JumpIf(not_equal, not_a_number);
  } else if (v8_flags.debug_code) {
    __ CompareRoot(FieldOperand(value, HeapObject::kMapOffset),
                   RootIndex::kHeapNumberMap);
    __ Assert(equal, AbortReason::kUnexpectedValue);
  }
  auto double_value = kScratchDoubleReg;
  __ Movsd(double_value, FieldOperand(value, HeapNumber::kValueOffset));
  __ TruncateDoubleToInt32(result_reg, double_value);
  __ bind(&done);
}

}  // namespace

void CheckedTruncateNumberToInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void CheckedTruncateNumberToInt32::GenerateCode(MaglevAssembler* masm,
                                                const ProcessingState& state) {
  Register value = ToRegister(input());
  Register result_reg = ToRegister(result());
  DCHECK_EQ(value, result_reg);
  Label* deopt_label = __ GetDeoptLabel(this, DeoptimizeReason::kNotANumber);
  EmitTruncateNumberToInt32(masm, value, result_reg, deopt_label);
}

void TruncateNumberToInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void TruncateNumberToInt32::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  Register value = ToRegister(input());
  Register result_reg = ToRegister(result());
  DCHECK_EQ(value, result_reg);
  EmitTruncateNumberToInt32(masm, value, result_reg, nullptr);
}

void SetPendingMessage::SetValueLocationConstraints() {
  UseRegister(value());
  set_temporaries_needed(1);
  DefineAsRegister(this);
}

void SetPendingMessage::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  Register new_message = ToRegister(value());
  Register return_value = ToRegister(result());

  MemOperand pending_message_operand = __ ExternalReferenceAsOperand(
      ExternalReference::address_of_pending_message(masm->isolate()),
      kScratchRegister);

  if (new_message != return_value) {
    __ Move(return_value, pending_message_operand);
    __ movq(pending_message_operand, new_message);
  } else {
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ Move(scratch, pending_message_operand);
    __ movq(pending_message_operand, new_message);
    __ Move(return_value, scratch);
  }
}

void TestUndetectable::SetValueLocationConstraints() {
  UseRegister(value());
  set_temporaries_needed(1);
  DefineAsRegister(this);
}
void TestUndetectable::GenerateCode(MaglevAssembler* masm,
                                    const ProcessingState& state) {
  Register object = ToRegister(value());
  Register return_value = ToRegister(result());
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();

  Label return_false, done;
  __ JumpIfSmi(object, &return_false, Label::kNear);
  // For heap objects, check the map's undetectable bit.
  __ LoadMap(scratch, object);
  __ testl(FieldOperand(scratch, Map::kBitFieldOffset),
           Immediate(Map::Bits1::IsUndetectableBit::kMask));
  __ j(zero, &return_false, Label::kNear);

  __ LoadRoot(return_value, RootIndex::kTrueValue);
  __ jmp(&done, Label::kNear);

  __ bind(&return_false);
  __ LoadRoot(return_value, RootIndex::kFalseValue);

  __ bind(&done);
}

int ToObject::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kToObject>::type;
  return D::GetStackParameterCount();
}
void ToObject::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kToObject>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(value_input(), D::GetRegisterParameter(D::kInput));
  DefineAsFixed(this, kReturnRegister0);
}
void ToObject::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
#ifdef DEBUG
  using D = CallInterfaceDescriptorFor<Builtin::kToObject>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(value_input()), D::GetRegisterParameter(D::kInput));
#endif  // DEBUG
  Register value = ToRegister(value_input());
  Label call_builtin, done;
  // Avoid the builtin call if {value} is a JSReceiver.
  __ JumpIfSmi(value, &call_builtin);
  __ LoadMap(kScratchRegister, value);
  __ cmpw(FieldOperand(kScratchRegister, Map::kInstanceTypeOffset),
          Immediate(FIRST_JS_RECEIVER_TYPE));
  __ j(greater_equal, &done);
  __ bind(&call_builtin);
  __ CallBuiltin(Builtin::kToObject);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
  __ bind(&done);
}

int ToString::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kToString>::type;
  return D::GetStackParameterCount();
}
void ToString::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kToString>::type;
  UseFixed(context(), kContextRegister);
  UseFixed(value_input(), D::GetRegisterParameter(D::kO));
  DefineAsFixed(this, kReturnRegister0);
}
void ToString::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
#ifdef DEBUG
  using D = CallInterfaceDescriptorFor<Builtin::kToString>::type;
  DCHECK_EQ(ToRegister(context()), kContextRegister);
  DCHECK_EQ(ToRegister(value_input()), D::GetRegisterParameter(D::kO));
#endif  // DEBUG
  Register value = ToRegister(value_input());
  Label call_builtin, done;
  // Avoid the builtin call if {value} is a string.
  __ JumpIfSmi(value, &call_builtin);
  __ LoadMap(kScratchRegister, value);
  __ cmpw(FieldOperand(kScratchRegister, Map::kInstanceTypeOffset),
          Immediate(FIRST_NONSTRING_TYPE));
  __ j(less, &done);
  __ bind(&call_builtin);
  __ CallBuiltin(Builtin::kToString);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
  __ bind(&done);
}

void CheckedInt32ToUint32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void CheckedInt32ToUint32::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register input_reg = ToRegister(input());
  __ testl(input_reg, input_reg);
  __ EmitEagerDeoptIf(negative, DeoptimizeReason::kNotUint32, this);
}

void CheckedUint32ToInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void CheckedUint32ToInt32::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register input_reg = ToRegister(input());
  // Check if the top bit is set -- if it is, then this is not a valid int32,
  // otherwise it is.
  __ testl(input_reg, Immediate(1 << 31));
  __ EmitEagerDeoptIf(not_zero, DeoptimizeReason::kNotInt32, this);
}

void ChangeInt32ToFloat64::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void ChangeInt32ToFloat64::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  __ Cvtlsi2sd(ToDoubleRegister(result()), ToRegister(input()));
}

void ChangeUint32ToFloat64::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void ChangeUint32ToFloat64::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  // TODO(leszeks): Cvtlui2sd does a manual movl to clear the top bits of the
  // input register. We could eliminate this movl by ensuring that word32
  // registers are always written with 32-bit ops and not 64-bit ones.
  __ Cvtlui2sd(ToDoubleRegister(result()), ToRegister(input()));
}

void CheckedTruncateFloat64ToInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void CheckedTruncateFloat64ToInt32::GenerateCode(MaglevAssembler* masm,
                                                 const ProcessingState& state) {
  DoubleRegister input_reg = ToDoubleRegister(input());
  Register result_reg = ToRegister(result());
  DoubleRegister converted_back = kScratchDoubleReg;

  // Convert the input float64 value to int32.
  __ Cvttsd2si(result_reg, input_reg);
  // Convert that int32 value back to float64.
  __ Cvtlsi2sd(converted_back, result_reg);
  // Check that the result of the float64->int32->float64 is equal to the input
  // (i.e. that the conversion didn't truncate.
  __ Ucomisd(input_reg, converted_back);
  __ EmitEagerDeoptIf(parity_even, DeoptimizeReason::kNotInt32, this);
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotInt32, this);

  // Check if {input} is -0.
  Label check_done;
  __ cmpl(result_reg, Immediate(0));
  __ j(not_equal, &check_done);

  // In case of 0, we need to check the high bits for the IEEE -0 pattern.
  Register high_word32_of_input = kScratchRegister;
  __ Pextrd(high_word32_of_input, input_reg, 1);
  __ cmpl(high_word32_of_input, Immediate(0));
  __ EmitEagerDeoptIf(less, DeoptimizeReason::kNotInt32, this);

  __ bind(&check_done);
}

void CheckedTruncateFloat64ToUint32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void CheckedTruncateFloat64ToUint32::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  DoubleRegister input_reg = ToDoubleRegister(input());
  Register result_reg = ToRegister(result());
  DoubleRegister converted_back = kScratchDoubleReg;

  // Convert the input float64 value to uint32.
  Label* deopt = __ GetDeoptLabel(this, DeoptimizeReason::kNotUint32);
  __ Cvttsd2ui(result_reg, input_reg, deopt);
  // Convert that uint32 value back to float64.
  __ Cvtlui2sd(converted_back, result_reg);
  // Check that the result of the float64->uint32->float64 is equal to the input
  // (i.e. that the conversion didn't truncate.
  __ Ucomisd(input_reg, converted_back);
  __ EmitEagerDeoptIf(parity_even, DeoptimizeReason::kNotUint32, this);
  __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kNotUint32, this);

  // Check if {input} is -0.
  Label check_done;
  __ cmpl(result_reg, Immediate(0));
  __ j(not_equal, &check_done);

  // In case of 0, we need to check the high bits for the IEEE -0 pattern.
  Register high_word32_of_input = kScratchRegister;
  __ Pextrd(high_word32_of_input, input_reg, 1);
  __ cmpl(high_word32_of_input, Immediate(0));
  __ EmitEagerDeoptIf(less, DeoptimizeReason::kNotUint32, this);

  __ bind(&check_done);
}

int ConvertReceiver::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kToObject>::type;
  return D::GetStackParameterCount();
}
void ConvertReceiver::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kToObject>::type;
  UseFixed(receiver_input(), D::GetRegisterParameter(D::kInput));
  set_temporaries_needed(1);
  DefineAsFixed(this, kReturnRegister0);
}
void ConvertReceiver::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Label convert_to_object, done;
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  Register receiver = ToRegister(receiver_input());
  __ JumpIfSmi(receiver, &convert_to_object, Label::kNear);
  static_assert(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  __ CmpObjectType(receiver, FIRST_JS_RECEIVER_TYPE, scratch);
  __ j(above_equal, &done);

  if (mode_ != ConvertReceiverMode::kNotNullOrUndefined) {
    Label convert_global_proxy;
    __ JumpIfRoot(receiver, RootIndex::kUndefinedValue, &convert_global_proxy,
                  Label::kNear);
    __ JumpIfNotRoot(receiver, RootIndex::kNullValue, &convert_to_object,
                     Label::kNear);
    __ bind(&convert_global_proxy);
    {
      // Patch receiver to global proxy.
      __ Move(ToRegister(result()),
              target_.native_context().global_proxy_object().object());
    }
    __ jmp(&done);
  }

  __ bind(&convert_to_object);
  // ToObject needs to be ran with the target context installed.
  __ Move(kContextRegister, target_.context().object());
  __ CallBuiltin(Builtin::kToObject);
  __ bind(&done);
}

void IncreaseInterruptBudget::SetValueLocationConstraints() {
  set_temporaries_needed(1);
}
void IncreaseInterruptBudget::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  __ movq(scratch, MemOperand(rbp, StandardFrameConstants::kFunctionOffset));
  __ LoadTaggedPointerField(
      scratch, FieldOperand(scratch, JSFunction::kFeedbackCellOffset));
  __ addl(FieldOperand(scratch, FeedbackCell::kInterruptBudgetOffset),
          Immediate(amount()));
}

namespace {

void HandleInterruptsAndTiering(MaglevAssembler* masm, ZoneLabelRef done,
                                ReduceInterruptBudget* node,
                                Register scratch0) {
  // First, check for interrupts.
  {
    Label next;

    // Here, we only care about interrupts since we've already guarded against
    // real stack overflows on function entry.
    __ cmpq(rsp, __ StackLimitAsOperand(StackLimitKind::kInterruptStackLimit));
    __ j(above, &next);

    // An interrupt has been requested and we must call into runtime to handle
    // it; since we already pay the call cost, combine with the TieringManager
    // call.
    {
      SaveRegisterStateForCall save_register_state(masm,
                                                   node->register_snapshot());
      __ Move(kContextRegister, masm->native_context().object());
      __ Push(MemOperand(rbp, StandardFrameConstants::kFunctionOffset));
      __ CallRuntime(Runtime::kBytecodeBudgetInterruptWithStackCheck_Maglev, 1);
      save_register_state.DefineSafepointWithLazyDeopt(node->lazy_deopt_info());
    }
    __ jmp(*done);  // All done, continue.

    __ bind(&next);
  }

  // No pending interrupts. Call into the TieringManager if needed.
  {
    // Skip the runtime call if the tiering state is kInProgress. The runtime
    // only performs simple bookkeeping in this case, which we can easily
    // replicate here in generated code.
    // TODO(jgruber): Use the correct feedback vector once Maglev inlining is
    // enabled.
    Label update_profiler_ticks_and_interrupt_budget;
    {
      static_assert(kTieringStateInProgressBlocksTierup);
      const Register scratch1 = kScratchRegister;
      __ Move(scratch0, masm->compilation_info()
                            ->toplevel_compilation_unit()
                            ->feedback()
                            .object());

      // If tiering_state is kInProgress, skip the runtime call.
      __ movzxwl(scratch1,
                 FieldOperand(scratch0, FeedbackVector::kFlagsOffset));
      __ DecodeField<FeedbackVector::TieringStateBits>(scratch1);
      __ cmpl(scratch1, Immediate(static_cast<int>(TieringState::kInProgress)));
      __ j(equal, &update_profiler_ticks_and_interrupt_budget);

      // If osr_tiering_state is kInProgress, skip the runtime call.
      __ movzxwl(scratch1,
                 FieldOperand(scratch0, FeedbackVector::kFlagsOffset));
      __ DecodeField<FeedbackVector::OsrTieringStateBit>(scratch1);
      __ cmpl(scratch1, Immediate(static_cast<int>(TieringState::kInProgress)));
      __ j(equal, &update_profiler_ticks_and_interrupt_budget);
    }

    {
      SaveRegisterStateForCall save_register_state(masm,
                                                   node->register_snapshot());
      __ Move(kContextRegister, masm->native_context().object());
      __ Push(MemOperand(rbp, StandardFrameConstants::kFunctionOffset));
      // Note: must not cause a lazy deopt!
      __ CallRuntime(Runtime::kBytecodeBudgetInterrupt_Maglev, 1);
      save_register_state.DefineSafepoint();
    }
    __ jmp(*done);

    __ bind(&update_profiler_ticks_and_interrupt_budget);
    // We are skipping the call to Runtime::kBytecodeBudgetInterrupt_Maglev
    // since the tiering state is kInProgress. Perform bookkeeping that would
    // have been done in the runtime function:
    __ AssertFeedbackVector(scratch0);
    // FeedbackVector::SaturatingIncrementProfilerTicks.
    // TODO(jgruber): This isn't saturating and thus we may theoretically
    // exceed Smi::kMaxValue. But, 1) this is very unlikely since it'd take
    // quite some time to exhaust the budget that many times; and 2) even an
    // overflow doesn't hurt us at all.
    __ incl(FieldOperand(scratch0, FeedbackVector::kProfilerTicksOffset));
    // JSFunction::SetInterruptBudget.
    __ movq(scratch0, MemOperand(rbp, StandardFrameConstants::kFunctionOffset));
    __ LoadTaggedPointerField(
        scratch0, FieldOperand(scratch0, JSFunction::kFeedbackCellOffset));
    __ movl(FieldOperand(scratch0, FeedbackCell::kInterruptBudgetOffset),
            Immediate(v8_flags.interrupt_budget));
    __ jmp(*done);
  }
}

}  // namespace

int ReduceInterruptBudget::MaxCallStackArgs() const { return 1; }
void ReduceInterruptBudget::SetValueLocationConstraints() {
  set_temporaries_needed(1);
}
void ReduceInterruptBudget::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  __ movq(scratch, MemOperand(rbp, StandardFrameConstants::kFunctionOffset));
  __ LoadTaggedPointerField(
      scratch, FieldOperand(scratch, JSFunction::kFeedbackCellOffset));
  __ subl(FieldOperand(scratch, FeedbackCell::kInterruptBudgetOffset),
          Immediate(amount()));
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(less, HandleInterruptsAndTiering, done, this, scratch);
  __ bind(*done);
}

int ThrowIfNotSuperConstructor::MaxCallStackArgs() const { return 2; }
void ThrowIfNotSuperConstructor::SetValueLocationConstraints() {
  UseRegister(constructor());
  UseRegister(function());
}
void ThrowIfNotSuperConstructor::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  __ LoadMap(kScratchRegister, ToRegister(constructor()));
  __ testl(FieldOperand(kScratchRegister, Map::kBitFieldOffset),
           Immediate(Map::Bits1::IsConstructorBit::kMask));
  __ JumpToDeferredIf(
      equal,
      [](MaglevAssembler* masm, ThrowIfNotSuperConstructor* node) {
        __ Push(ToRegister(node->constructor()));
        __ Push(ToRegister(node->function()));
        __ Move(kContextRegister, masm->native_context().object());
        __ CallRuntime(Runtime::kThrowNotSuperConstructor, 2);
        masm->DefineExceptionHandlerAndLazyDeoptPoint(node);
        __ Abort(AbortReason::kUnexpectedReturnFromThrow);
      },
      this);
}

// ---
// Control nodes
// ---
void Return::SetValueLocationConstraints() {
  UseFixed(value_input(), kReturnRegister0);
}
void Return::GenerateCode(MaglevAssembler* masm, const ProcessingState& state) {
  DCHECK_EQ(ToRegister(value_input()), kReturnRegister0);

  // Read the formal number of parameters from the top level compilation unit
  // (i.e. the outermost, non inlined function).
  int formal_params_size =
      masm->compilation_info()->toplevel_compilation_unit()->parameter_count();

  // We're not going to continue execution, so we can use an arbitrary register
  // here instead of relying on temporaries from the register allocator.
  Register actual_params_size = r8;

  // Compute the size of the actual parameters + receiver (in bytes).
  // TODO(leszeks): Consider making this an input into Return to re-use the
  // incoming argc's register (if it's still valid).
  __ movq(actual_params_size,
          MemOperand(rbp, StandardFrameConstants::kArgCOffset));

  // Leave the frame.
  __ LeaveFrame(StackFrame::MAGLEV);

  // If actual is bigger than formal, then we should use it to free up the stack
  // arguments.
  Label drop_dynamic_arg_size;
  __ cmpq(actual_params_size, Immediate(formal_params_size));
  __ j(greater, &drop_dynamic_arg_size);

  // Drop receiver + arguments according to static formal arguments size.
  __ Ret(formal_params_size * kSystemPointerSize, kScratchRegister);

  __ bind(&drop_dynamic_arg_size);
  // Drop receiver + arguments according to dynamic arguments size.
  __ DropArguments(actual_params_size, r9, TurboAssembler::kCountIsInteger,
                   TurboAssembler::kCountIncludesReceiver);
  __ Ret();
}

void BranchIfJSReceiver::SetValueLocationConstraints() {
  UseRegister(condition_input());
}
void BranchIfJSReceiver::GenerateCode(MaglevAssembler* masm,
                                      const ProcessingState& state) {
  Register value = ToRegister(condition_input());
  __ JumpIfSmi(value, if_false()->label());
  __ LoadMap(kScratchRegister, value);
  __ CmpInstanceType(kScratchRegister, FIRST_JS_RECEIVER_TYPE);
  __ Branch(above_equal, if_true(), if_false(), state.next_block());
}

void BranchIfFloat64Compare::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
}
void BranchIfFloat64Compare::GenerateCode(MaglevAssembler* masm,
                                          const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  __ Ucomisd(left, right);
  __ j(parity_even, if_false()->label());
  __ Branch(ConditionForFloat64(operation_), if_true(), if_false(),
            state.next_block());
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
