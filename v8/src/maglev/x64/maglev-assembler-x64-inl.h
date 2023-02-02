// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_X64_MAGLEV_ASSEMBLER_X64_INL_H_
#define V8_MAGLEV_X64_MAGLEV_ASSEMBLER_X64_INL_H_

#include <tuple>
#include <type_traits>
#include <utility>

#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/maglev/maglev-assembler.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-code-gen-state.h"

namespace v8 {
namespace internal {
namespace maglev {

constexpr Condition ConditionFor(Operation operation) {
  switch (operation) {
    case Operation::kEqual:
    case Operation::kStrictEqual:
      return equal;
    case Operation::kLessThan:
      return less;
    case Operation::kLessThanOrEqual:
      return less_equal;
    case Operation::kGreaterThan:
      return greater;
    case Operation::kGreaterThanOrEqual:
      return greater_equal;
    default:
      UNREACHABLE();
  }
}

class MaglevAssembler::ScratchRegisterScope {
 public:
  explicit ScratchRegisterScope(MaglevAssembler* masm)
      : masm_(masm),
        prev_scope_(masm->scratch_register_scope_),
        available_(masm->scratch_register_scope_
                       ? masm_->scratch_register_scope_->available_
                       : RegList()),
        available_double_(
            masm->scratch_register_scope_
                ? masm_->scratch_register_scope_->available_double_
                : DoubleRegList()) {
    masm_->scratch_register_scope_ = this;
  }
  ~ScratchRegisterScope() { masm_->scratch_register_scope_ = prev_scope_; }

  Register Acquire() { return available_.PopFirst(); }
  void Include(Register reg) { available_.set(reg); }
  void Include(const RegList list) { available_ = available_ | list; }

  DoubleRegister AcquireDouble() { return available_double_.PopFirst(); }
  void IncludeDouble(const DoubleRegList list) {
    available_double_ = available_double_ | list;
  }

  RegList Available() { return available_; }
  void SetAvailable(RegList list) { available_ = list; }

  DoubleRegList AvailableDouble() { return available_double_; }
  void SetAvailableDouble(DoubleRegList list) { available_double_ = list; }

