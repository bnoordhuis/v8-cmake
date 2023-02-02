// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/logging.h"
#include "src/codegen/arm64/assembler-arm64-inl.h"
#include "src/codegen/arm64/register-arm64.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/maglev/maglev-assembler-inl.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/objects/feedback-cell.h"
#include "src/objects/js-function.h"

namespace v8 {
namespace internal {
namespace maglev {

#define __ masm->

void Int32NegateWithOverflow::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineAsRegister(this);
}

void Int32NegateWithOverflow::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register value = ToRegister(value_input()).W();
  Register out = ToRegister(result()).W();

  // Deopt when result would be -0.
  static_assert(Int32NegateWithOverflow::kProperties.can_eager_deopt());
  Label* fail = __ GetDeoptLabel(this, DeoptimizeReason::kOverflow);
  __ RecordComment("-- Jump to eager deopt");
  __ Cbz(value, fail);

  __ negs(out, value);
  // Output register must not be a register input into the eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

void Int32IncrementWithOverflow::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineAsRegister(this);
}

void Int32IncrementWithOverflow::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register value = ToRegister(value_input()).W();
  Register out = ToRegister(result()).W();
  __ Adds(out, value, Immediate(1));
  // Output register must not be a register input into the eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

void Int32DecrementWithOverflow::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineAsRegister(this);
}

void Int32DecrementWithOverflow::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  Register value = ToRegister(value_input()).W();
  Register out = ToRegister(result()).W();
  __ Subs(out, value, Immediate(1));
  // Output register must not be a register input into the eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

int ConvertReceiver::MaxCallStackArgs() const {
  using D = CallInterfaceDescriptorFor<Builtin::kToObject>::type;
  return D::GetStackParameterCount();
}
void ConvertReceiver::SetValueLocationConstraints() {
  using D = CallInterfaceDescriptorFor<Builtin::kToObject>::type;
  UseFixed(receiver_input(), D::GetRegisterParameter(D::kInput));
  DefineAsFixed(this, kReturnRegister0);
}
void ConvertReceiver::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Label convert_to_object, done;
  Register receiver = ToRegister(receiver_input());
  __ JumpIfSmi(receiver, &convert_to_object);
  static_assert(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  {
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ JumpIfObjectType(receiver, scratch, scratch, FIRST_JS_RECEIVER_TYPE,
                        &done, hs);
  }

  if (mode_ != ConvertReceiverMode::kNotNullOrUndefined) {
    Label convert_global_proxy;
    __ JumpIfRoot(receiver, RootIndex::kUndefinedValue, &convert_global_proxy);
    __ JumpIfNotRoot(receiver, RootIndex::kNullValue, &convert_to_object);
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
  {
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ LoadMap(scratch, value);
    __ CompareInstanceType(scratch, scratch.W(), FIRST_JS_RECEIVER_TYPE);
    __ B(&done, hs);
  }
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
  {
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ LoadMap(scratch, value);
    __ CompareInstanceType(scratch, scratch.W(), FIRST_NONSTRING_TYPE);
    __ B(&done, lo);
  }
  __ bind(&call_builtin);
  __ CallBuiltin(Builtin::kToString);
  masm->DefineExceptionHandlerAndLazyDeoptPoint(this);
  __ bind(&done);
}

void AssertInt32::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
}
void AssertInt32::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  __ Cmp(ToRegister(left_input()).W(), ToRegister(right_input()).W());
  __ Check(ToCondition(condition_), reason_);
}

void CheckJSObjectElementsBounds::SetValueLocationConstraints() {
  UseRegister(receiver_input());
  set_temporaries_needed(1);
  UseRegister(index_input());
}
void CheckJSObjectElementsBounds::GenerateCode(MaglevAssembler* masm,
                                               const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  Register object = ToRegister(receiver_input());
  Register index = ToRegister(index_input()).W();

  __ AssertNotSmi(object);

  if (v8_flags.debug_code) {
    __ CompareObjectType(object, scratch, scratch, FIRST_JS_OBJECT_TYPE);
    __ Assert(ge, AbortReason::kUnexpectedValue);
  }
  __ LoadAnyTaggedField(scratch,
                        FieldMemOperand(object, JSObject::kElementsOffset));
  if (v8_flags.debug_code) {
    __ AssertNotSmi(scratch);
  }
  __ SmiUntagField(scratch,
                   FieldMemOperand(scratch, FixedArray::kLengthOffset));
  __ Cmp(index, scratch.W());
  __ EmitEagerDeoptIf(hs, DeoptimizeReason::kOutOfBounds, this);
}

int BuiltinStringFromCharCode::MaxCallStackArgs() const {
  return AllocateDescriptor::GetStackParameterCount();
}
void BuiltinStringFromCharCode::SetValueLocationConstraints() {
  if (code_input().node()->Is<Int32Constant>()) {
    UseAny(code_input());
  } else {
    UseRegister(code_input());
  }
  set_temporaries_needed(1);
  DefineAsRegister(this);
}
void BuiltinStringFromCharCode::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  Register result_string = ToRegister(result());
  if (Int32Constant* constant = code_input().node()->TryCast<Int32Constant>()) {
    int32_t char_code = constant->value();
    if (0 <= char_code && char_code < String::kMaxOneByteCharCode) {
      __ LoadSingleCharacterString(result_string, char_code);
    } else {
      __ AllocateTwoByteString(register_snapshot(), result_string, 1);
      __ Move(scratch, char_code & 0xFFFF);
      __ Strh(scratch.W(),
              FieldMemOperand(result_string, SeqTwoByteString::kHeaderSize));
    }
  } else {
    __ StringFromCharCode(register_snapshot(), nullptr, result_string,
                          ToRegister(code_input()), scratch);
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
}
void BuiltinStringPrototypeCharCodeAt::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  Label done;
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  RegisterSnapshot save_registers = register_snapshot();
  __ StringCharCodeAt(save_registers, ToRegister(result()),
                      ToRegister(string_input()), ToRegister(index_input()),
                      scratch, &done);
  __ bind(&done);
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
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  __ Move(scratch, map().object());
  __ StoreTaggedField(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
  __ LoadRoot(scratch, RootIndex::kEmptyFixedArray);
  __ StoreTaggedField(
      scratch, FieldMemOperand(object, JSObject::kPropertiesOrHashOffset));
  __ StoreTaggedField(scratch,
                      FieldMemOperand(object, JSObject::kElementsOffset));
  __ LoadRoot(scratch, RootIndex::kUndefinedValue);
  for (int i = 0; i < map().GetInObjectProperties(); i++) {
    int offset = map().GetInObjectPropertyOffset(i);
    __ StoreTaggedField(scratch, FieldMemOperand(object, offset));
  }
}

void CheckedInt32ToUint32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void CheckedInt32ToUint32::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register input_reg = ToRegister(input()).W();
  __ Tst(input_reg, input_reg);
  __ EmitEagerDeoptIf(mi, DeoptimizeReason::kNotUint32, this);
}

void CheckedUint32ToInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineSameAsFirst(this);
}
void CheckedUint32ToInt32::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register input_reg = ToRegister(input()).W();
  // Check if the top bit is set -- if it is, then this is not a valid int32,
  // otherwise it is.
  static_assert(CheckedUint32ToInt32::kProperties.can_eager_deopt());
  Label* fail = __ GetDeoptLabel(this, DeoptimizeReason::kNotInt32);
  __ RecordComment("-- Jump to eager deopt");
  __ Tbnz(input_reg, 31, fail);
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

  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();

  if (v8_flags.debug_code) {
    __ CompareObjectType(object, scratch, scratch, JS_ARRAY_TYPE);
    __ Assert(eq, AbortReason::kUnexpectedValue);
  }

  __ SmiUntagField(scratch, FieldMemOperand(object, JSArray::kLengthOffset));
  __ Cmp(index, scratch);
  __ EmitEagerDeoptIf(hs, DeoptimizeReason::kOutOfBounds, this);
}

void ChangeInt32ToFloat64::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void ChangeInt32ToFloat64::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  __ Scvtf(ToDoubleRegister(result()), ToRegister(input()).W());
}

void ChangeUint32ToFloat64::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void ChangeUint32ToFloat64::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  __ Ucvtf(ToDoubleRegister(result()), ToRegister(input()).W());
}

void CheckedTruncateFloat64ToInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void CheckedTruncateFloat64ToInt32::GenerateCode(MaglevAssembler* masm,
                                                 const ProcessingState& state) {
  DoubleRegister input_reg = ToDoubleRegister(input());
  Register result_reg = ToRegister(result()).W();

  MaglevAssembler::ScratchRegisterScope temps(masm);
  DoubleRegister converted_back = temps.AcquireDouble();

  // Convert the input float64 value to int32.
  __ Fcvtzs(result_reg, input_reg);
  // Convert that int32 value back to float64.
  __ Scvtf(converted_back, result_reg);
  // Check that the result of the float64->int32->float64 is equal to the input
  // (i.e. that the conversion didn't truncate.
  __ Fcmp(input_reg, converted_back);
  __ EmitEagerDeoptIf(ne, DeoptimizeReason::kNotInt32, this);

  // Check if {input} is -0.
  Label check_done;
  __ Cmp(result_reg, wzr);
  __ B(&check_done, ne);

  // In case of 0, we need to check the high bits for the IEEE -0 pattern.
  Register high_word32_of_input = temps.Acquire().W();
  __ Umov(high_word32_of_input, input_reg.V2S(), 1);
  __ Cmp(high_word32_of_input, wzr);
  __ EmitEagerDeoptIf(lt, DeoptimizeReason::kNotInt32, this);

  __ bind(&check_done);
}

