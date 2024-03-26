// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/logging.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/x64/assembler-x64-inl.h"
#include "src/codegen/x64/assembler-x64.h"
#include "src/codegen/x64/register-x64.h"
#include "src/maglev/maglev-assembler-inl.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "src/objects/feedback-cell.h"
#include "src/objects/instance-type.h"
#include "src/objects/js-function.h"

namespace v8 {
namespace internal {
namespace maglev {

#define __ masm->

// ---
// Nodes
// ---

void FoldedAllocation::SetValueLocationConstraints() {
  UseRegister(raw_allocation());
  DefineAsRegister(this);
}

void FoldedAllocation::GenerateCode(MaglevAssembler* masm,
                                    const ProcessingState& state) {
  __ leaq(ToRegister(result()),
          Operand(ToRegister(raw_allocation()), offset()));
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

  // Normal DataView (backed by AB / SAB) or non-length tracking backed by GSAB.
  __ LoadBoundedSizeFromObject(byte_length, object,
                               JSDataView::kRawByteLengthOffset);

  int element_size = ExternalArrayElementSize(element_type_);
  if (element_size > 1) {
    __ subq(byte_length, Immediate(element_size - 1));
    __ EmitEagerDeoptIf(negative, DeoptimizeReason::kOutOfBounds, this);
  }
  __ cmpl(index, byte_length);
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

  if (check_type() == CheckType::kOmitHeapObjectCheck) {
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
        __ cmpw(map_tmp, Immediate(THIN_STRING_TYPE));
        // Deopt if this isn't a thin string.
        __ EmitEagerDeoptIf(not_equal, DeoptimizeReason::kWrongMap, node);
        __ LoadTaggedField(object,
                           FieldOperand(object, ThinString::kActualOffset));
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

int BuiltinStringPrototypeCharCodeOrCodePointAt::MaxCallStackArgs() const {
  DCHECK_EQ(Runtime::FunctionForId(Runtime::kStringCharCodeAt)->nargs, 2);
  return 2;
}
void BuiltinStringPrototypeCharCodeOrCodePointAt::
    SetValueLocationConstraints() {
  UseAndClobberRegister(string_input());
  UseAndClobberRegister(index_input());
  DefineAsRegister(this);
  set_temporaries_needed(1);
}

void BuiltinStringPrototypeCharCodeOrCodePointAt::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  Register string = ToRegister(string_input());
  Register index = ToRegister(index_input());
  ZoneLabelRef done(masm);
  RegisterSnapshot save_registers = register_snapshot();
  __ StringCharCodeOrCodePointAt(mode_, save_registers, ToRegister(result()),
                                 string, index, scratch, *done);
  __ bind(*done);
}

void StoreFixedDoubleArrayElement::SetValueLocationConstraints() {
  UseRegister(elements_input());
  UseRegister(index_input());
  UseRegister(value_input());
}
void StoreFixedDoubleArrayElement::GenerateCode(MaglevAssembler* masm,
                                                const ProcessingState& state) {
  Register elements = ToRegister(elements_input());
  Register index = ToRegister(index_input());
  DoubleRegister value = ToDoubleRegister(value_input());
  if (v8_flags.debug_code) {
    __ AssertNotSmi(elements);
    __ CmpObjectType(elements, FIXED_DOUBLE_ARRAY_TYPE, kScratchRegister);
    __ Assert(equal, AbortReason::kUnexpectedValue);
    __ cmpq(index, Immediate(0));
    __ Assert(above_equal, AbortReason::kUnexpectedNegativeValue);
  }
  __ Movsd(
      FieldOperand(elements, index, times_8, FixedDoubleArray::kHeaderSize),
      value);
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
      __ ToBoolean(ToRegister(is_little_endian_input()),
                   CheckType::kCheckHeapObject, is_little_endian, is_big_endian,
                   false);
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
      __ ToBoolean(ToRegister(is_little_endian_input()),
                   CheckType::kCheckHeapObject, is_little_endian, is_big_endian,
                   false);
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
    __ ToBoolean(ToRegister(is_little_endian_input()),
                 CheckType::kCheckHeapObject, is_little_endian, is_big_endian,
                 true);
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
    __ ToBoolean(ToRegister(is_little_endian_input()),
                 CheckType::kCheckHeapObject, is_little_endian, is_big_endian,
                 true);
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

template <bool check_detached, typename ValueReg, typename NodeT>
void GenerateTypedArrayStore(MaglevAssembler* masm, NodeT* node,
                             Register object, Register index, ValueReg value,
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

  if constexpr (std::is_same_v<ValueReg, Register>) {
    int element_size = ElementsKindSize(kind);
    __ StoreField(
        Operand(data_pointer, index, ScaleFactorFromInt(element_size), 0),
        value, element_size);
  } else {
#ifdef DEBUG
    bool value_is_double = std::is_same_v<ValueReg, DoubleRegister>;
    DCHECK(value_is_double);
    DCHECK(IsFloatTypedArrayElementsKind(kind));
#endif
    switch (kind) {
      case FLOAT32_ELEMENTS:
        __ Cvtsd2ss(kScratchDoubleReg, value);
        __ Movss(Operand(data_pointer, index, times_4, 0), kScratchDoubleReg);
        break;
      case FLOAT64_ELEMENTS:
        __ Movsd(Operand(data_pointer, index, times_8, 0), value);
        break;
      default:
        UNREACHABLE();
    }
  }
}

}  // namespace

#define DEF_LOAD_TYPED_ARRAY(Name, ResultReg, ToResultReg, check_detached) \
  void Name::SetValueLocationConstraints() {                               \
    UseRegister(object_input());                                           \
    UseRegister(index_input());                                            \
    DefineAsRegister(this);                                                \
    set_temporaries_needed(1);                                             \
  }                                                                        \
  void Name::GenerateCode(MaglevAssembler* masm,                           \
                          const ProcessingState& state) {                  \
    Register object = ToRegister(object_input());                          \
    Register index = ToRegister(index_input());                            \
    ResultReg result_reg = ToResultReg(result());                          \
    MaglevAssembler::ScratchRegisterScope temps(masm);                     \
    Register scratch = temps.Acquire();                                    \
                                                                           \
    GenerateTypedArrayLoad<check_detached>(                                \
        masm, this, object, index, result_reg, scratch, elements_kind_);   \
  }

DEF_LOAD_TYPED_ARRAY(LoadSignedIntTypedArrayElement, Register, ToRegister,
                     /*check_detached*/ true)
DEF_LOAD_TYPED_ARRAY(LoadSignedIntTypedArrayElementNoDeopt, Register,
                     ToRegister,
                     /*check_detached*/ false)

DEF_LOAD_TYPED_ARRAY(LoadUnsignedIntTypedArrayElement, Register, ToRegister,
                     /*check_detached*/ true)
DEF_LOAD_TYPED_ARRAY(LoadUnsignedIntTypedArrayElementNoDeopt, Register,
                     ToRegister,
                     /*check_detached*/ false)

DEF_LOAD_TYPED_ARRAY(LoadDoubleTypedArrayElement, DoubleRegister,
                     ToDoubleRegister,
                     /*check_detached*/ true)
DEF_LOAD_TYPED_ARRAY(LoadDoubleTypedArrayElementNoDeopt, DoubleRegister,
                     ToDoubleRegister, /*check_detached*/ false)

#undef DEF_LOAD_TYPED_ARRAY

#define DEF_STORE_TYPED_ARRAY(Name, ValueReg, ToValueReg, check_detached)     \
  void Name::SetValueLocationConstraints() {                                  \
    UseRegister(object_input());                                              \
    UseRegister(index_input());                                               \
    UseRegister(value_input());                                               \
    set_temporaries_needed(1);                                                \
  }                                                                           \
  void Name::GenerateCode(MaglevAssembler* masm,                              \
                          const ProcessingState& state) {                     \
    Register object = ToRegister(object_input());                             \
    Register index = ToRegister(index_input());                               \
    ValueReg value = ToValueReg(value_input());                               \
    MaglevAssembler::ScratchRegisterScope temps(masm);                        \
    Register scratch = temps.Acquire();                                       \
                                                                              \
    GenerateTypedArrayStore<check_detached>(masm, this, object, index, value, \
                                            scratch, elements_kind_);         \
  }

DEF_STORE_TYPED_ARRAY(StoreIntTypedArrayElement, Register, ToRegister,
                      /*check_detached*/ true)
DEF_STORE_TYPED_ARRAY(StoreIntTypedArrayElementNoDeopt, Register, ToRegister,
                      /*check_detached*/ false)

DEF_STORE_TYPED_ARRAY(StoreDoubleTypedArrayElement, DoubleRegister,
                      ToDoubleRegister,
                      /*check_detached*/ true)
DEF_STORE_TYPED_ARRAY(StoreDoubleTypedArrayElementNoDeopt, DoubleRegister,
                      ToDoubleRegister, /*check_detached*/ false)

#undef DEF_STORE_TYPED_ARRAY

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
  __ DecompressTagged(tmp, FieldOperand(object, offset()));
  __ AssertNotSmi(tmp);
  __ Movsd(FieldOperand(tmp, HeapNumber::kValueOffset), value);
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

void Float64Round::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  DoubleRegister in = ToDoubleRegister(input());
  DoubleRegister out = ToDoubleRegister(result());

  if (kind_ == Kind::kNearest) {
    MaglevAssembler::ScratchRegisterScope temps(masm);
    DoubleRegister temp = temps.AcquireDouble();
    __ Move(temp, in);
    __ Roundsd(out, in, kRoundToNearest);
    // RoundToNearest rounds to even on tie, while JS expects it to round
    // towards +Infinity. Fix the difference by checking if we rounded down by
    // exactly 0.5, and if so, round to the other side.
    __ Subsd(temp, out);
    __ Move(kScratchDoubleReg, 0.5);
    Label done;
    __ Ucomisd(temp, kScratchDoubleReg);
    __ JumpIf(not_equal, &done, Label::kNear);
    // Fix wrong tie-to-even by adding 0.5 twice.
    __ Addsd(out, kScratchDoubleReg);
    __ Addsd(out, kScratchDoubleReg);
    __ bind(&done);
  } else if (kind_ == Kind::kFloor) {
    __ Roundsd(out, in, kRoundDown);
  } else if (kind_ == Kind::kCeil) {
    __ Roundsd(out, in, kRoundUp);
  }
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

void HoleyFloat64ToMaybeNanFloat64::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void HoleyFloat64ToMaybeNanFloat64::GenerateCode(MaglevAssembler* masm,
                                                 const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(input());
  // The hole value is a signalling NaN, so just silence it to get the float64
  // value.
  __ Xorpd(kScratchDoubleReg, kScratchDoubleReg);
  __ Subsd(value, kScratchDoubleReg);
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

namespace {

enum class ReduceInterruptBudgetType { kLoop, kReturn };

void HandleInterruptsAndTiering(MaglevAssembler* masm, ZoneLabelRef done,
                                Node* node, ReduceInterruptBudgetType type,
                                Register scratch0) {
  // For loops, first check for interrupts. Don't do this for returns, as we
  // can't lazy deopt to the end of a return.
  if (type == ReduceInterruptBudgetType::kLoop) {
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
    SaveRegisterStateForCall save_register_state(masm,
                                                 node->register_snapshot());
    __ Move(kContextRegister, masm->native_context().object());
    __ Push(MemOperand(rbp, StandardFrameConstants::kFunctionOffset));
    // Note: must not cause a lazy deopt!
    __ CallRuntime(Runtime::kBytecodeBudgetInterrupt_Maglev, 1);
    save_register_state.DefineSafepoint();
  }
  __ jmp(*done);
}

void GenerateReduceInterruptBudget(MaglevAssembler* masm, Node* node,
                                   ReduceInterruptBudgetType type, int amount) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  __ movq(scratch, MemOperand(rbp, StandardFrameConstants::kFunctionOffset));
  __ LoadTaggedField(scratch,
                     FieldOperand(scratch, JSFunction::kFeedbackCellOffset));
  __ subl(FieldOperand(scratch, FeedbackCell::kInterruptBudgetOffset),
          Immediate(amount));
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(less, HandleInterruptsAndTiering, done, node, type,
                      scratch);
  __ bind(*done);
}

}  // namespace

int ReduceInterruptBudgetForLoop::MaxCallStackArgs() const { return 1; }
void ReduceInterruptBudgetForLoop::SetValueLocationConstraints() {
  set_temporaries_needed(1);
}
void ReduceInterruptBudgetForLoop::GenerateCode(MaglevAssembler* masm,
                                                const ProcessingState& state) {
  GenerateReduceInterruptBudget(masm, this, ReduceInterruptBudgetType::kLoop,
                                amount());
}

int ReduceInterruptBudgetForReturn::MaxCallStackArgs() const { return 1; }
void ReduceInterruptBudgetForReturn::SetValueLocationConstraints() {
  set_temporaries_needed(1);
}
void ReduceInterruptBudgetForReturn::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  GenerateReduceInterruptBudget(masm, this, ReduceInterruptBudgetType::kReturn,
                                amount());
}

int FunctionEntryStackCheck::MaxCallStackArgs() const { return 1; }
void FunctionEntryStackCheck::SetValueLocationConstraints() {}
void FunctionEntryStackCheck::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  if (!masm->code_gen_state()->needs_stack_check()) return;
  // Stack check. This folds the checks for both the interrupt stack limit
  // check and the real stack limit into one by just checking for the
  // interrupt limit. The interrupt limit is either equal to the real
  // stack limit or tighter. By ensuring we have space until that limit
  // after building the frame we can quickly precheck both at once.
  const int stack_check_offset = masm->code_gen_state()->stack_check_offset();
  Register stack_cmp_reg = rsp;
  if (stack_check_offset > kStackLimitSlackForDeoptimizationInBytes) {
    stack_cmp_reg = kScratchRegister;
    __ leaq(stack_cmp_reg, Operand(rsp, -stack_check_offset));
  }
  __ cmpq(stack_cmp_reg,
          __ StackLimitAsOperand(StackLimitKind::kInterruptStackLimit));

  ZoneLabelRef deferred_call_stack_guard_return(masm);
  __ JumpToDeferredIf(
      below_equal,
      [](MaglevAssembler* masm, FunctionEntryStackCheck* node,
         ZoneLabelRef done, int stack_check_offset) {
        ASM_CODE_COMMENT_STRING(masm, "Stack/interrupt call");
        {
          SaveRegisterStateForCall save_register_state(
              masm, node->register_snapshot());
          // Push the frame size
          __ Push(Immediate(Smi::FromInt(stack_check_offset)));
          __ CallRuntime(Runtime::kStackGuardWithGap, 1);
          save_register_state.DefineSafepointWithLazyDeopt(
              node->lazy_deopt_info());
        }
        __ jmp(*done);
      },
      this, deferred_call_stack_guard_return, stack_check_offset);
  __ bind(*deferred_call_stack_guard_return);
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
  __ DropArguments(actual_params_size, r9, MacroAssembler::kCountIsInteger,
                   MacroAssembler::kCountIncludesReceiver);
  __ Ret();
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
