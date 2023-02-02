// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-load-elimination.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/wasm/struct-types.h"
#include "src/wasm/wasm-subtyping.h"

namespace v8::internal::compiler {

/**** Helpers ****/

namespace {
bool TypesUnrelated(Node* lhs, Node* rhs) {
  wasm::TypeInModule type1 = NodeProperties::GetType(lhs).AsWasm();
  wasm::TypeInModule type2 = NodeProperties::GetType(rhs).AsWasm();
  return wasm::TypesUnrelated(type1.type, type2.type, type1.module,
                              type2.module);
}

bool IsFresh(Node* node) {
  return node->opcode() == IrOpcode::kAllocate ||
         node->opcode() == IrOpcode::kAllocateRaw;
}

bool IsConstant(Node* node) {
  return node->opcode() == IrOpcode::kParameter ||
         node->opcode() == IrOpcode::kHeapConstant;
}

bool MayAlias(Node* lhs, Node* rhs) {
  if (lhs == rhs) return true;
  if (TypesUnrelated(lhs, rhs) || (IsFresh(lhs) && IsFresh(rhs)) ||
      (IsFresh(lhs) && IsConstant(rhs)) || (IsConstant(lhs) && IsFresh(rhs))) {
    return false;
  }
  return true;
}

Node* ResolveAliases(Node* node) {
  while (node->opcode() == IrOpcode::kWasmTypeCast ||
         node->opcode() == IrOpcode::kAssertNotNull ||
         node->opcode() == IrOpcode::kTypeGuard) {
    node = NodeProperties::GetValueInput(node, 0);
  }
  return node;
}

// We model array length as a field at index kArrayLengthFieldIndex.
constexpr int kArrayLengthFieldIndex = -1;
}  // namespace

Reduction WasmLoadElimination::UpdateState(Node* node,
                                           AbstractState const* state) {
  AbstractState const* original = node_states_.Get(node);
  // Only signal that the {node} has Changed, if the information about {state}
  // has changed wrt. the {original}.
  if (state != original) {
    if (original == nullptr || !state->Equals(original)) {
      node_states_.Set(node, state);
      return Changed(node);
    }
  }
  return NoChange();
}

std::tuple<Node*, Node*> WasmLoadElimination::TruncateAndExtendOrType(
    Node* value, Node* effect, Node* control, wasm::ValueType field_type,
    bool is_signed) {
  if (field_type == wasm::kWasmI8 || field_type == wasm::kWasmI16) {
    Node* ret = nullptr;
    if (is_signed) {
      int shift = 32 - 8 * field_type.value_kind_size();
      ret = graph()->NewNode(machine()->Word32Sar(),
                             graph()->NewNode(machine()->Word32Shl(), value,
                                              jsgraph()->Int32Constant(shift)),
                             jsgraph()->Int32Constant(shift));
    } else {
      int mask = (1 << 8 * field_type.value_kind_size()) - 1;
      ret = graph()->NewNode(machine()->Word32And(), value,
                             jsgraph()->Int32Constant(mask));
    }

    NodeProperties::SetType(ret, NodeProperties::GetType(value));
    return {ret, effect};
  }

  wasm::TypeInModule node_type = NodeProperties::GetType(value).AsWasm();

  // TODO(12166): Adapt this if cross-module inlining is allowed.
  if (!wasm::IsSubtypeOf(node_type.type, field_type, node_type.module)) {
    Type type = Type::Wasm({field_type, node_type.module}, graph()->zone());
    Node* ret =
        graph()->NewNode(common()->TypeGuard(type), value, effect, control);
    NodeProperties::SetType(ret, type);
    return {ret, ret};
  }

  return {value, effect};
}

/***** Reductions *****/

Reduction WasmLoadElimination::Reduce(Node* node) {
  if (v8_flags.trace_turbo_load_elimination) {
    // TODO(manoskouk): Add some tracing.
  }
  switch (node->opcode()) {
    case IrOpcode::kWasmStructGet:
      return ReduceWasmStructGet(node);
    case IrOpcode::kWasmStructSet:
      return ReduceWasmStructSet(node);
    case IrOpcode::kWasmArrayLength:
      return ReduceWasmArrayLength(node);
    case IrOpcode::kWasmArrayInitializeLength:
      return ReduceWasmArrayInitializeLength(node);
    case IrOpcode::kEffectPhi:
      return ReduceEffectPhi(node);
    case IrOpcode::kDead:
      return NoChange();
    case IrOpcode::kStart:
      return ReduceStart(node);
    default:
      return ReduceOtherNode(node);
  }
}

Reduction WasmLoadElimination::ReduceWasmStructGet(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmStructGet);
  Node* object = ResolveAliases(NodeProperties::GetValueInput(node, 0));
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();