void CheckedTruncateFloat64ToUint32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void CheckedTruncateFloat64ToUint32::GenerateCode(
    MaglevAssembler* masm, const ProcessingState& state) {
  DoubleRegister input_reg = ToDoubleRegister(input());
  Register result_reg = ToRegister(result()).W();

  MaglevAssembler::ScratchRegisterScope temps(masm);
  DoubleRegister converted_back = temps.AcquireDouble();

  // Convert the input float64 value to uint32.
  __ Fcvtzu(result_reg, input_reg);
  // Convert that uint32 value back to float64.
  __ Ucvtf(converted_back, result_reg);
  // Check that the result of the float64->uint32->float64 is equal to the input
  // (i.e. that the conversion didn't truncate.
  __ Fcmp(input_reg, converted_back);
  __ EmitEagerDeoptIf(ne, DeoptimizeReason::kNotUint32, this);

  // Check if {input} is -0.
  Label check_done;
  __ Cmp(result_reg, wzr);
  __ B(&check_done, ne);

  // In case of 0, we need to check the high bits for the IEEE -0 pattern.
  Register high_word32_of_input = temps.Acquire().W();
  __ Umov(high_word32_of_input, input_reg.V2S(), 1);
  __ Cmp(high_word32_of_input, wzr);
  __ EmitEagerDeoptIf(lt, DeoptimizeReason::kNotUint32, this);

  __ bind(&check_done);
}

namespace {

void EmitTruncateNumberToInt32(MaglevAssembler* masm, Register value,
                               Register result_reg, Label* not_a_number) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Label is_not_smi, done;
  // Check if Smi.
  __ JumpIfNotSmi(value, &is_not_smi);
  // If Smi, convert to Int32.
  __ SmiToInt32(result_reg, value);
  __ B(&done);
  __ bind(&is_not_smi);
  if (not_a_number != nullptr) {
    // Check if HeapNumber, deopt otherwise.
    Register scratch = temps.Acquire().W();
    __ Ldr(scratch, FieldMemOperand(value, HeapObject::kMapOffset));
    __ CompareRoot(scratch, RootIndex::kHeapNumberMap);
    __ RecordComment("-- Jump to eager deopt");
    __ JumpIf(ne, not_a_number);
  } else if (v8_flags.debug_code) {
    Register scratch = temps.Acquire().W();
    __ Ldr(scratch, FieldMemOperand(value, HeapObject::kMapOffset));
    __ CompareRoot(scratch, RootIndex::kHeapNumberMap);
    __ Assert(eq, AbortReason::kUnexpectedValue);
  }
  DoubleRegister double_value = temps.AcquireDouble();
  __ Ldr(double_value, FieldMemOperand(value, HeapNumber::kValueOffset));
  __ TruncateDoubleToInt32(result_reg, double_value);
  __ bind(&done);
}

}  // namespace

void CheckedTruncateNumberToInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void CheckedTruncateNumberToInt32::GenerateCode(MaglevAssembler* masm,
                                                const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register value = ToRegister(input());
  Register result_reg = ToRegister(result());
  Label* deopt_label = __ GetDeoptLabel(this, DeoptimizeReason::kNotANumber);
  EmitTruncateNumberToInt32(masm, value, result_reg, deopt_label);
}

void TruncateNumberToInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void TruncateNumberToInt32::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  Register value = ToRegister(input());
  Register result_reg = ToRegister(result());
  EmitTruncateNumberToInt32(masm, value, result_reg, nullptr);
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
      __ B(&done, is_smi);
    } else {
      __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kWrongMap, this);
    }
  }

  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register object_map = temps.Acquire();
  Register map = temps.Acquire();
  __ LoadMap(object_map, object);
  size_t map_count = maps().size();
  for (size_t i = 0; i < map_count - 1; ++i) {
    Handle<Map> map_handle = maps().at(i);
    __ Move(map, map_handle);
    __ CmpTagged(object_map, map);
    __ B(&done, eq);
  }
  Handle<Map> last_map_handle = maps().at(map_count - 1);
  __ Move(map, last_map_handle);
  __ CmpTagged(object_map, map);
  __ EmitEagerDeoptIf(ne, DeoptimizeReason::kWrongMap, this);
  __ bind(&done);
}

int CheckMapsWithMigration::MaxCallStackArgs() const {
  DCHECK_EQ(Runtime::FunctionForId(Runtime::kTryMigrateInstance)->nargs, 1);
  return 1;
}
void CheckMapsWithMigration::SetValueLocationConstraints() {
  UseRegister(receiver_input());
  set_temporaries_needed(1);
}
void CheckMapsWithMigration::GenerateCode(MaglevAssembler* masm,
                                          const ProcessingState& state) {
  // TODO(victorgomes): This can happen, because we do not emit an unconditional
  // deopt when we intersect the map sets.
  if (maps().is_empty()) {
    __ EmitEagerDeopt(this, DeoptimizeReason::kWrongMap);
    return;
  }

  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register object = ToRegister(receiver_input());

  bool maps_include_heap_number = AnyMapIsHeapNumber(maps());

  ZoneLabelRef done(masm);
  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    if (maps_include_heap_number) {
      // Smis count as matching the HeapNumber map, so we're done.
      __ B(*done, is_smi);
    } else {
      __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kWrongMap, this);
    }
  }

  // Use general temporaries to be able to send to deferred code.
  Register object_map = temps.Acquire();
  Register scratch = temps.Acquire();
  __ LoadMap(object_map, object);

  size_t map_count = maps().size();
  for (size_t i = 0; i < map_count; ++i) {
    ZoneLabelRef continue_label(masm);
    Handle<Map> map_handle = maps().at(i);
    {
      Register map = scratch;
      __ Move(map, map_handle);
      __ CmpTagged(object_map, map);
    }
    bool last_map = (i == map_count - 1);
    if (map_handle->is_migration_target()) {
      __ JumpToDeferredIf(
          ne,
          [](MaglevAssembler* masm, ZoneLabelRef continue_label,
             ZoneLabelRef done, Register object, Register object_map,
             Register scratch, int map_index, CheckMapsWithMigration* node) {
            // If the map is not deprecated, we fail the map check, continue to
            // the next one.
            __ Ldr(scratch.W(),
                   FieldMemOperand(object_map, Map::kBitField3Offset));
            __ TestAndBranchIfAllClear(scratch.W(),
                                       Map::Bits3::IsDeprecatedBit::kMask,
                                       *continue_label);

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
                DCHECK(!register_snapshot.live_registers.has(scratch));
                __ Mov(scratch, return_val);
                return_val = scratch;
              }
            }

            // On failure, the returned value is zero
            __ Cbz(return_val, *continue_label);

            // The migrated object is returned on success, retry the map check.
            __ Move(object, return_val);
            __ LoadMap(object_map, object);
            __ Move(scratch, node->maps().at(map_index));
            __ CmpTagged(object_map, scratch);
            __ B(*done, eq);
            __ B(*continue_label);
          },
          // If this is the last map to check, we should deopt if we fail.
          // This is safe to do, since {eager_deopt_info} is ZoneAllocated.
          (last_map ? ZoneLabelRef::UnsafeFromLabelPointer(masm->GetDeoptLabel(
                          this, DeoptimizeReason::kWrongMap))
                    : continue_label),
          done, object, object_map, scratch, i, this);
    } else if (last_map) {
      // If it is the last map and it is not a migration target, we should deopt
      // if the check fails.
      __ EmitEagerDeoptIf(ne, DeoptimizeReason::kWrongMap, this);
    }

    if (!last_map) {
      // We don't need to bind the label for the last map.
      __ B(*done, eq);
      __ bind(*continue_label);
    }
  }

  __ bind(*done);
}

void CheckNumber::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckNumber::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  Label done;
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  Register value = ToRegister(receiver_input());
  // If {value} is a Smi or a HeapNumber, we're done.
  __ JumpIfSmi(value, &done);
  if (mode() == Object::Conversion::kToNumeric) {
    __ LoadMap(scratch, value);
    __ CompareRoot(scratch.W(), RootIndex::kHeapNumberMap);
    // Jump to done if it is a HeapNumber.
    __ B(&done, eq);
    // Check if it is a BigInt.
    __ Ldrh(scratch.W(), FieldMemOperand(scratch, Map::kInstanceTypeOffset));
    __ Cmp(scratch, Immediate(BIGINT_TYPE));
  } else {
    __ Ldr(scratch.W(), FieldMemOperand(value, HeapObject::kMapOffset));
    __ CompareRoot(scratch, RootIndex::kHeapNumberMap);
  }
  __ EmitEagerDeoptIf(ne, DeoptimizeReason::kNotANumber, this);
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
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  __ CompareObjectType(object, scratch, scratch, SYMBOL_TYPE);
  __ EmitEagerDeoptIf(ne, DeoptimizeReason::kNotASymbol, this);
}

