// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_ASSEMBLER_H_
#define V8_MAGLEV_MAGLEV_ASSEMBLER_H_

#include "src/codegen/machine-type.h"
#include "src/codegen/macro-assembler.h"
#include "src/flags/flags.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/maglev/maglev-code-gen-state.h"
#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

class Graph;
class MaglevAssembler;

// Label allowed to be passed to deferred code.
class ZoneLabelRef {
 public:
  explicit ZoneLabelRef(Zone* zone) : label_(zone->New<Label>()) {}
  explicit inline ZoneLabelRef(MaglevAssembler* masm);

  static ZoneLabelRef UnsafeFromLabelPointer(Label* label) {
    // This is an unsafe operation, {label} must be zone allocated.
    return ZoneLabelRef(label);
  }

  Label* operator*() { return label_; }

 private:
  Label* label_;

  // Unsafe constructor. {label} must be zone allocated.
  explicit ZoneLabelRef(Label* label) : label_(label) {}
};

// The slot index is the offset from the frame pointer.
struct StackSlot {
  int32_t index;
};

class MaglevAssembler : public MacroAssembler {
 public:
  class ScratchRegisterScope;

  explicit MaglevAssembler(Isolate* isolate, MaglevCodeGenState* code_gen_state)
      : MacroAssembler(isolate, CodeObjectRequired::kNo),
        code_gen_state_(code_gen_state) {}

  inline MemOperand GetStackSlot(const compiler::AllocatedOperand& operand);
  inline MemOperand ToMemOperand(const compiler::InstructionOperand& operand);
  inline MemOperand ToMemOperand(const ValueLocation& location);

  inline int GetFramePointerOffsetForStackSlot(
      const compiler::AllocatedOperand& operand) {
    int index = operand.index();
    if (operand.representation() != MachineRepresentation::kTagged) {
      index += code_gen_state()->tagged_slots();
    }
    return GetFramePointerOffsetForStackSlot(index);
  }

  template <typename Dest, typename Source>
  inline void MoveRepr(MachineRepresentation repr, Dest dst, Source src);

  void Allocate(RegisterSnapshot& register_snapshot, Register result,
                int size_in_bytes,
                AllocationType alloc_type = AllocationType::kYoung,
                AllocationAlignment alignment = kTaggedAligned);

  void AllocateHeapNumber(RegisterSnapshot register_snapshot, Register result,
                          DoubleRegister value);

  void AllocateTwoByteString(RegisterSnapshot register_snapshot,
                             Register result, int length);

  void LoadSingleCharacterString(Register result, int char_code);
  void LoadSingleCharacterString(Register result, Register char_code,
                                 Register scratch);

  inline void BindJumpTarget(Label* label);
  inline void BindBlock(BasicBlock* block);

  inline Condition IsInt64Constant(Register reg, int64_t constant);
  inline Condition IsRootConstant(Input input, RootIndex root_index);

  inline void Branch(Condition condition, BasicBlock* if_true,
                     BasicBlock* if_false, BasicBlock* next_block);
  inline void Branch(Condition condition, Label* if_true,
                     Label::Distance true_distance, bool fallthrough_when_true,
                     Label* if_false, Label::Distance false_distance,
                     bool fallthrough_when_false);

  Register FromAnyToRegister(const Input& input, Register scratch);

  inline void LoadBoundedSizeFromObject(Register result, Register object,
                                        int offset);
  inline void LoadExternalPointerField(Register result, MemOperand operand);

  inline void LoadSignedField(Register result, MemOperand operand,
                              int element_size);
  inline void LoadUnsignedField(Register result, MemOperand operand,
                                int element_size);
  template <typename BitField>
  inline void LoadBitField(Register result, MemOperand operand) {
    // Pick a load with the right size, which makes sure to read the whole
    // field.
    static constexpr int load_size =
        RoundUp<8>(BitField::kSize + BitField::kShift) / 8;
    // TODO(leszeks): If the shift is 8 or 16, we could have loaded from a
    // shifted address instead.
    LoadUnsignedField(result, operand, load_size);
    DecodeField<BitField>(result);
  }