  const WasmFieldInfo& field_info = OpParameter<WasmFieldInfo>(node->op());
  bool is_mutable = field_info.type->mutability(field_info.field_index);

  // - The node can only be typed as bottom in unreachable code.
  // - We can only find the field in the wrong half-state in unreachable code.
  if (NodeProperties::GetType(node).AsWasm().type.is_bottom() ||
      !(is_mutable ? &state->immutable_state : &state->mutable_state)
           ->LookupField(field_info.field_index, object)
           .IsEmpty()) {
    Node* unreachable =
        graph()->NewNode(jsgraph()->common()->Unreachable(), effect, control);
    MachineRepresentation rep =
        field_info.type->field(field_info.field_index).machine_representation();
    Node* dead_value =
        graph()->NewNode(jsgraph()->common()->DeadValue(rep), unreachable);
    NodeProperties::SetType(dead_value, NodeProperties::GetType(node));
    ReplaceWithValue(node, dead_value, unreachable, control);
    node->Kill();
    return Replace(dead_value);
  }

  HalfState const* half_state =
      is_mutable ? &state->mutable_state : &state->immutable_state;

  FieldOrElementValue lookup_result =
      half_state->LookupField(field_info.field_index, object);

  if (!lookup_result.IsEmpty() && !lookup_result.value->IsDead()) {
    std::tuple<Node*, Node*> replacement = TruncateAndExtendOrType(
        lookup_result.value, effect, control,
        field_info.type->field(field_info.field_index), field_info.is_signed);
    ReplaceWithValue(node, std::get<0>(replacement), std::get<1>(replacement),
                     control);
    node->Kill();
    return Replace(std::get<0>(replacement));
  }

  half_state = half_state->AddField(field_info.field_index, object, node);

  AbstractState const* new_state =
      is_mutable
          ? zone()->New<AbstractState>(*half_state, state->immutable_state)
          : zone()->New<AbstractState>(state->mutable_state, *half_state);

  return UpdateState(node, new_state);
}

Reduction WasmLoadElimination::ReduceWasmStructSet(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmStructSet);
  Node* object = ResolveAliases(NodeProperties::GetValueInput(node, 0));
  Node* value = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();

  const WasmFieldInfo& field_info = OpParameter<WasmFieldInfo>(node->op());
  bool is_mutable = field_info.type->mutability(field_info.field_index);

  if (is_mutable) {
    // We can find the field in the wrong half-state only in unreachable code.
    if (!(state->immutable_state.LookupField(field_info.field_index, object)
              .IsEmpty())) {
      Node* unreachable =
          graph()->NewNode(jsgraph()->common()->Unreachable(), effect, control);
      return Replace(unreachable);
    }

    HalfState const* mutable_state =
        state->mutable_state.KillField(field_info.field_index, object);
    mutable_state =
        mutable_state->AddField(field_info.field_index, object, value);
    AbstractState const* new_state =
        zone()->New<AbstractState>(*mutable_state, state->immutable_state);
    return UpdateState(node, new_state);
  } else {
    // We can find the field in the wrong half-state only in unreachable code.
    if (!(state->mutable_state.LookupField(field_info.field_index, object)
              .IsEmpty())) {
      Node* unreachable =
          graph()->NewNode(jsgraph()->common()->Unreachable(), effect, control);
      return Replace(unreachable);
    }
    // We should not initialize the same immutable field twice.
    DCHECK(state->immutable_state.LookupField(field_info.field_index, object)
               .IsEmpty());
    HalfState const* immutable_state =
        state->immutable_state.AddField(field_info.field_index, object, value);
    AbstractState const* new_state =
        zone()->New<AbstractState>(state->mutable_state, *immutable_state);
    return UpdateState(node, new_state);
  }
}