void CheckInstanceType::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckInstanceType::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  Register object = ToRegister(receiver_input());
  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kWrongInstanceType, this);
  }
  __ LoadMap(scratch, object);
  __ CompareInstanceType(scratch, scratch, instance_type());
  __ EmitEagerDeoptIf(ne, DeoptimizeReason::kWrongInstanceType, this);
}

void CheckString::SetValueLocationConstraints() {
  UseRegister(receiver_input());
}
void CheckString::GenerateCode(MaglevAssembler* masm,
                               const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  Register object = ToRegister(receiver_input());
  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kNotAString, this);
  }
  __ LoadMap(scratch, object);
  __ CompareInstanceTypeRange(scratch, scratch, FIRST_STRING_TYPE,
                              LAST_STRING_TYPE);
  __ EmitEagerDeoptIf(hi, DeoptimizeReason::kNotAString, this);
}

int CheckedObjectToIndex::MaxCallStackArgs() const { return 0; }
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
        MaglevAssembler::ScratchRegisterScope temps(masm);
        Register scratch = temps.Acquire();
        __ LoadMap(scratch, object);
        __ CompareInstanceTypeRange(scratch, scratch, FIRST_STRING_TYPE,
                                    LAST_STRING_TYPE);
        __ b(&is_string, ls);

        __ Cmp(scratch, Immediate(HEAP_NUMBER_TYPE));
        // The IC will go generic if it encounters something other than a
        // Number or String key.
        __ EmitEagerDeoptIf(ne, DeoptimizeReason::kNotInt32, node);

        // Heap Number.
        {
          DoubleRegister number_value = temps.AcquireDouble();
          DoubleRegister converted_back = temps.AcquireDouble();
          __ Ldr(number_value,
                 FieldMemOperand(object, HeapNumber::kValueOffset));
          // Convert the input float64 value to int32.
          __ TruncateDoubleToInt32(result_reg, number_value);
          // Convert that int32 value back to float64.
          __ Scvtf(converted_back, result_reg);
          // Check that the result of the float64->int32->float64 is equal to
          // the input (i.e. that the conversion didn't truncate.
          __ Fcmp(number_value, converted_back);
          __ B(*done, eq);
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
            __ Move(x0, object);
            __ CallCFunction(
                ExternalReference::string_to_array_index_function(), 1);
            // No need for safepoint since this is a fast C call.
            __ Move(result_reg, kReturnRegister0);
          }
          __ Cmp(result_reg, Immediate(0));
          __ B(*done, ge);
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

void Int32ToNumber::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void Int32ToNumber::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  ZoneLabelRef done(masm);
  Register object = ToRegister(result());
  Register value = ToRegister(input());
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  __ Adds(scratch.W(), value.W(), value.W());
  __ JumpToDeferredIf(
      vs,
      [](MaglevAssembler* masm, Register object, Register value,
         Register scratch, ZoneLabelRef done, Int32ToNumber* node) {
        MaglevAssembler::ScratchRegisterScope temps(masm);
        // We can include {scratch} back to the temporary set, since we jump
        // over its use to the label {done}.
        temps.Include(scratch);
        DoubleRegister double_value = temps.AcquireDouble();
        __ Scvtf(double_value, value.W());
        __ AllocateHeapNumber(node->register_snapshot(), object, double_value);
        __ B(*done);
      },
      object, value, scratch, done, this);
  __ Mov(object, scratch);
  __ bind(*done);
}

void Uint32ToNumber::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void Uint32ToNumber::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  ZoneLabelRef done(masm);
  Register value = ToRegister(input());
  Register object = ToRegister(result());
  __ Cmp(value.W(), Immediate(Smi::kMaxValue));
  __ JumpToDeferredIf(
      hi,
      [](MaglevAssembler* masm, Register object, Register value,
         ZoneLabelRef done, Uint32ToNumber* node) {
        MaglevAssembler::ScratchRegisterScope temps(masm);
        DoubleRegister double_value = temps.AcquireDouble();
        __ Ucvtf(double_value, value.W());
        __ AllocateHeapNumber(node->register_snapshot(), object, double_value);
        __ B(*done);
      },
      object, value, done, this);
  __ Add(object, value, value);
  __ bind(*done);
}

void Int32AddWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

void Int32AddWithOverflow::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register left = ToRegister(left_input()).W();
  Register right = ToRegister(right_input()).W();
  Register out = ToRegister(result()).W();
  __ Adds(out, left, right);
  // The output register shouldn't be a register input into the eager deopt
  // info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

void Int32SubtractWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}
void Int32SubtractWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register left = ToRegister(left_input()).W();
  Register right = ToRegister(right_input()).W();
  Register out = ToRegister(result()).W();
  __ Subs(out, left, right);
  // The output register shouldn't be a register input into the eager deopt
  // info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

void Int32MultiplyWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}
void Int32MultiplyWithOverflow::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  Register left = ToRegister(left_input()).W();
  Register right = ToRegister(right_input()).W();
  Register out = ToRegister(result()).W();

  // TODO(leszeks): peephole optimise multiplication by a constant.

  MaglevAssembler::ScratchRegisterScope temps(masm);
  bool out_alias_input = out == left || out == right;
  Register res = out.X();
  if (out_alias_input) {
    res = temps.Acquire();
  }

  __ Smull(res, left, right);

  // if res != (res[0:31] sign extended to 64 bits), then the multiplication
  // result is too large for 32 bits.
  __ Cmp(res, Operand(res.W(), SXTW));
  __ EmitEagerDeoptIf(ne, DeoptimizeReason::kOverflow, this);

  // If the result is zero, check if either lhs or rhs is negative.
  Label end;
  __ CompareAndBranch(res, Immediate(0), ne, &end);
  {
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register temp = temps.Acquire().W();
    __ orr(temp, left, right);
    __ Cmp(temp, Immediate(0));
    // If one of them is negative, we must have a -0 result, which is non-int32,
    // so deopt.
    // TODO(leszeks): Consider splitting these deopts to have distinct deopt
    // reasons. Otherwise, the reason has to match the above.
    __ EmitEagerDeoptIf(lt, DeoptimizeReason::kOverflow, this);
  }
  __ bind(&end);
  if (out_alias_input) {
    __ Move(out, res.W());
  }
}

void Int32DivideWithOverflow::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}
void Int32DivideWithOverflow::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  Register left = ToRegister(left_input()).W();
  Register right = ToRegister(right_input()).W();
  Register out = ToRegister(result()).W();

  // TODO(leszeks): peephole optimise division by a constant.

  // Pre-check for overflow, since idiv throws a division exception on overflow
  // rather than setting the overflow flag. Logic copied from
  // effect-control-linearizer.cc

  // Check if {right} is positive (and not zero).
  __ Cmp(right, Immediate(0));
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(
      le,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register left,
         Register right, Int32DivideWithOverflow* node) {
        // {right} is negative or zero.

        // Check if {right} is zero.
        // We've already done the compare and flags won't be cleared yet.
        // TODO(leszeks): Using kNotInt32 here, but kDivisionByZero would be
        // better. Right now all eager deopts in a node have to be the same --
        // we should allow a node to emit multiple eager deopts with different
        // reasons.
        __ EmitEagerDeoptIf(eq, DeoptimizeReason::kNotInt32, node);

        // Check if {left} is zero, as that would produce minus zero.
        __ Cmp(left, Immediate(0));
        // TODO(leszeks): Better DeoptimizeReason = kMinusZero.
        __ EmitEagerDeoptIf(eq, DeoptimizeReason::kNotInt32, node);

        // Check if {left} is kMinInt and {right} is -1, in which case we'd have
        // to return -kMinInt, which is not representable as Int32.
        __ Cmp(left, Immediate(kMinInt));
        __ JumpIf(ne, *done);
        __ Cmp(right, Immediate(-1));
        __ JumpIf(ne, *done);
        // TODO(leszeks): Better DeoptimizeReason = kOverflow, but
        // eager_deopt_info is already configured as kNotInt32.
        __ EmitEagerDeopt(node, DeoptimizeReason::kNotInt32);
      },
      done, left, right, this);
  __ bind(*done);

  // Perform the actual integer division.
  MaglevAssembler::ScratchRegisterScope temps(masm);
  bool out_alias_input = out == left || out == right;
  Register res = out;
  if (out_alias_input) {
    res = temps.Acquire().W();
  }
  __ sdiv(res, left, right);

  // Check that the remainder is zero.
  Register temp = temps.Acquire().W();
  __ Msub(temp, res, right, left);
  __ Cmp(temp, Immediate(0));
  __ EmitEagerDeoptIf(ne, DeoptimizeReason::kNotInt32, this);

  __ Move(out, res);
}