  inline void StoreField(MemOperand operand, Register value, int element_size);
  inline void ReverseByteOrder(Register value, int element_size);

  void BuildTypedArrayDataPointer(Register data_pointer, Register object);

  // Warning: Input registers {string} and {index} will be scratched.
  // {result} is allowed to alias with one the other 3 input registers.
  // {result} is an int32.
  void StringCharCodeAt(RegisterSnapshot& register_snapshot, Register result,
                        Register string, Register index, Register scratch,
                        Label* result_fits_one_byte);
  // Warning: Input {char_code} will be scratched.
  void StringFromCharCode(RegisterSnapshot register_snapshot,
                          Label* char_code_fits_one_byte, Register result,
                          Register char_code, Register scratch);

  void ToBoolean(Register value, ZoneLabelRef is_true, ZoneLabelRef is_false,
                 bool fallthrough_when_true);
  void TestTypeOf(Register object,
                  interpreter::TestTypeOfFlags::LiteralFlag literal,
                  Label* if_true, Label::Distance true_distance,
                  bool fallthrough_when_true, Label* if_false,
                  Label::Distance false_distance, bool fallthrough_when_false);

  inline void DoubleToInt64Repr(Register dst, DoubleRegister src);
  void TruncateDoubleToInt32(Register dst, DoubleRegister src);

  inline void DefineLazyDeoptPoint(LazyDeoptInfo* info);
  inline void DefineExceptionHandlerPoint(NodeBase* node);
  inline void DefineExceptionHandlerAndLazyDeoptPoint(NodeBase* node);

  template <typename Function, typename... Args>
  inline DeferredCodeInfo* PushDeferredCode(Function&& deferred_code_gen,
                                            Args&&... args);
  template <typename Function, typename... Args>
  inline void JumpToDeferredIf(Condition cond, Function&& deferred_code_gen,
                               Args&&... args);

  template <typename NodeT>
  inline Label* GetDeoptLabel(NodeT* node, DeoptimizeReason reason);
  template <typename NodeT>
  inline void EmitEagerDeopt(NodeT* node, DeoptimizeReason reason);
  template <typename NodeT>
  inline void EmitEagerDeoptIf(Condition cond, DeoptimizeReason reason,
                               NodeT* node);
  template <typename NodeT>
  inline void EmitEagerDeoptIfNotEqual(DeoptimizeReason reason, NodeT* node);

  inline void MaterialiseValueNode(Register dst, ValueNode* value);

  inline MemOperand StackSlotOperand(StackSlot slot);
  inline void Move(StackSlot dst, Register src);
  inline void Move(StackSlot dst, DoubleRegister src);
  inline void Move(Register dst, StackSlot src);
  inline void Move(DoubleRegister dst, StackSlot src);
  inline void Move(MemOperand dst, Register src);
  inline void Move(MemOperand dst, DoubleRegister src);
  inline void Move(Register dst, MemOperand src);
  inline void Move(DoubleRegister dst, MemOperand src);
  inline void Move(DoubleRegister dst, DoubleRegister src);
  inline void Move(Register dst, Smi src);
  inline void Move(Register dst, ExternalReference src);
  inline void Move(Register dst, Register src);
  inline void Move(Register dst, TaggedIndex i);
  inline void Move(Register dst, int32_t i);
  inline void Move(DoubleRegister dst, double n);
  inline void Move(Register dst, Handle<HeapObject> obj);

  inline void LoadByte(Register dst, MemOperand src);

  inline void SignExtend32To64Bits(Register dst, Register src);

  template <typename NodeT>
  inline void DeoptIfBufferDetached(Register array, Register scratch,
                                    NodeT* node);

  inline void CompareTagged(Register reg, Handle<HeapObject> obj);

  inline void CompareInt32(Register reg, int32_t imm);
  inline void CompareInt32(Register src1, Register src2);