Reduction WasmLoadElimination::ReduceWasmArrayLength(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmArrayLength);
  Node* object = ResolveAliases(NodeProperties::GetValueInput(node, 0));
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();

  HalfState const* immutable_state = &state->immutable_state;

  FieldOrElementValue lookup_result =
      immutable_state->LookupField(kArrayLengthFieldIndex, object);

  if (!lookup_result.IsEmpty() && !lookup_result.value->IsDead()) {
    ReplaceWithValue(node, lookup_result.value, effect, control);
    node->Kill();
    return Replace(lookup_result.value);
  }

  immutable_state =
      immutable_state->AddField(kArrayLengthFieldIndex, object, node);

  AbstractState const* new_state =
      zone()->New<AbstractState>(state->mutable_state, *immutable_state);

  return UpdateState(node, new_state);
}

Reduction WasmLoadElimination::ReduceWasmArrayInitializeLength(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kWasmArrayInitializeLength);
  Node* object = ResolveAliases(NodeProperties::GetValueInput(node, 0));
  Node* value = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);

  AbstractState const* state = node_states_.Get(effect);
  if (state == nullptr) return NoChange();

  // We should not initialize the length twice.
  DCHECK(state->immutable_state.LookupField(kArrayLengthFieldIndex, object)
             .IsEmpty());
  HalfState const* immutable_state =
      state->immutable_state.AddField(kArrayLengthFieldIndex, object, value);
  AbstractState const* new_state =
      zone()->New<AbstractState>(state->mutable_state, *immutable_state);
  return UpdateState(node, new_state);
}

Reduction WasmLoadElimination::ReduceOtherNode(Node* node) {
  if (node->op()->EffectOutputCount() == 0) return NoChange();
  DCHECK_EQ(node->op()->EffectInputCount(), 1);
  Node* const effect = NodeProperties::GetEffectInput(node);
  AbstractState const* state = node_states_.Get(effect);
  // If we do not know anything about the predecessor, do not propagate just
  // yet because we will have to recompute anyway once we compute the
  // predecessor.
  if (state == nullptr) return NoChange();
  // If this {node} has some uncontrolled side effects (i.e. it is a call
  // without {kNoWrite}), set its state to the immutable half-state of its
  // input state, otherwise to its input state.
  return UpdateState(node, node->opcode() == IrOpcode::kCall &&
                                   !node->op()->HasProperty(Operator::kNoWrite)
                               ? zone()->New<AbstractState>(
                                     HalfState(zone()), state->immutable_state)
                               : state);
}

Reduction WasmLoadElimination::ReduceStart(Node* node) {
  return UpdateState(node, empty_state());
}

Reduction WasmLoadElimination::ReduceEffectPhi(Node* node) {
  Node* const effect0 = NodeProperties::GetEffectInput(node, 0);
  Node* const control = NodeProperties::GetControlInput(node);
  AbstractState const* state0 = node_states_.Get(effect0);
  if (state0 == nullptr) return NoChange();
  if (control->opcode() == IrOpcode::kLoop) {
    // Here we rely on having only reducible loops:
    // The loop entry edge always dominates the header, so we can just take
    // the state from the first input, and compute the loop state based on it.
    AbstractState const* state = ComputeLoopState(node, state0);
    return UpdateState(node, state);
  }
  DCHECK_EQ(IrOpcode::kMerge, control->opcode());

  // Shortcut for the case when we do not know anything about some input.
  int const input_count = node->op()->EffectInputCount();
  for (int i = 1; i < input_count; ++i) {
    Node* const effect = NodeProperties::GetEffectInput(node, i);
    if (node_states_.Get(effect) == nullptr) return NoChange();
  }

  // Make a copy of the first input's state and intersect it with the state
  // from other inputs.
  // TODO(manoskouk): Consider computing phis for at least a subset of the
  // state.
  AbstractState* state = zone()->New<AbstractState>(*state0);
  for (int i = 1; i < input_count; ++i) {
    Node* const input = NodeProperties::GetEffectInput(node, i);
    state->IntersectWith(node_states_.Get(input));
  }
  return UpdateState(node, state);
}

