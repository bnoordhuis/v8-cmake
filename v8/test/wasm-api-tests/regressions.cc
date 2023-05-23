// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/wasm-api-tests/wasm-api-test.h"

namespace v8 {
namespace internal {
namespace wasm {

TEST_F(WasmCapiTest, Regressions) {
  FunctionSig sig(0, 0, nullptr);
  uint8_t code[] = {WASM_UNREACHABLE};
  WasmFunctionBuilder* start_func = builder()->AddFunction(&sig);
  start_func->EmitCode(code, static_cast<uint32_t>(sizeof(code)));
  start_func->Emit(kExprEnd);
  builder()->MarkStartFunction(start_func);
  builder()->AddImport(base::CStrVector("dummy"), &sig);

  // Ensure we can validate.
  bool valid = Validate();
  EXPECT_EQ(valid, true);

  // Ensure we can compile after validating.
  Compile();
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