  inline void Jump(Label* target, Label::Distance distance = Label::kFar);
  inline void JumpIf(Condition cond, Label* target,
                     Label::Distance distance = Label::kFar);
  inline void JumpIfEqual(Label* target,
                          Label::Distance distance = Label::kFar);
  inline void JumpIfNotEqual(Label* target,
                             Label::Distance distance = Label::kFar);
  inline void JumpIfTaggedEqual(Register r1, Register r2, Label* target,
                                Label::Distance distance = Label::kFar);

  // TODO(victorgomes): Import baseline Pop(T...) methods.
  inline void Pop(Register dst);
  using MacroAssembler::Pop;

  template <typename... T>
  inline void Push(T... vals);
  template <typename... T>
  inline void PushReverse(T... vals);

  void Prologue(Graph* graph);

  inline void FinishCode();

  inline void AssertStackSizeCorrect();

  void MaybeEmitDeoptBuiltinsCall(size_t eager_deopt_count,
                                  Label* eager_deopt_entry,
                                  size_t lazy_deopt_count,
                                  Label* lazy_deopt_entry);

  compiler::NativeContextRef native_context() const {
    return code_gen_state()->broker()->target_native_context();
  }

  MaglevCodeGenState* code_gen_state() const { return code_gen_state_; }
  MaglevSafepointTableBuilder* safepoint_table_builder() const {
    return code_gen_state()->safepoint_table_builder();
  }
  MaglevCompilationInfo* compilation_info() const {
    return code_gen_state()->compilation_info();
  }

 private:
  inline constexpr int GetFramePointerOffsetForStackSlot(int index) {
    return StandardFrameConstants::kExpressionsOffset -
           index * kSystemPointerSize;
  }

  MaglevCodeGenState* const code_gen_state_;
  ScratchRegisterScope* scratch_register_scope_ = nullptr;
};

class SaveRegisterStateForCall {
 public:
  SaveRegisterStateForCall(MaglevAssembler* masm, RegisterSnapshot snapshot)
      : masm(masm), snapshot_(snapshot) {
    masm->PushAll(snapshot_.live_registers);
    masm->PushAll(snapshot_.live_double_registers, kDoubleSize);
  }

  ~SaveRegisterStateForCall() {
    masm->PopAll(snapshot_.live_double_registers, kDoubleSize);
    masm->PopAll(snapshot_.live_registers);
  }

  MaglevSafepointTableBuilder::Safepoint DefineSafepoint() {
    // TODO(leszeks): Avoid emitting safepoints when there are no registers to
    // save.
    auto safepoint = masm->safepoint_table_builder()->DefineSafepoint(masm);
    int pushed_reg_index = 0;
    for (Register reg : snapshot_.live_registers) {
      if (snapshot_.live_tagged_registers.has(reg)) {
        safepoint.DefineTaggedRegister(pushed_reg_index);
      }
      pushed_reg_index++;
    }
#ifdef V8_TARGET_ARCH_ARM64
    pushed_reg_index = RoundUp<2>(pushed_reg_index);
#endif
    int num_pushed_double_reg = snapshot_.live_double_registers.Count();
#ifdef V8_TARGET_ARCH_ARM64
    num_pushed_double_reg = RoundUp<2>(num_pushed_double_reg);
#endif
    safepoint.SetNumPushedRegisters(pushed_reg_index + num_pushed_double_reg);
    return safepoint;
  }

  MaglevSafepointTableBuilder::Safepoint DefineSafepointWithLazyDeopt(
      LazyDeoptInfo* lazy_deopt_info) {
    lazy_deopt_info->set_deopting_call_return_pc(
        masm->pc_offset_for_safepoint());
    masm->code_gen_state()->PushLazyDeopt(lazy_deopt_info);
    return DefineSafepoint();
  }

 private:
  MaglevAssembler* masm;
  RegisterSnapshot snapshot_;
};

ZoneLabelRef::ZoneLabelRef(MaglevAssembler* masm)
    : ZoneLabelRef(masm->compilation_info()->zone()) {}