/***** AbstractState implementation *****/

WasmLoadElimination::FieldOrElementValue
WasmLoadElimination::HalfState::LookupField(int field_index,
                                            Node* object) const {
  return fields_.Get(field_index).Get(object);
}

WasmLoadElimination::HalfState const* WasmLoadElimination::HalfState::AddField(
    int field_index, Node* object, Node* value) const {
  HalfState* new_state = zone_->New<HalfState>(*this);
  Update(new_state->fields_, field_index, object, FieldOrElementValue(value));
  return new_state;
}

WasmLoadElimination::HalfState const* WasmLoadElimination::HalfState::KillField(
    int field_index, Node* object) const {
  const InnerMap& same_index_map = fields_.Get(field_index);
  InnerMap new_map(same_index_map);
  for (std::pair<Node*, FieldOrElementValue> pair : same_index_map) {
    if (MayAlias(pair.first, object)) {
      new_map.Set(pair.first, FieldOrElementValue());
    }
  }
  HalfState* result = zone_->New<HalfState>(*this);
  result->fields_.Set(field_index, new_map);
  return result;
}

WasmLoadElimination::AbstractState const* WasmLoadElimination::ComputeLoopState(
    Node* node, AbstractState const* state) const {
  DCHECK_EQ(node->opcode(), IrOpcode::kEffectPhi);
  std::queue<Node*> queue;
  std::unordered_set<Node*> visited;
  visited.insert(node);
  for (int i = 1; i < node->InputCount() - 1; ++i) {
    queue.push(node->InputAt(i));
  }
  while (!queue.empty()) {
    Node* const current = queue.front();
    queue.pop();
    if (visited.insert(current).second) {
      if (current->opcode() == IrOpcode::kWasmStructSet) {
        Node* object = NodeProperties::GetValueInput(current, 0);
        WasmFieldInfo field_info = OpParameter<WasmFieldInfo>(current->op());
        bool is_mutable = field_info.type->mutability(field_info.field_index);
        if (is_mutable) {
          const HalfState* new_mutable_state =
              state->mutable_state.KillField(field_info.field_index, object);
          state = zone()->New<AbstractState>(*new_mutable_state,
                                             state->immutable_state);
        } else {
          // TODO(manoskouk): DCHECK
        }
      } else if (current->opcode() == IrOpcode::kCall &&
                 !current->op()->HasProperty(Operator::kNoWrite)) {
        return zone()->New<AbstractState>(HalfState(zone()),
                                          state->immutable_state);
      }
      for (int i = 0; i < current->op()->EffectInputCount(); ++i) {
        queue.push(NodeProperties::GetEffectInput(current, i));
      }
    }
  }
  return state;
}

void WasmLoadElimination::HalfState::IntersectWith(HalfState const* that) {
  FieldOrElementValue empty;
  for (const std::pair<int, InnerMap> to_map : fields_) {
    InnerMap to_map_copy(to_map.second);
    int key = to_map.first;
    const InnerMap& current_map = that->fields_.Get(key);
    for (std::pair<Node*, FieldOrElementValue> value : to_map.second) {
      if (current_map.Get(value.first) != value.second) {
        to_map_copy.Set(value.first, empty);
      }
    }
    fields_.Set(key, to_map_copy);
  }
}

/***** Constructor/ trivial accessors *****/
WasmLoadElimination::WasmLoadElimination(Editor* editor, JSGraph* jsgraph,
                                         Zone* zone)
    : AdvancedReducer(editor),
      empty_state_(zone),
      node_states_(jsgraph->graph()->NodeCount(), zone),
      jsgraph_(jsgraph),
      zone_(zone) {}

CommonOperatorBuilder* WasmLoadElimination::common() const {
  return jsgraph()->common();
}

MachineOperatorBuilder* WasmLoadElimination::machine() const {
  return jsgraph()->machine();
}

Graph* WasmLoadElimination::graph() const { return jsgraph()->graph(); }

Isolate* WasmLoadElimination::isolate() const { return jsgraph()->isolate(); }

}  // namespace v8::internal::compiler