void Int32ModulusWithOverflow::SetValueLocationConstraints() {
  UseAndClobberRegister(left_input());
  UseAndClobberRegister(right_input());
  DefineAsRegister(this);
}
void Int32ModulusWithOverflow::GenerateCode(MaglevAssembler* masm,
                                            const ProcessingState& state) {
  // If AreAliased(lhs, rhs):
  //   deopt if lhs < 0  // Minus zero.
  //   0
  //
  // Using same algorithm as in EffectControlLinearizer:
  //   if rhs <= 0 then
  //     rhs = -rhs
  //     deopt if rhs == 0
  //   if lhs < 0 then
  //     let lhs_abs = -lsh in
  //     let res = lhs_abs % rhs in
  //     deopt if res == 0
  //     -res
  //   else
  //     let msk = rhs - 1 in
  //     if rhs & msk == 0 then
  //       lhs & msk
  //     else
  //       lhs % rhs

  Register lhs = ToRegister(left_input()).W();
  Register rhs = ToRegister(right_input()).W();
  Register out = ToRegister(result()).W();

  static constexpr DeoptimizeReason deopt_reason =
      DeoptimizeReason::kDivisionByZero;

  if (lhs == rhs) {
    // For the modulus algorithm described above, lhs and rhs must not alias
    // each other.
    __ Tst(lhs, lhs);
    // TODO(victorgomes): This ideally should be kMinusZero, but Maglev only
    // allows one deopt reason per IR.
    __ EmitEagerDeoptIf(mi, deopt_reason, this);
    __ Move(ToRegister(result()), 0);
    return;
  }

  DCHECK(!AreAliased(lhs, rhs));

  ZoneLabelRef done(masm);
  ZoneLabelRef rhs_checked(masm);
  __ Cmp(rhs, Immediate(0));
  __ JumpToDeferredIf(
      le,
      [](MaglevAssembler* masm, ZoneLabelRef rhs_checked, Register rhs,
         Int32ModulusWithOverflow* node) {
        __ Negs(rhs, rhs);
        __ EmitEagerDeoptIf(eq, deopt_reason, node);
        __ Jump(*rhs_checked);
      },
      rhs_checked, rhs, this);
  __ bind(*rhs_checked);

  __ Cmp(lhs, Immediate(0));
  __ JumpToDeferredIf(
      lt,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register lhs, Register rhs,
         Register out, Int32ModulusWithOverflow* node) {
        MaglevAssembler::ScratchRegisterScope temps(masm);
        Register res = temps.Acquire().W();
        __ neg(lhs, lhs);
        __ udiv(res, lhs, rhs);
        __ msub(out, res, rhs, lhs);
        __ Cmp(out, Immediate(0));
        // TODO(victorgomes): This ideally should be kMinusZero, but Maglev
        // only allows one deopt reason per IR.
        __ EmitEagerDeoptIf(eq, deopt_reason, node);
        __ neg(out, out);
        __ b(*done);
      },
      done, lhs, rhs, out, this);

  Label rhs_not_power_of_2;
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register mask = temps.Acquire().W();
  __ Add(mask, rhs, Immediate(-1));
  __ Tst(mask, rhs);
  __ JumpIf(ne, &rhs_not_power_of_2);

  // {rhs} is power of 2.
  __ And(out, mask, lhs);
  __ Jump(*done);

  __ bind(&rhs_not_power_of_2);

  // We store the result of the Udiv in a temporary register in case {out} is
  // the same as {lhs} or {rhs}: we'll still need those 2 registers intact to
  // get the remainder.
  Register res = mask;
  __ Udiv(res, lhs, rhs);
  __ Msub(out, res, rhs, lhs);

  __ bind(*done);
}

#define DEF_BITWISE_BINOP(Instruction, opcode)                   \
  void Instruction::SetValueLocationConstraints() {              \
    UseRegister(left_input());                                   \
    UseRegister(right_input());                                  \
    DefineAsRegister(this);                                      \
  }                                                              \
                                                                 \
  void Instruction::GenerateCode(MaglevAssembler* masm,          \
                                 const ProcessingState& state) { \
    Register left = ToRegister(left_input()).W();                \
    Register right = ToRegister(right_input()).W();              \
    Register out = ToRegister(result()).W();                     \
    __ opcode(out, left, right);                                 \
  }
DEF_BITWISE_BINOP(Int32BitwiseAnd, and_)
DEF_BITWISE_BINOP(Int32BitwiseOr, orr)
DEF_BITWISE_BINOP(Int32BitwiseXor, eor)
DEF_BITWISE_BINOP(Int32ShiftLeft, lslv)
DEF_BITWISE_BINOP(Int32ShiftRight, asrv)
DEF_BITWISE_BINOP(Int32ShiftRightLogical, lsrv)
#undef DEF_BITWISE_BINOP

void Int32BitwiseNot::SetValueLocationConstraints() {
  UseRegister(value_input());
  DefineAsRegister(this);
}

void Int32BitwiseNot::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  Register value = ToRegister(value_input()).W();
  Register out = ToRegister(result()).W();
  __ mvn(out, value);
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
  Register left = ToRegister(left_input()).W();
  Register right = ToRegister(right_input()).W();
  Register result = ToRegister(this->result());
  Label is_true, end;
  // TODO(leszeks): Investigate using cmov here.
  __ CompareAndBranch(left, right, ConditionFor(kOperation), &is_true);
  // TODO(leszeks): Investigate loading existing materialisations of roots here,
  // if available.
  __ LoadRoot(result, RootIndex::kFalseValue);
  __ Jump(&end);
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
  DefineAsRegister(this);
}

void Float64Add::GenerateCode(MaglevAssembler* masm,
                              const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  DoubleRegister out = ToDoubleRegister(result());
  __ Fadd(out, left, right);
}

void Float64Subtract::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

void Float64Subtract::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  DoubleRegister out = ToDoubleRegister(result());
  __ Fsub(out, left, right);
}

void Float64Multiply::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

void Float64Multiply::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  DoubleRegister out = ToDoubleRegister(result());
  __ Fmul(out, left, right);
}

void Float64Divide::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
  DefineAsRegister(this);
}

void Float64Divide::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  DoubleRegister out = ToDoubleRegister(result());
  __ Fdiv(out, left, right);
}

int Float64Modulus::MaxCallStackArgs() const { return 0; }
void Float64Modulus::SetValueLocationConstraints() {
  UseFixed(left_input(), v0);
  UseFixed(right_input(), v1);
  DefineSameAsFirst(this);
}
void Float64Modulus::GenerateCode(MaglevAssembler* masm,
                                  const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
  __ CallCFunction(ExternalReference::mod_two_doubles_operation(), 0, 2);
}

void Float64Negate::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void Float64Negate::GenerateCode(MaglevAssembler* masm,
                                 const ProcessingState& state) {
  DoubleRegister value = ToDoubleRegister(input());
  DoubleRegister out = ToDoubleRegister(result());
  __ Fneg(out, value);
}

int Float64Exponentiate::MaxCallStackArgs() const { return 0; }
void Float64Exponentiate::SetValueLocationConstraints() {
  UseFixed(left_input(), v0);
  UseFixed(right_input(), v1);
  DefineSameAsFirst(this);
}
void Float64Exponentiate::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
  __ CallCFunction(ExternalReference::ieee754_pow_function(), 2);
}

int Float64Ieee754Unary::MaxCallStackArgs() const { return 0; }
void Float64Ieee754Unary::SetValueLocationConstraints() {
  UseFixed(input(), v0);
  DefineSameAsFirst(this);
}
void Float64Ieee754Unary::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  AllowExternalCallThatCantCauseGC scope(masm);
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
  __ Fcmp(left, right);
  // Check for NaN first.
  __ JumpIf(vs, &is_false);
  __ JumpIf(NegateCondition(ConditionFor(kOperation)), &is_false);
  // TODO(leszeks): Investigate loading existing materialisations of roots here,
  // if available.
  __ LoadRoot(result, RootIndex::kTrueValue);
  __ Jump(&end);
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
  Register reg = ToRegister(input()).W();
  __ Adds(wzr, reg, reg);
  DCHECK_REGLIST_EMPTY(RegList{reg} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kNotASmi, this);
}

void CheckedSmiTagInt32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void CheckedSmiTagInt32::GenerateCode(MaglevAssembler* masm,
                                      const ProcessingState& state) {
  Register reg = ToRegister(input()).W();
  Register out = ToRegister(result()).W();
  __ Adds(out, reg, reg);
  // None of the mutated input registers should be a register input into the
  // eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{out} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  __ EmitEagerDeoptIf(vs, DeoptimizeReason::kOverflow, this);
}