// ---
// Deopt
// ---

template <typename NodeT>
inline Label* MaglevAssembler::GetDeoptLabel(NodeT* node,
                                             DeoptimizeReason reason) {
  static_assert(NodeT::kProperties.can_eager_deopt());
  EagerDeoptInfo* deopt_info = node->eager_deopt_info();
  if (deopt_info->reason() != DeoptimizeReason::kUnknown) {
    DCHECK_EQ(deopt_info->reason(), reason);
  }
  if (deopt_info->deopt_entry_label()->is_unused()) {
    code_gen_state()->PushEagerDeopt(deopt_info);
    deopt_info->set_reason(reason);
  }
  return node->eager_deopt_info()->deopt_entry_label();
}

template <typename NodeT>
inline void MaglevAssembler::EmitEagerDeopt(NodeT* node,
                                            DeoptimizeReason reason) {
  RecordComment("-- Jump to eager deopt");
  Jump(GetDeoptLabel(node, reason));
}

template <typename NodeT>
inline void MaglevAssembler::EmitEagerDeoptIf(Condition cond,
                                              DeoptimizeReason reason,
                                              NodeT* node) {
  RecordComment("-- Jump to eager deopt");
  JumpIf(cond, GetDeoptLabel(node, reason));
}

inline void MaglevAssembler::DefineLazyDeoptPoint(LazyDeoptInfo* info) {
  info->set_deopting_call_return_pc(pc_offset_for_safepoint());
  code_gen_state()->PushLazyDeopt(info);
  safepoint_table_builder()->DefineSafepoint(this);
}

inline void MaglevAssembler::DefineExceptionHandlerPoint(NodeBase* node) {
  ExceptionHandlerInfo* info = node->exception_handler_info();
  if (!info->HasExceptionHandler()) return;
  info->pc_offset = pc_offset_for_safepoint();
  code_gen_state()->PushHandlerInfo(node);
}

inline void MaglevAssembler::DefineExceptionHandlerAndLazyDeoptPoint(
    NodeBase* node) {
  DefineExceptionHandlerPoint(node);
  DefineLazyDeoptPoint(node->lazy_deopt_info());
}

// Helpers for pushing arguments.
template <typename T>
class RepeatIterator {
 public:
  // Although we pretend to be a random access iterator, only methods that are
  // required for Push() are implemented right now.
  typedef std::random_access_iterator_tag iterator_category;
  typedef T value_type;
  typedef int difference_type;
  typedef T* pointer;
  typedef T reference;
  RepeatIterator(T val, int count) : val_(val), count_(count) {}
  reference operator*() const { return val_; }
  pointer operator->() { return &val_; }
  RepeatIterator& operator++() {
    ++count_;
    return *this;
  }
  RepeatIterator& operator--() {
    --count_;
    return *this;
  }
  RepeatIterator& operator+=(difference_type diff) {
    count_ += diff;
    return *this;
  }
  bool operator!=(const RepeatIterator<T>& that) const {
    return count_ != that.count_;
  }
  bool operator==(const RepeatIterator<T>& that) const {
    return count_ == that.count_;
  }
  difference_type operator-(const RepeatIterator<T>& it) const {
    return count_ - it.count_;
  }

 private:
  T val_;
  int count_;
};

template <typename T>
auto RepeatValue(T val, int count) {
  return base::make_iterator_range(RepeatIterator<T>(val, 0),
                                   RepeatIterator<T>(val, count));
}

namespace detail {

template <class T>
struct is_iterator_range : std::false_type {};
template <typename T>
struct is_iterator_range<base::iterator_range<T>> : std::true_type {};

}  // namespace detail

// General helpers.

inline bool AnyMapIsHeapNumber(const ZoneHandleSet<Map>& maps) {
  return std::any_of(maps.begin(), maps.end(),
                     [](Handle<Map> map) { return map->IsHeapNumberMap(); });
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_ASSEMBLER_H_