 private:
  MaglevAssembler* masm_;
  ScratchRegisterScope* prev_scope_;
  RegList available_;
  DoubleRegList available_double_;
};

namespace detail {

template <typename... Args>
struct PushAllHelper;

template <>
struct PushAllHelper<> {
  static void Push(MaglevAssembler* masm) {}
  static void PushReverse(MaglevAssembler* masm) {}
};

inline void PushInput(MaglevAssembler* masm, const Input& input) {
  if (input.operand().IsConstant()) {
    input.node()->LoadToRegister(masm, kScratchRegister);
    masm->Push(kScratchRegister);
  } else {
    // TODO(leszeks): Consider special casing the value. (Toon: could possibly
    // be done through Input directly?)
    const compiler::AllocatedOperand& operand =
        compiler::AllocatedOperand::cast(input.operand());

    if (operand.IsRegister()) {
      masm->Push(operand.GetRegister());
    } else {
      DCHECK(operand.IsStackSlot());
      masm->Push(masm->GetStackSlot(operand));
    }
  }
}

template <typename T, typename... Args>
inline void PushIterator(MaglevAssembler* masm, base::iterator_range<T> range,
                         Args... args) {
  for (auto iter = range.begin(), end = range.end(); iter != end; ++iter) {
    masm->Push(*iter);
  }
  PushAllHelper<Args...>::Push(masm, args...);
}

template <typename T, typename... Args>
inline void PushIteratorReverse(MaglevAssembler* masm,
                                base::iterator_range<T> range, Args... args) {
  PushAllHelper<Args...>::PushReverse(masm, args...);
  for (auto iter = range.rbegin(), end = range.rend(); iter != end; ++iter) {
    masm->Push(*iter);
  }
}

template <typename... Args>
struct PushAllHelper<Input, Args...> {
  static void Push(MaglevAssembler* masm, const Input& arg, Args... args) {
    PushInput(masm, arg);
    PushAllHelper<Args...>::Push(masm, args...);
  }
  static void PushReverse(MaglevAssembler* masm, const Input& arg,
                          Args... args) {
    PushAllHelper<Args...>::PushReverse(masm, args...);
    PushInput(masm, arg);
  }
};
template <typename Arg, typename... Args>
struct PushAllHelper<Arg, Args...> {
  static void Push(MaglevAssembler* masm, Arg arg, Args... args) {
    if constexpr (is_iterator_range<Arg>::value) {
      PushIterator(masm, arg, args...);
    } else {
      masm->MacroAssembler::Push(arg);
      PushAllHelper<Args...>::Push(masm, args...);
    }
  }
  static void PushReverse(MaglevAssembler* masm, Arg arg, Args... args) {
    if constexpr (is_iterator_range<Arg>::value) {
      PushIteratorReverse(masm, arg, args...);
    } else {
      PushAllHelper<Args...>::PushReverse(masm, args...);
      masm->Push(arg);
    }
  }
};

}  // namespace detail

template <typename... T>
void MaglevAssembler::Push(T... vals) {
  detail::PushAllHelper<T...>::Push(this, vals...);
}

template <typename... T>
void MaglevAssembler::PushReverse(T... vals) {
  detail::PushAllHelper<T...>::PushReverse(this, vals...);
}

inline void MaglevAssembler::BindJumpTarget(Label* label) { bind(label); }

inline void MaglevAssembler::BindBlock(BasicBlock* block) {
  bind(block->label());
}

inline void MaglevAssembler::DoubleToInt64Repr(Register dst,
                                               DoubleRegister src) {
  Movq(dst, src);
}

inline Condition MaglevAssembler::IsInt64Constant(Register reg,
                                                  int64_t constant) {
  movq(kScratchRegister, kHoleNanInt64);
  cmpq(reg, kScratchRegister);
  return equal;
}

inline Condition MaglevAssembler::IsRootConstant(Input input,
                                                 RootIndex root_index) {
  if (input.operand().IsRegister()) {
    CompareRoot(ToRegister(input), root_index);
  } else {
    DCHECK(input.operand().IsStackSlot());
    CompareRoot(ToMemOperand(input), root_index);
  }
  return equal;
}

inline MemOperand MaglevAssembler::GetStackSlot(
    const compiler::AllocatedOperand& operand) {
  return MemOperand(rbp, GetFramePointerOffsetForStackSlot(operand));
}

inline MemOperand MaglevAssembler::ToMemOperand(
    const compiler::InstructionOperand& operand) {
  return GetStackSlot(compiler::AllocatedOperand::cast(operand));
}

inline MemOperand MaglevAssembler::ToMemOperand(const ValueLocation& location) {
  return ToMemOperand(location.operand());
}

inline void MaglevAssembler::BuildTypedArrayDataPointer(Register data_pointer,
                                                        Register object) {
  DCHECK_NE(data_pointer, object);
  LoadExternalPointerField(
      data_pointer, FieldOperand(object, JSTypedArray::kExternalPointerOffset));
  if (JSTypedArray::kMaxSizeInHeap == 0) return;

  Register base = kScratchRegister;
  movl(base, FieldOperand(object, JSTypedArray::kBasePointerOffset));
  addq(data_pointer, base);
}

inline void MaglevAssembler::LoadBoundedSizeFromObject(Register result,
                                                       Register object,
                                                       int offset) {
  movq(result, FieldOperand(object, offset));
#ifdef V8_ENABLE_SANDBOX
  shrq(result, Immediate(kBoundedSizeShift));
#endif  // V8_ENABLE_SANDBOX
}

inline void MaglevAssembler::LoadExternalPointerField(Register result,
                                                      Operand operand) {
#ifdef V8_ENABLE_SANDBOX
  LoadSandboxedPointerField(result, operand);
#else
  movq(result, operand);
#endif
}

inline void MaglevAssembler::LoadSignedField(Register result, Operand operand,
                                             int size) {
  if (size == 1) {
    movsxbl(result, operand);
  } else if (size == 2) {
    movsxwl(result, operand);
  } else {
    DCHECK_EQ(size, 4);
    movl(result, operand);
  }
}

inline void MaglevAssembler::LoadUnsignedField(Register result, Operand operand,
                                               int size) {
  if (size == 1) {
    movzxbl(result, operand);
  } else if (size == 2) {
    movzxwl(result, operand);
  } else {
    DCHECK_EQ(size, 4);
    movl(result, operand);
  }
}

inline void MaglevAssembler::StoreField(Operand operand, Register value,
                                        int size) {
  DCHECK(size == 1 || size == 2 || size == 4);
  if (size == 1) {
    movb(operand, value);
  } else if (size == 2) {
    movw(operand, value);
  } else {
    DCHECK_EQ(size, 4);
    movl(operand, value);
  }
}

inline void MaglevAssembler::ReverseByteOrder(Register value, int size) {
  if (size == 2) {
    bswapl(value);
    sarl(value, Immediate(16));
  } else if (size == 4) {
    bswapl(value);
  } else {
    DCHECK_EQ(size, 1);
  }
}

inline MemOperand MaglevAssembler::StackSlotOperand(StackSlot stack_slot) {
  return MemOperand(rbp, stack_slot.index);
}

inline void MaglevAssembler::Move(StackSlot dst, Register src) {
  movq(StackSlotOperand(dst), src);
}

inline void MaglevAssembler::Move(StackSlot dst, DoubleRegister src) {
  Movsd(StackSlotOperand(dst), src);
}

inline void MaglevAssembler::Move(Register dst, StackSlot src) {
  movq(dst, StackSlotOperand(src));
}

inline void MaglevAssembler::Move(DoubleRegister dst, StackSlot src) {
  Movsd(dst, StackSlotOperand(src));
}

inline void MaglevAssembler::Move(MemOperand dst, Register src) {
  movq(dst, src);
}

inline void MaglevAssembler::Move(MemOperand dst, DoubleRegister src) {
  Movsd(dst, src);
}

inline void MaglevAssembler::Move(Register dst, TaggedIndex i) {
  MacroAssembler::Move(dst, i);
}

inline void MaglevAssembler::Move(DoubleRegister dst, DoubleRegister src) {
  MacroAssembler::Move(dst, src);
}

inline void MaglevAssembler::Move(Register dst, Smi src) {
  MacroAssembler::Move(dst, src);
}

inline void MaglevAssembler::Move(Register dst, ExternalReference src) {
  MacroAssembler::Move(dst, src);
}

inline void MaglevAssembler::Move(Register dst, MemOperand src) {
  MacroAssembler::Move(dst, src);
}

inline void MaglevAssembler::Move(DoubleRegister dst, MemOperand src) {
  Movsd(dst, src);
}

inline void MaglevAssembler::Move(Register dst, Register src) {
  MacroAssembler::Move(dst, src);
}

inline void MaglevAssembler::Move(Register dst, int32_t i) {
  // Move as a uint32 to avoid sign extension.
  MacroAssembler::Move(dst, static_cast<uint32_t>(i));
}

inline void MaglevAssembler::Move(DoubleRegister dst, double n) {
  MacroAssembler::Move(dst, n);
}

inline void MaglevAssembler::Move(Register dst, Handle<HeapObject> obj) {
  MacroAssembler::Move(dst, obj);
}

inline void MaglevAssembler::SignExtend32To64Bits(Register dst, Register src) {
  movsxlq(dst, src);
}

template <typename NodeT>
inline void MaglevAssembler::DeoptIfBufferDetached(Register array,
                                                   Register scratch,
                                                   NodeT* node) {
  if (!code_gen_state()
           ->broker()
           ->dependencies()
           ->DependOnArrayBufferDetachingProtector()) {
    // A detached buffer leads to megamorphic feedback, so we won't have a deopt
    // loop if we deopt here.
    LoadTaggedPointerField(
        scratch, FieldOperand(array, JSArrayBufferView::kBufferOffset));
    LoadTaggedPointerField(
        scratch, FieldOperand(scratch, JSArrayBuffer::kBitFieldOffset));
    testl(scratch, Immediate(JSArrayBuffer::WasDetachedBit::kMask));
    EmitEagerDeoptIf(not_zero, DeoptimizeReason::kArrayBufferWasDetached, node);
  }
}

inline void MaglevAssembler::LoadByte(Register dst, MemOperand src) {
  movzxbl(dst, src);
}

inline void MaglevAssembler::CompareTagged(Register reg,
                                           Handle<HeapObject> obj) {
  Cmp(reg, obj);
}

inline void MaglevAssembler::CompareInt32(Register reg, int32_t imm) {
  cmpl(reg, Immediate(imm));
}

inline void MaglevAssembler::CompareInt32(Register src1, Register src2) {
  cmpl(src1, src2);
}

inline void MaglevAssembler::Jump(Label* target, Label::Distance distance) {
  jmp(target, distance);
}

inline void MaglevAssembler::JumpIf(Condition cond, Label* target,
                                    Label::Distance distance) {
  j(cond, target, distance);
}

inline void MaglevAssembler::JumpIfEqual(Label* target,
                                         Label::Distance distance) {
  j(equal, target, distance);
}

inline void MaglevAssembler::JumpIfNotEqual(Label* target,
                                            Label::Distance distance) {
  j(not_equal, target, distance);
}

inline void MaglevAssembler::JumpIfTaggedEqual(Register r1, Register r2,
                                               Label* target,
                                               Label::Distance distance) {
  cmp_tagged(r1, r2);
  j(equal, target, distance);
}

inline void MaglevAssembler::Pop(Register dst) { MacroAssembler::Pop(dst); }

template <typename NodeT>
inline void MaglevAssembler::EmitEagerDeoptIfNotEqual(DeoptimizeReason reason,
                                                      NodeT* node) {
  EmitEagerDeoptIf(not_equal, reason, node);
}

inline void MaglevAssembler::MaterialiseValueNode(Register dst,
                                                  ValueNode* value) {
  switch (value->opcode()) {
    case Opcode::kInt32Constant: {
      int32_t int_value = value->Cast<Int32Constant>()->value();
      if (Smi::IsValid(int_value)) {
        Move(dst, Smi::FromInt(int_value));
      } else {
        movq_heap_number(dst, int_value);
      }
      return;
    }
    case Opcode::kFloat64Constant: {
      double double_value = value->Cast<Float64Constant>()->value();
      movq_heap_number(dst, double_value);
      return;
    }
    default:
      break;
  }

  DCHECK(!value->allocation().IsConstant());
  DCHECK(value->allocation().IsAnyStackSlot());
  using D = NewHeapNumberDescriptor;
  MemOperand src = ToMemOperand(value->allocation());
  switch (value->properties().value_representation()) {
    case ValueRepresentation::kInt32: {
      Label done;
      movl(dst, src);
      addl(dst, dst);
      j(no_overflow, &done, Label::kNear);
      // If we overflow, instead of bailing out (deopting), we change
      // representation to a HeapNumber.
      Cvtlsi2sd(D::GetDoubleRegisterParameter(D::kValue), src);
      CallBuiltin(Builtin::kNewHeapNumber);
      Move(dst, kReturnRegister0);
      bind(&done);
      break;
    }
    case ValueRepresentation::kUint32: {
      Label done, tag_smi;
      movl(dst, src);
      // Unsigned comparison against Smi::kMaxValue.
      cmpl(dst, Immediate(Smi::kMaxValue));
      // If we don't fit in a Smi, instead of bailing out (deopting), we
      // change representation to a HeapNumber.
      j(below_equal, &tag_smi, Label::kNear);
      // The value was loaded with movl, so is zero extended in 64-bit.
      // Therefore, we can do an unsigned 32-bit converstion to double with a
      // 64-bit signed conversion (Cvt_q_si2sd instead of Cvt_l_si2sd).
      Cvtqsi2sd(D::GetDoubleRegisterParameter(D::kValue), dst);
      CallBuiltin(Builtin::kNewHeapNumber);
      Move(dst, kReturnRegister0);
      jmp(&done, Label::kNear);
      bind(&tag_smi);
      SmiTag(dst);
      bind(&done);
      break;
    }
    case ValueRepresentation::kFloat64:
      Movsd(D::GetDoubleRegisterParameter(D::kValue), src);
      CallBuiltin(Builtin::kNewHeapNumber);
      Move(dst, kReturnRegister0);
      break;
    case ValueRepresentation::kWord64:
    case ValueRepresentation::kTagged:
      UNREACHABLE();
  }
}

inline void MaglevAssembler::AssertStackSizeCorrect() {
  if (v8_flags.debug_code) {
    movq(kScratchRegister, rbp);
    subq(kScratchRegister, rsp);
    cmpq(kScratchRegister,
         Immediate(code_gen_state()->stack_slots() * kSystemPointerSize +
                   StandardFrameConstants::kFixedFrameSizeFromFp));
    Assert(equal, AbortReason::kStackAccessBelowStackPointer);
  }
}

inline void MaglevAssembler::FinishCode() {}

template <typename Dest, typename Source>
inline void MaglevAssembler::MoveRepr(MachineRepresentation repr, Dest dst,
                                      Source src) {
  switch (repr) {
    case MachineRepresentation::kWord32:
      return movl(dst, src);
    case MachineRepresentation::kTagged:
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTaggedSigned:
      return movq(dst, src);
    default:
      UNREACHABLE();
  }
}
template <>
inline void MaglevAssembler::MoveRepr(MachineRepresentation repr,
                                      MemOperand dst, MemOperand src) {
  MoveRepr(repr, kScratchRegister, src);
  MoveRepr(repr, dst, kScratchRegister);
}

inline Condition ToCondition(AssertCondition cond) {
  switch (cond) {
    case AssertCondition::kLess:
      return less;
    case AssertCondition::kLessOrEqual:
      return less_equal;
    case AssertCondition::kGreater:
      return greater;
    case AssertCondition::kGeaterOrEqual:
      return greater_equal;
    case AssertCondition::kBelow:
      return below;
    case AssertCondition::kBelowOrEqual:
      return below_equal;
    case AssertCondition::kAbove:
      return above;
    case AssertCondition::kAboveOrEqual:
      return above_equal;
    case AssertCondition::kEqual:
      return equal;
    case AssertCondition::kNotEqual:
      return not_equal;
  }
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_X64_MAGLEV_ASSEMBLER_X64_INL_H_