void CheckedSmiTagUint32::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void CheckedSmiTagUint32::GenerateCode(MaglevAssembler* masm,
                                       const ProcessingState& state) {
  Register reg = ToRegister(input()).W();
  Register result_reg = ToRegister(result()).W();
  // Perform an unsigned comparison against Smi::kMaxValue.
  __ Cmp(reg, Immediate(Smi::kMaxValue));
  __ EmitEagerDeoptIf(hi, DeoptimizeReason::kOverflow, this);
  __ Adds(result_reg, reg, reg);
  __ Assert(vc, AbortReason::kInputDoesNotFitSmi);
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

  if (v8_flags.debug_code) {
    __ AssertNotSmi(object);
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ CompareObjectType(object, scratch, scratch, JS_TYPED_ARRAY_TYPE);
    __ Assert(eq, AbortReason::kUnexpectedValue);
  }

  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register byte_length = temps.Acquire();
  __ LoadBoundedSizeFromObject(byte_length, object,
                               JSTypedArray::kRawByteLengthOffset);
  int element_size = ElementsKindSize(elements_kind_);
  if (element_size > 1) {
    DCHECK(element_size == 2 || element_size == 4 || element_size == 8);
    __ Cmp(byte_length,
           Operand(index, LSL, base::bits::CountTrailingZeros(element_size)));
  } else {
    __ Cmp(byte_length, index);
  }
  // We use {lo} which does an unsigned comparison to handle negative
  // indices as well.
  __ EmitEagerDeoptIf(lo, DeoptimizeReason::kOutOfBounds, this);
}

int CheckJSDataViewBounds::MaxCallStackArgs() const { return 1; }
void CheckJSDataViewBounds::SetValueLocationConstraints() {
  UseRegister(receiver_input());
  UseRegister(index_input());
  set_temporaries_needed(1);
}
void CheckJSDataViewBounds::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  Register object = ToRegister(receiver_input());
  Register index = ToRegister(index_input());
  Register byte_length = scratch;
  if (v8_flags.debug_code) {
    __ AssertNotSmi(object);
    __ CompareObjectType(object, scratch, scratch, JS_DATA_VIEW_TYPE);
    __ Assert(eq, AbortReason::kUnexpectedValue);
  }

  ZoneLabelRef done_byte_length(masm);
  DeferredCodeInfo* deferred_get_byte_length = __ PushDeferredCode(
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
        __ Cmp(byte_length, Immediate(0));
        // The reason might not be OOB, but because array was detached.
        // Unfortunately we can only add one reason type in Maglev.
        __ EmitEagerDeoptIf(lt, DeoptimizeReason::kOutOfBounds, node);
        __ B(*done);
      },
      this, done_byte_length, object, index, byte_length);
  __ Ldr(scratch.W(), FieldMemOperand(object, JSDataView::kBitFieldOffset));
  __ Cbnz(scratch.W(), &deferred_get_byte_length->deferred_code_label);

  // Normal DataView (backed by AB / SAB) or non-length tracking backed by GSAB.
  __ LoadBoundedSizeFromObject(byte_length, object,
                               JSDataView::kRawByteLengthOffset);
  __ bind(*done_byte_length);

  int element_size = ExternalArrayElementSize(element_type_);
  if (element_size > 1) {
    __ Cmp(byte_length, Immediate(element_size - 1));
    __ EmitEagerDeoptIf(mi, DeoptimizeReason::kOutOfBounds, this);
  }
  __ Cmp(index, byte_length);
  __ EmitEagerDeoptIf(hs, DeoptimizeReason::kOutOfBounds, this);
}

void CheckedInternalizedString::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineSameAsFirst(this);
  set_temporaries_needed(1);
}
void CheckedInternalizedString::GenerateCode(MaglevAssembler* masm,
                                             const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  Register object = ToRegister(object_input());

  if (check_type_ == CheckType::kOmitHeapObjectCheck) {
    __ AssertNotSmi(object);
  } else {
    Condition is_smi = __ CheckSmi(object);
    __ EmitEagerDeoptIf(is_smi, DeoptimizeReason::kWrongMap, this);
  }

  __ LoadMap(scratch, object);
  __ RecordComment("Test IsInternalizedString");
  // Go to the slow path if this is a non-string, or a non-internalised string.
  __ Ldrh(scratch.W(), FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  __ Tst(scratch.W(), Immediate(kIsNotStringMask | kIsNotInternalizedMask));
  static_assert((kStringTag | kInternalizedTag) == 0);
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(
      ne,
      [](MaglevAssembler* masm, ZoneLabelRef done, Register object,
         CheckedInternalizedString* node, EagerDeoptInfo* deopt_info,
         Register instance_type) {
        __ RecordComment("Deferred Test IsThinString");
        static_assert(kThinStringTagBit > 0);
        // Deopt if this isn't a string.
        __ Tst(instance_type.W(), Immediate(kIsNotStringMask));
        __ EmitEagerDeoptIf(ne, DeoptimizeReason::kWrongMap, node);
        // Deopt if this isn't a thin string.
        __ Tst(instance_type.W(), Immediate(kThinStringTagBit));
        __ EmitEagerDeoptIf(eq, DeoptimizeReason::kWrongMap, node);
        __ LoadTaggedPointerField(
            object, FieldMemOperand(object, ThinString::kActualOffset));
        if (v8_flags.debug_code) {
          __ RecordComment("DCHECK IsInternalizedString");
          Register scratch = instance_type;
          __ LoadMap(scratch, object);
          __ Ldrh(scratch.W(),
                  FieldMemOperand(scratch, Map::kInstanceTypeOffset));
          __ Tst(scratch.W(),
                 Immediate(kIsNotStringMask | kIsNotInternalizedMask));
          static_assert((kStringTag | kInternalizedTag) == 0);
          __ Check(eq, AbortReason::kUnexpectedValue);
        }
        __ jmp(*done);
      },
      done, object, this, eager_deopt_info(), scratch);
  __ bind(*done);
}

void UnsafeSmiTag::SetValueLocationConstraints() {
  UseRegister(input());
  DefineAsRegister(this);
}
void UnsafeSmiTag::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  Register reg = ToRegister(input()).W();
  Register out = ToRegister(result()).W();
  if (v8_flags.debug_code) {
    if (input().node()->properties().value_representation() ==
        ValueRepresentation::kUint32) {
      __ Cmp(reg, Immediate(Smi::kMaxValue));
      __ Check(ls, AbortReason::kInputDoesNotFitSmi);
    }
  }
  __ Adds(out, reg, reg);
  if (v8_flags.debug_code) {
    __ Check(vc, AbortReason::kInputDoesNotFitSmi);
  }
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
  __ JumpIfNotSmi(value, &is_not_smi);
  // If Smi, convert to Float64.
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register temp = temps.Acquire();
  __ SmiToInt32(temp, value);
  __ sxtw(temp, temp.W());
  __ scvtf(ToDoubleRegister(result()), temp);
  __ Jump(&done);
  __ bind(&is_not_smi);
  // Check if HeapNumber, deopt otherwise.
  __ Move(temp, FieldMemOperand(value, HeapObject::kMapOffset));
  __ CompareRoot(temp, RootIndex::kHeapNumberMap);
  __ EmitEagerDeoptIf(ne, DeoptimizeReason::kNotANumber, this);
  __ Move(temp, FieldMemOperand(value, HeapNumber::kValueOffset));
  __ fmov(ToDoubleRegister(result()), temp);
  __ bind(&done);
}

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
      array, FieldMemOperand(generator,
                             JSGeneratorObject::kParametersAndRegistersOffset));

  for (int i = 0; i < num_parameters_and_registers(); i++) {
    // Use WriteBarrierDescriptor::SlotAddressRegister() as the scratch
    // register since it's a known temporary, and the write barrier slow path
    // generates better code when value == scratch.
    Register value =
        __ FromAnyToRegister(parameters_and_registers(i),
                             WriteBarrierDescriptor::SlotAddressRegister());

    ZoneLabelRef done(masm);
    DeferredCodeInfo* deferred_write_barrier = __ PushDeferredCode(
        [](MaglevAssembler* masm, ZoneLabelRef done, Register value,
           Register array, GeneratorStore* node, int32_t offset) {
          ASM_CODE_COMMENT_STRING(masm, "Write barrier slow path");
          __ CheckPageFlag(
              value,
              MemoryChunk::kPointersToHereAreInterestingOrInSharedHeapMask, eq,
              *done);

          Register slot_reg = WriteBarrierDescriptor::SlotAddressRegister();
          __ Add(slot_reg, array, offset - kHeapObjectTag);

          // TODO(leszeks): Add an interface for flushing all double registers
          // before this Node, to avoid needing to save them here.
          SaveFPRegsMode const save_fp_mode =
              !node->register_snapshot().live_double_registers.is_empty()
                  ? SaveFPRegsMode::kSave
                  : SaveFPRegsMode::kIgnore;

          __ CallRecordWriteStub(array, slot_reg, save_fp_mode);

          __ B(*done);
        },
        done, value, array, this, FixedArray::OffsetOfElementAt(i));

    __ StoreTaggedField(
        value, FieldMemOperand(array, FixedArray::OffsetOfElementAt(i)));
    __ JumpIfSmi(value, *done);
    // TODO(leszeks): This will stay either false or true throughout this loop.
    // Consider hoisting the check out of the loop and duplicating the loop into
    // with and without write barrier.
    __ CheckPageFlag(array, MemoryChunk::kPointersFromHereAreInterestingMask,
                     ne, &deferred_write_barrier->deferred_code_label);

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
        // TODO(leszeks): The context is almost always going to be in
        // old-space, consider moving this check to the fast path, maybe even
        // as the first bailout.
        __ CheckPageFlag(
            context,
            MemoryChunk::kPointersToHereAreInterestingOrInSharedHeapMask, eq,
            *done);

        __ Move(WriteBarrierDescriptor::ObjectRegister(), generator);
        generator = WriteBarrierDescriptor::ObjectRegister();

        Register slot_reg = WriteBarrierDescriptor::SlotAddressRegister();
        __ Add(slot_reg, generator,
               JSGeneratorObject::kContextOffset - kHeapObjectTag);

        // TODO(leszeks): Add an interface for flushing all double registers
        // before this Node, to avoid needing to save them here.
        SaveFPRegsMode const save_fp_mode =
            !node->register_snapshot().live_double_registers.is_empty()
                ? SaveFPRegsMode::kSave
                : SaveFPRegsMode::kIgnore;

        __ CallRecordWriteStub(generator, slot_reg, save_fp_mode);

        __ B(*done);
      },
      done, context, generator, this);
  __ StoreTaggedField(
      context, FieldMemOperand(generator, JSGeneratorObject::kContextOffset));
  __ AssertNotSmi(context);
  __ CheckPageFlag(generator, MemoryChunk::kPointersFromHereAreInterestingMask,
                   ne, &deferred_context_write_barrier->deferred_code_label);
  __ bind(*done);

  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  __ Mov(scratch, Smi::FromInt(suspend_id()));
  __ StoreTaggedField(
      scratch,
      FieldMemOperand(generator, JSGeneratorObject::kContinuationOffset));
  __ Mov(scratch, Smi::FromInt(bytecode_offset()));
  __ StoreTaggedField(
      scratch,
      FieldMemOperand(generator, JSGeneratorObject::kInputOrDebugPosOffset));
}

void IncreaseInterruptBudget::SetValueLocationConstraints() {
  set_temporaries_needed(1);
}
void IncreaseInterruptBudget::GenerateCode(MaglevAssembler* masm,
                                           const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register feedback_cell = temps.Acquire();
  Register budget = temps.Acquire().W();
  __ Ldr(feedback_cell,
         MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ LoadTaggedPointerField(
      feedback_cell,
      FieldMemOperand(feedback_cell, JSFunction::kFeedbackCellOffset));
  __ Ldr(budget,
         FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
  __ Add(budget, budget, Immediate(amount()));
  __ Str(budget,
         FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
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
    {
      Register stack_limit = scratch0;
      __ LoadStackLimit(stack_limit, StackLimitKind::kInterruptStackLimit);
      __ Cmp(sp, stack_limit);
      __ B(&next, hi);
    }

    // An interrupt has been requested and we must call into runtime to handle
    // it; since we already pay the call cost, combine with the TieringManager
    // call.
    {
      SaveRegisterStateForCall save_register_state(masm,
                                                   node->register_snapshot());
      Register function = scratch0;
      __ Ldr(function, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
      __ Push(function);
      // Move into kContextRegister after the load into scratch0, just in case
      // scratch0 happens to be kContextRegister.
      __ Move(kContextRegister, masm->native_context().object());
      __ CallRuntime(Runtime::kBytecodeBudgetInterruptWithStackCheck_Maglev, 1);
      save_register_state.DefineSafepointWithLazyDeopt(node->lazy_deopt_info());
    }
    __ B(*done);  // All done, continue.
    __ Bind(&next);
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
      MaglevAssembler::ScratchRegisterScope temps(masm);
      Register scratch1 = temps.Acquire();
      static_assert(kTieringStateInProgressBlocksTierup);
      __ Move(scratch0, masm->compilation_info()
                            ->toplevel_compilation_unit()
                            ->feedback()
                            .object());

      // If tiering_state is kInProgress, skip the runtime call.
      __ Ldrh(scratch1.W(),
              FieldMemOperand(scratch0, FeedbackVector::kFlagsOffset));
      __ DecodeField<FeedbackVector::TieringStateBits>(scratch1);
      __ Cmp(scratch1.W(),
             Immediate(static_cast<int>(TieringState::kInProgress)));
      __ B(&update_profiler_ticks_and_interrupt_budget, eq);

      // If osr_tiering_state is kInProgress, skip the runtime call.
      __ Ldrh(scratch1.W(),
              FieldMemOperand(scratch0, FeedbackVector::kFlagsOffset));
      __ DecodeField<FeedbackVector::OsrTieringStateBit>(scratch1);
      __ Cmp(scratch1.W(),
             Immediate(static_cast<int>(TieringState::kInProgress)));
      __ B(&update_profiler_ticks_and_interrupt_budget, eq);
    }

    {
      SaveRegisterStateForCall save_register_state(masm,
                                                   node->register_snapshot());
      Register function = scratch0;
      __ Ldr(function, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
      __ Push(function);
      // Move into kContextRegister after the load into scratch0, just in case
      // scratch0 happens to be kContextRegister.
      __ Move(kContextRegister, masm->native_context().object());
      // Note: must not cause a lazy deopt!
      __ CallRuntime(Runtime::kBytecodeBudgetInterrupt_Maglev, 1);
      save_register_state.DefineSafepoint();
    }
    __ B(*done);

    __ Bind(&update_profiler_ticks_and_interrupt_budget);
    {
      MaglevAssembler::ScratchRegisterScope temps(masm);
      Register feedback_vector = scratch0;
      Register ticks = temps.Acquire().W();
      // We are skipping the call to Runtime::kBytecodeBudgetInterrupt_Maglev
      // since the tiering state is kInProgress. Perform bookkeeping that would
      // have been done in the runtime function:
      __ AssertFeedbackVector(feedback_vector);
      // FeedbackVector::SaturatingIncrementProfilerTicks.
      // TODO(jgruber): This isn't saturating and thus we may theoretically
      // exceed Smi::kMaxValue. But, 1) this is very unlikely since it'd take
      // quite some time to exhaust the budget that many times; and 2) even an
      // overflow doesn't hurt us at all.
      __ Ldr(ticks, FieldMemOperand(feedback_vector,
                                    FeedbackVector::kProfilerTicksOffset));
      __ Add(ticks, ticks, Immediate(1));
      __ Str(ticks, FieldMemOperand(feedback_vector,
                                    FeedbackVector::kProfilerTicksOffset));
    }

    // JSFunction::SetInterruptBudget.
    {
      MaglevAssembler::ScratchRegisterScope temps(masm);
      Register feedback_cell = scratch0;
      Register budget = temps.Acquire().W();
      __ Ldr(feedback_cell,
             MemOperand(fp, StandardFrameConstants::kFunctionOffset));
      __ LoadTaggedPointerField(
          feedback_cell,
          FieldMemOperand(feedback_cell, JSFunction::kFeedbackCellOffset));
      __ Move(budget, v8_flags.interrupt_budget);
      __ Str(budget, FieldMemOperand(feedback_cell,
                                     FeedbackCell::kInterruptBudgetOffset));
    }
    __ B(*done);
  }
}

}  // namespace

int ReduceInterruptBudget::MaxCallStackArgs() const { return 1; }
void ReduceInterruptBudget::SetValueLocationConstraints() {
  set_temporaries_needed(2);
}
void ReduceInterruptBudget::GenerateCode(MaglevAssembler* masm,
                                         const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  Register feedback_cell = scratch;
  Register budget = temps.Acquire().W();
  __ Ldr(feedback_cell,
         MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ LoadTaggedPointerField(
      feedback_cell,
      FieldMemOperand(feedback_cell, JSFunction::kFeedbackCellOffset));
  __ Ldr(budget,
         FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
  __ Subs(budget, budget, Immediate(amount()));
  __ Str(budget,
         FieldMemOperand(feedback_cell, FeedbackCell::kInterruptBudgetOffset));
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(lt, HandleInterruptsAndTiering, done, this, scratch);
  __ bind(*done);
}

namespace {

template <bool check_detached, typename ResultReg, typename NodeT>
void GenerateTypedArrayLoad(MaglevAssembler* masm, NodeT* node, Register object,
                            Register index, ResultReg result_reg,
                            ElementsKind kind) {
  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ CompareObjectType(object, scratch, scratch, JS_TYPED_ARRAY_TYPE);
    __ Assert(eq, AbortReason::kUnexpectedValue);
  }

  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();

  if constexpr (check_detached) {
    __ DeoptIfBufferDetached(object, scratch, node);
  }

  Register data_pointer = scratch;
  __ BuildTypedArrayDataPointer(data_pointer, object);

  if constexpr (std::is_same_v<ResultReg, Register>) {
    if (IsSignedIntTypedArrayElementsKind(kind)) {
      int element_size = ElementsKindSize(kind);
      __ Add(data_pointer, data_pointer, Operand(index, LSL, element_size / 2));
      __ LoadSignedField(result_reg.W(), MemOperand(data_pointer),
                         element_size);
    } else {
      DCHECK(IsUnsignedIntTypedArrayElementsKind(kind));
      int element_size = ElementsKindSize(kind);
      __ Add(data_pointer, data_pointer, Operand(index, LSL, element_size / 2));
      __ LoadUnsignedField(result_reg.W(), MemOperand(data_pointer),
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
        __ Add(data_pointer, data_pointer, Operand(index, LSL, 2));
        __ Ldr(result_reg.S(), MemOperand(data_pointer));
        __ Fcvt(result_reg, result_reg.S());
        break;
      case FLOAT64_ELEMENTS:
        __ Add(data_pointer, data_pointer, Operand(index, LSL, 3));
        __ Ldr(result_reg, MemOperand(data_pointer));
        break;
      default:
        UNREACHABLE();
    }
  }
}

}  // namespace

#define DEF_OPERATION(Name, ResultReg, ToResultReg, check_detached)     \
  void Name::SetValueLocationConstraints() {                            \
    UseRegister(object_input());                                        \
    UseRegister(index_input());                                         \
    DefineAsRegister(this);                                             \
  }                                                                     \
  void Name::GenerateCode(MaglevAssembler* masm,                        \
                          const ProcessingState& state) {               \
    Register object = ToRegister(object_input());                       \
    Register index = ToRegister(index_input());                         \
    ResultReg result_reg = ToResultReg(result());                       \
                                                                        \
    GenerateTypedArrayLoad<check_detached>(masm, this, object, index,   \
                                           result_reg, elements_kind_); \
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

void LoadDoubleField::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineAsRegister(this);
}
void LoadDoubleField::GenerateCode(MaglevAssembler* masm,
                                   const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register tmp = temps.Acquire();
  Register object = ToRegister(object_input());
  __ AssertNotSmi(object);
  __ DecompressAnyTagged(tmp, FieldMemOperand(object, offset()));
  __ AssertNotSmi(tmp);
  __ Ldr(ToDoubleRegister(result()),
         FieldMemOperand(tmp, HeapNumber::kValueOffset));
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
  if (v8_flags.debug_code) {
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ AssertNotSmi(elements);
    __ CompareObjectType(elements, scratch, scratch, FIXED_ARRAY_TYPE);
    __ Assert(eq, AbortReason::kUnexpectedValue);
  }
  Register result_reg = ToRegister(result());
  __ Add(result_reg, elements, Operand(index, LSL, kTaggedSizeLog2));
  __ DecompressAnyTagged(result_reg,
                         FieldMemOperand(result_reg, FixedArray::kHeaderSize));
}

void LoadFixedDoubleArrayElement::SetValueLocationConstraints() {
  UseAndClobberRegister(elements_input());
  UseRegister(index_input());
  DefineAsRegister(this);
}
void LoadFixedDoubleArrayElement::GenerateCode(MaglevAssembler* masm,
                                               const ProcessingState& state) {
  Register elements = ToRegister(elements_input());
  Register index = ToRegister(index_input());
  if (v8_flags.debug_code) {
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ AssertNotSmi(elements);
    __ CompareObjectType(elements, scratch, scratch, FIXED_DOUBLE_ARRAY_TYPE);
    __ Assert(eq, AbortReason::kUnexpectedValue);
  }
  __ Add(elements, elements, Operand(index, LSL, kDoubleSizeLog2));
  __ Ldr(ToDoubleRegister(result()),
         FieldMemOperand(elements, FixedArray::kHeaderSize));
}

void StoreDoubleField::SetValueLocationConstraints() {
  UseRegister(object_input());
  UseRegister(value_input());
}
void StoreDoubleField::GenerateCode(MaglevAssembler* masm,
                                    const ProcessingState& state) {
  Register object = ToRegister(object_input());
  DoubleRegister value = ToDoubleRegister(value_input());

  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register tmp = temps.Acquire();

  __ AssertNotSmi(object);
  __ DecompressAnyTagged(tmp, FieldMemOperand(object, offset()));
  __ AssertNotSmi(tmp);
  __ Move(FieldMemOperand(tmp, HeapNumber::kValueOffset), value);
}

int StoreMap::MaxCallStackArgs() const {
  return WriteBarrierDescriptor::GetStackParameterCount();
}
void StoreMap::SetValueLocationConstraints() {
  UseFixed(object_input(), WriteBarrierDescriptor::ObjectRegister());
  set_temporaries_needed(1);
}
void StoreMap::GenerateCode(MaglevAssembler* masm,
                            const ProcessingState& state) {
  // TODO(leszeks): Consider making this an arbitrary register and push/popping
  // in the deferred path.
  Register object = WriteBarrierDescriptor::ObjectRegister();
  DCHECK_EQ(object, ToRegister(object_input()));

  __ AssertNotSmi(object);
  // Since {value} will be passed to deferred code, we have to use a general
  // temporary for it, rather than the regular scratch registers.
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register value = temps.Acquire();
  __ Move(value, map_.object());
  __ StoreTaggedField(value, FieldMemOperand(object, HeapObject::kMapOffset));

  ZoneLabelRef done(masm);
  DeferredCodeInfo* deferred_write_barrier = __ PushDeferredCode(
      [](MaglevAssembler* masm, ZoneLabelRef done, Register value,
         Register object, StoreMap* node) {
        ASM_CODE_COMMENT_STRING(masm, "Write barrier slow path");
        __ CheckPageFlag(
            value, MemoryChunk::kPointersToHereAreInterestingOrInSharedHeapMask,
            eq, *done);

        Register slot_reg = WriteBarrierDescriptor::SlotAddressRegister();
        RegList saved;
        if (node->register_snapshot().live_registers.has(slot_reg)) {
          saved.set(slot_reg);
        }

        __ PushAll(saved);
        __ Add(slot_reg, object, HeapObject::kMapOffset - kHeapObjectTag);

        SaveFPRegsMode const save_fp_mode =
            !node->register_snapshot().live_double_registers.is_empty()
                ? SaveFPRegsMode::kSave
                : SaveFPRegsMode::kIgnore;

        __ CallRecordWriteStub(object, slot_reg, save_fp_mode);

        __ PopAll(saved);
        __ B(*done);
      },
      done, value, object, this);

  __ JumpIfSmi(value, *done);
  __ CheckPageFlag(object, MemoryChunk::kPointersFromHereAreInterestingMask, ne,
                   &deferred_write_barrier->deferred_code_label);
  __ bind(*done);
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
  DefineAsRegister(this);
}
void LoadSignedIntDataViewElement::GenerateCode(MaglevAssembler* masm,
                                                const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  Register result_reg = ToRegister(result());

  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ CompareObjectType(object, scratch, scratch, JS_DATA_VIEW_TYPE);
    __ Assert(hs, AbortReason::kUnexpectedValue);
  }

  int element_size = ExternalArrayElementSize(type_);

  // Load data pointer.
  {
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register data_pointer = temps.Acquire();
    __ LoadExternalPointerField(
        data_pointer, FieldMemOperand(object, JSDataView::kDataPointerOffset));

    __ LoadSignedField(result_reg.W(), MemOperand(data_pointer, index),
                       element_size);
  }

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
      // arm64 is little endian.
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
}
void StoreSignedIntDataViewElement::GenerateCode(MaglevAssembler* masm,
                                                 const ProcessingState& state) {
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  Register value = ToRegister(value_input());

  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ CompareObjectType(object, scratch, scratch, JS_DATA_VIEW_TYPE);
    __ Assert(hs, AbortReason::kUnexpectedValue);
  }

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
      // arm64 is little endian.
      static_assert(V8_TARGET_LITTLE_ENDIAN == 1);
    }
  }

  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register data_pointer = temps.Acquire();
  __ LoadExternalPointerField(
      data_pointer, FieldMemOperand(object, JSDataView::kDataPointerOffset));
  __ StoreField(MemOperand(data_pointer, index), value.W(), element_size);
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
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  DoubleRegister result_reg = ToDoubleRegister(result());
  Register data_pointer = temps.Acquire();

  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ CompareObjectType(object, scratch, scratch, JS_DATA_VIEW_TYPE);
    __ Assert(hs, AbortReason::kUnexpectedValue);
  }

  // Load data pointer.
  __ LoadExternalPointerField(
      data_pointer, FieldMemOperand(object, JSDataView::kDataPointerOffset));

  if (is_little_endian_constant()) {
    if (FromConstantToBool(masm, is_little_endian_input().node())) {
      __ Move(result_reg, MemOperand(data_pointer, index));
    } else {
      MaglevAssembler::ScratchRegisterScope temps(masm);
      Register scratch = temps.Acquire();
      __ Move(scratch, MemOperand(data_pointer, index));
      __ Rev(scratch, scratch);
      __ Fmov(result_reg, scratch);
    }
  } else {
    Label done;
    ZoneLabelRef is_little_endian(masm), is_big_endian(masm);
    // TODO(leszeks): We're likely to be calling this on an existing boolean --
    // maybe that's a case we should fast-path here and re-use that boolean
    // value?
    __ ToBoolean(ToRegister(is_little_endian_input()), is_little_endian,
                 is_big_endian, true);
    // arm64 is little endian.
    static_assert(V8_TARGET_LITTLE_ENDIAN == 1);
    __ bind(*is_little_endian);
    __ Move(result_reg, MemOperand(data_pointer, index));
    __ jmp(&done);
    // We should swap the bytes if big endian.
    __ bind(*is_big_endian);
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ Move(scratch, MemOperand(data_pointer, index));
    __ Rev(scratch, scratch);
    __ Fmov(result_reg, scratch);
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
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register object = ToRegister(object_input());
  Register index = ToRegister(index_input());
  DoubleRegister value = ToDoubleRegister(value_input());
  Register data_pointer = temps.Acquire();

  __ AssertNotSmi(object);
  if (v8_flags.debug_code) {
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ CompareObjectType(object, scratch, scratch, JS_DATA_VIEW_TYPE);
    __ Assert(hs, AbortReason::kUnexpectedValue);
  }

  // Load data pointer.
  __ LoadExternalPointerField(
      data_pointer, FieldMemOperand(object, JSDataView::kDataPointerOffset));

  if (is_little_endian_constant()) {
    if (FromConstantToBool(masm, is_little_endian_input().node())) {
      __ Str(value, MemOperand(data_pointer, index));
    } else {
      MaglevAssembler::ScratchRegisterScope temps(masm);
      Register scratch = temps.Acquire();
      __ Fmov(scratch, value);
      __ Rev(scratch, scratch);
      __ Str(scratch, MemOperand(data_pointer, index));
    }
  } else {
    Label done;
    ZoneLabelRef is_little_endian(masm), is_big_endian(masm);
    // TODO(leszeks): We're likely to be calling this on an existing boolean --
    // maybe that's a case we should fast-path here and re-use that boolean
    // value?
    __ ToBoolean(ToRegister(is_little_endian_input()), is_little_endian,
                 is_big_endian, true);
    // arm64 is little endian.
    static_assert(V8_TARGET_LITTLE_ENDIAN == 1);
    __ bind(*is_little_endian);
    __ Str(value, MemOperand(data_pointer, index));
    __ jmp(&done);
    // We should swap the bytes if big endian.
    __ bind(*is_big_endian);
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ Fmov(scratch, value);
    __ Rev(scratch, scratch);
    __ Str(scratch, MemOperand(data_pointer, index));
    __ bind(&done);
  }
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
  __ StoreTaggedField(FieldMemOperand(object, offset()), value);

  ZoneLabelRef done(masm);
  DeferredCodeInfo* deferred_write_barrier = __ PushDeferredCode(
      [](MaglevAssembler* masm, ZoneLabelRef done, Register value,
         Register object, StoreTaggedFieldWithWriteBarrier* node) {
        ASM_CODE_COMMENT_STRING(masm, "Write barrier slow path");
        __ CheckPageFlag(
            value, MemoryChunk::kPointersToHereAreInterestingOrInSharedHeapMask,
            eq, *done);

        Register slot_reg = WriteBarrierDescriptor::SlotAddressRegister();
        RegList saved;
        if (node->register_snapshot().live_registers.has(slot_reg)) {
          saved.set(slot_reg);
        }

        __ PushAll(saved);
        __ Add(slot_reg, object, node->offset() - kHeapObjectTag);

        SaveFPRegsMode const save_fp_mode =
            !node->register_snapshot().live_double_registers.is_empty()
                ? SaveFPRegsMode::kSave
                : SaveFPRegsMode::kIgnore;

        __ CallRecordWriteStub(object, slot_reg, save_fp_mode);

        __ PopAll(saved);
        __ B(*done);
      },
      done, value, object, this);

  __ JumpIfSmi(value, *done);
  __ CheckPageFlag(object, MemoryChunk::kPointersFromHereAreInterestingMask, ne,
                   &deferred_write_barrier->deferred_code_label);

  __ bind(*done);
}

void SetPendingMessage::SetValueLocationConstraints() {
  UseRegister(value());
  DefineAsRegister(this);
}

void SetPendingMessage::GenerateCode(MaglevAssembler* masm,
                                     const ProcessingState& state) {
  Register new_message = ToRegister(value());
  Register return_value = ToRegister(result());

  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch0 = temps.Acquire();
  MemOperand pending_message_operand = __ ExternalReferenceAsOperand(
      ExternalReference::address_of_pending_message(masm->isolate()), scratch0);

  if (new_message != return_value) {
    __ Ldr(return_value, pending_message_operand);
    __ Str(new_message, pending_message_operand);
  } else {
    Register scratch1 = temps.Acquire();
    __ Ldr(scratch1, pending_message_operand);
    __ Str(new_message, pending_message_operand);
    __ Move(return_value, scratch1);
  }
}

void StringLength::SetValueLocationConstraints() {
  UseRegister(object_input());
  DefineAsRegister(this);
}
void StringLength::GenerateCode(MaglevAssembler* masm,
                                const ProcessingState& state) {
  Register object = ToRegister(object_input());
  if (v8_flags.debug_code) {
    // Check if {object} is a string.
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ AssertNotSmi(object);
    __ LoadMap(scratch, object);
    __ CompareInstanceTypeRange(scratch, scratch, FIRST_STRING_TYPE,
                                LAST_STRING_TYPE);
    __ Check(ls, AbortReason::kUnexpectedValue);
  }
  __ Ldr(ToRegister(result()).W(),
         FieldMemOperand(object, String::kLengthOffset));
}

void TestUndetectable::SetValueLocationConstraints() {
  UseRegister(value());
  DefineAsRegister(this);
}
void TestUndetectable::GenerateCode(MaglevAssembler* masm,
                                    const ProcessingState& state) {
  Register object = ToRegister(value());
  Register return_value = ToRegister(result());

  Label return_false, done;
  __ JumpIfSmi(object, &return_false);
  {
    // For heap objects, check the map's undetectable bit.
    MaglevAssembler::ScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ LoadMap(scratch, object);
    __ Ldr(scratch.W(), FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ TestAndBranchIfAllClear(
        scratch.W(), Map::Bits1::IsUndetectableBit::kMask, &return_false);
  }

  __ LoadRoot(return_value, RootIndex::kTrueValue);
  __ B(&done);

  __ bind(&return_false);
  __ LoadRoot(return_value, RootIndex::kFalseValue);

  __ bind(&done);
}

int ThrowIfNotSuperConstructor::MaxCallStackArgs() const { return 2; }
void ThrowIfNotSuperConstructor::SetValueLocationConstraints() {
  UseRegister(constructor());
  UseRegister(function());
}
void ThrowIfNotSuperConstructor::GenerateCode(MaglevAssembler* masm,
                                              const ProcessingState& state) {
  DeferredCodeInfo* deferred_abort = __ PushDeferredCode(
      [](MaglevAssembler* masm, ThrowIfNotSuperConstructor* node) {
        __ Push(ToRegister(node->constructor()), ToRegister(node->function()));
        __ Move(kContextRegister, masm->native_context().object());
        __ CallRuntime(Runtime::kThrowNotSuperConstructor, 2);
        masm->DefineExceptionHandlerAndLazyDeoptPoint(node);
        __ Abort(AbortReason::kUnexpectedReturnFromThrow);
      },
      this);
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  __ LoadMap(scratch, ToRegister(constructor()));
  __ Ldr(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
  __ TestAndBranchIfAllClear(scratch, Map::Bits1::IsConstructorBit::kMask,
                             &deferred_abort->deferred_code_label);
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
  // We cannot use scratch registers, since they're used in LeaveFrame and
  // DropArguments.
  Register actual_params_size = x9;
  Register params_size = x10;

  // Compute the size of the actual parameters + receiver (in bytes).
  // TODO(leszeks): Consider making this an input into Return to re-use the
  // incoming argc's register (if it's still valid).
  __ Ldr(actual_params_size,
         MemOperand(fp, StandardFrameConstants::kArgCOffset));
  __ Mov(params_size, Immediate(formal_params_size));

  // If actual is bigger than formal, then we should use it to free up the stack
  // arguments.
  Label corrected_args_count;
  __ CompareAndBranch(params_size, actual_params_size, ge,
                      &corrected_args_count);
  __ Mov(params_size, actual_params_size);
  __ bind(&corrected_args_count);

  // Leave the frame.
  __ LeaveFrame(StackFrame::MAGLEV);

  // Drop receiver + arguments according to dynamic arguments size.
  __ DropArguments(params_size, TurboAssembler::kCountIncludesReceiver);
  __ Ret();
}

void BranchIfJSReceiver::SetValueLocationConstraints() {
  UseRegister(condition_input());
}
void BranchIfJSReceiver::GenerateCode(MaglevAssembler* masm,
                                      const ProcessingState& state) {
  MaglevAssembler::ScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  Register value = ToRegister(condition_input());
  __ JumpIfSmi(value, if_false()->label());
  __ LoadMap(scratch, value);
  __ CompareInstanceType(scratch, scratch, FIRST_JS_RECEIVER_TYPE);
  __ Branch(hs, if_true(), if_false(), state.next_block());
}

void BranchIfFloat64Compare::SetValueLocationConstraints() {
  UseRegister(left_input());
  UseRegister(right_input());
}
void BranchIfFloat64Compare::GenerateCode(MaglevAssembler* masm,
                                          const ProcessingState& state) {
  DoubleRegister left = ToDoubleRegister(left_input());
  DoubleRegister right = ToDoubleRegister(right_input());
  __ Fcmp(left, right);
  __ JumpIf(vs, if_false()->label());  // NaN check
  __ Branch(ConditionFor(operation_), if_true(), if_false(),
            state.next_block());
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
