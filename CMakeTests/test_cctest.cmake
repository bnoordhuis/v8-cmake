#
# From test/cctest/BUILD.gn
#

set(D ${PROJECT_SOURCE_DIR}/v8/test/cctest)

config(cctest_config)

add_executable(cctest)
target_sources(cctest
  PRIVATE
    ${D}/cctest.cc
    $<TARGET_OBJECTS:cctest_sources>
    $<TARGET_OBJECTS:wasm_test_common>
  )
target_config(cctest
  PRIVATE v8_features v8_disable_exceptions internal_config_base v8_tracing_config cctest_config)
target_link_libraries(cctest PRIVATE
  v8_compiler
  v8_snapshot
  v8_initializers
  v8_libplatform
  v8_debug_helper
  )
add_dependencies(cctest v8_dump_build_config)

add_library(cctest_sources OBJECT)
target_sources(cctest_sources
  PRIVATE
    ${D}/../common/assembler-tester.h
    ${D}/../common/wasm/flag-utils.h
    ${D}/../common/wasm/test-signatures.h
    ${D}/../common/wasm/wasm-macro-gen.h
    ${D}/collector.h
    ${D}/compiler/c-signature.h
    ${D}/compiler/call-tester.h
    ${D}/compiler/code-assembler-tester.h
    ${D}/compiler/codegen-tester.cc
    ${D}/compiler/codegen-tester.h
    ${D}/compiler/function-tester.cc
    ${D}/compiler/function-tester.h
    #${D}/compiler/graph-builder-tester.h
    ${D}/compiler/serializer-tester.cc
    ${D}/compiler/serializer-tester.h
    ${D}/compiler/test-basic-block-profiler.cc
    ${D}/compiler/test-branch-combine.cc
    ${D}/compiler/test-code-assembler.cc
    ${D}/compiler/test-code-generator.cc
    ${D}/compiler/test-gap-resolver.cc
    ${D}/compiler/test-graph-visualizer.cc
    ${D}/compiler/test-instruction-scheduler.cc
    ${D}/compiler/test-instruction.cc
    ${D}/compiler/test-js-constant-cache.cc
    ${D}/compiler/test-js-context-specialization.cc
    ${D}/compiler/test-js-typed-lowering.cc
    ${D}/compiler/test-jump-threading.cc
    ${D}/compiler/test-linkage.cc
    ${D}/compiler/test-loop-analysis.cc
    ${D}/compiler/test-machine-operator-reducer.cc
    ${D}/compiler/test-multiple-return.cc
    ${D}/compiler/test-node.cc
    ${D}/compiler/test-operator.cc
    ${D}/compiler/test-representation-change.cc
    ${D}/compiler/test-run-bytecode-graph-builder.cc
    ${D}/compiler/test-run-calls-to-external-references.cc
    ${D}/compiler/test-run-deopt.cc
    ${D}/compiler/test-run-intrinsics.cc
    ${D}/compiler/test-run-jsbranches.cc
    ${D}/compiler/test-run-jscalls.cc
    ${D}/compiler/test-run-jsexceptions.cc
    ${D}/compiler/test-run-jsobjects.cc
    ${D}/compiler/test-run-jsops.cc
    ${D}/compiler/test-run-load-store.cc
    ${D}/compiler/test-run-machops.cc
    ${D}/compiler/test-run-native-calls.cc
    ${D}/compiler/test-run-retpoline.cc
    ${D}/compiler/test-run-stackcheck.cc
    ${D}/compiler/test-run-tail-calls.cc
    ${D}/compiler/test-run-unwinding-info.cc
    ${D}/compiler/test-run-variables.cc
    ${D}/compiler/value-helper.cc
    ${D}/compiler/value-helper.h
    ${D}/disasm-regex-helper.cc
    ${D}/disasm-regex-helper.h
    ${D}/expression-type-collector-macros.h
    ${D}/gay-fixed.cc
    ${D}/gay-fixed.h
    ${D}/gay-precision.cc
    ${D}/gay-precision.h
    ${D}/gay-shortest.cc
    ${D}/gay-shortest.h
    ${D}/heap/heap-tester.h
    ${D}/heap/heap-utils.cc
    ${D}/heap/heap-utils.h
    ${D}/heap/test-alloc.cc
    ${D}/heap/test-array-buffer-tracker.cc
    ${D}/heap/test-compaction.cc
    ${D}/heap/test-concurrent-allocation.cc
    ${D}/heap/test-concurrent-marking.cc
    ${D}/heap/test-embedder-tracing.cc
    ${D}/heap/test-external-string-tracker.cc
    ${D}/heap/test-heap.cc
    ${D}/heap/test-incremental-marking.cc
    ${D}/heap/test-invalidated-slots.cc
    ${D}/heap/test-iterators.cc
    ${D}/heap/test-lab.cc
    ${D}/heap/test-mark-compact.cc
    ${D}/heap/test-memory-measurement.cc
    ${D}/heap/test-page-promotion.cc
    ${D}/heap/test-spaces.cc
    ${D}/heap/test-unmapper.cc
    ${D}/heap/test-weak-references.cc
    ${D}/interpreter/bytecode-expectations-printer.cc
    ${D}/interpreter/bytecode-expectations-printer.h
    ${D}/interpreter/interpreter-tester.cc
    ${D}/interpreter/interpreter-tester.h
    ${D}/interpreter/source-position-matcher.cc
    ${D}/interpreter/source-position-matcher.h
    ${D}/interpreter/test-bytecode-generator.cc
    ${D}/interpreter/test-interpreter-intrinsics.cc
    ${D}/interpreter/test-interpreter.cc
    ${D}/interpreter/test-source-positions.cc
    ${D}/libplatform/test-tracing.cc
    ${D}/libsampler/test-sampler.cc
    ${D}/manually-externalized-buffer.h
    ${D}/parsing/test-parse-decision.cc
    ${D}/parsing/test-preparser.cc
    ${D}/parsing/test-scanner-streams.cc
    ${D}/parsing/test-scanner.cc
    ${D}/print-extension.cc
    ${D}/print-extension.h
    ${D}/profiler-extension.cc
    ${D}/profiler-extension.h
    ${D}/scope-test-helper.h
    ${D}/setup-isolate-for-tests.cc
    ${D}/setup-isolate-for-tests.h
    ${D}/test-access-checks.cc
    ${D}/test-accessor-assembler.cc
    ${D}/test-accessors.cc
    ${D}/test-allocation.cc
    ${D}/test-api-accessors.cc
    ${D}/test-api-array-buffer.cc
    ${D}/test-api-icu.cc
    ${D}/test-api-interceptors.cc
    ${D}/test-api-stack-traces.cc
    ${D}/test-api-typed-array.cc
    ${D}/test-api-wasm.cc
    ${D}/test-api.cc
    ${D}/test-api.h
    ${D}/test-array-list.cc
    ${D}/test-atomicops.cc
    ${D}/test-backing-store.cc
    ${D}/test-bignum-dtoa.cc
    ${D}/test-bignum.cc
    ${D}/test-bit-vector.cc
    ${D}/test-circular-queue.cc
    ${D}/test-code-layout.cc
    ${D}/test-code-pages.cc
    ${D}/test-code-stub-assembler.cc
    ${D}/test-compiler.cc
    ${D}/test-constantpool.cc
    ${D}/test-conversions.cc
    ${D}/test-cpu-profiler.cc
    ${D}/test-date.cc
    ${D}/test-debug-helper.cc
    ${D}/test-debug.cc
    ${D}/test-decls.cc
    ${D}/test-deoptimization.cc
    ${D}/test-dictionary.cc
    ${D}/test-diy-fp.cc
    ${D}/test-double.cc
    ${D}/test-dtoa.cc
    ${D}/test-elements-kind.cc
    ${D}/test-factory.cc
    ${D}/test-fast-dtoa.cc
    ${D}/test-feedback-vector.cc
    ${D}/test-feedback-vector.h
    ${D}/test-field-type-tracking.cc
    ${D}/test-fixed-dtoa.cc
    ${D}/test-flags.cc
    ${D}/test-func-name-inference.cc
    ${D}/test-global-handles.cc
    ${D}/test-global-object.cc
    ${D}/test-hashcode.cc
    ${D}/test-hashmap.cc
    ${D}/test-heap-profiler.cc
    ${D}/test-icache.cc
    ${D}/test-identity-map.cc
    ${D}/test-inobject-slack-tracking.cc
    ${D}/test-inspector.cc
    ${D}/test-intl.cc
    ${D}/test-js-weak-refs.cc
    ${D}/test-liveedit.cc
    ${D}/test-local-handles.cc
    ${D}/test-lockers.cc
    ${D}/test-log.cc
    ${D}/test-managed.cc
    ${D}/test-mementos.cc
    ${D}/test-modules.cc
    ${D}/test-object.cc
    ${D}/test-orderedhashtable.cc
    ${D}/test-parsing.cc
    ${D}/test-persistent-handles.cc
    ${D}/test-platform.cc
    ${D}/test-profile-generator.cc
    ${D}/test-random-number-generator.cc
    ${D}/test-regexp.cc
    ${D}/test-representation.cc
    ${D}/test-roots.cc
    ${D}/test-sampler-api.cc
    ${D}/test-serialize.cc
    ${D}/test-smi-lexicographic-compare.cc
    ${D}/test-strings.cc
    ${D}/test-strtod.cc
    ${D}/test-symbols.cc
    ${D}/test-thread-termination.cc
    ${D}/test-threads.cc
    ${D}/test-trace-event.cc
    ${D}/test-traced-value.cc
    ${D}/test-transitions.cc
    ${D}/test-transitions.h
    ${D}/test-typedarrays.cc
    ${D}/test-types.cc
    ${D}/test-unboxed-doubles.cc
    ${D}/test-unscopables-hidden-prototype.cc
    ${D}/test-unwinder-code-pages.cc
    ${D}/test-unwinder.cc
    ${D}/test-usecounters.cc
    ${D}/test-utils.cc
    ${D}/test-version.cc
    ${D}/test-weakmaps.cc
    ${D}/test-weaksets.cc
    ${D}/torque/test-torque.cc
    ${D}/trace-extension.cc
    ${D}/trace-extension.h
    ${D}/unicode-helpers.cc
    ${D}/unicode-helpers.h
    ${D}/wasm/test-c-wasm-entry.cc
    ${D}/wasm/test-compilation-cache.cc
    ${D}/wasm/test-gc.cc
    ${D}/wasm/test-grow-memory.cc
    ${D}/wasm/test-jump-table-assembler.cc
    ${D}/wasm/test-liftoff-inspection.cc
    ${D}/wasm/test-run-wasm-64.cc
    ${D}/wasm/test-run-wasm-asmjs.cc
    ${D}/wasm/test-run-wasm-atomics.cc
    ${D}/wasm/test-run-wasm-atomics64.cc
    ${D}/wasm/test-run-wasm-bulk-memory.cc
    ${D}/wasm/test-run-wasm-exceptions.cc
    ${D}/wasm/test-run-wasm-interpreter.cc
    ${D}/wasm/test-run-wasm-js.cc
    ${D}/wasm/test-run-wasm-module.cc
    ${D}/wasm/test-run-wasm-sign-extension.cc
    ${D}/wasm/test-run-wasm-simd-liftoff.cc
    ${D}/wasm/test-run-wasm-simd-scalar-lowering.cc
    ${D}/wasm/test-run-wasm-simd.cc
    ${D}/wasm/test-run-wasm.cc
    ${D}/wasm/test-streaming-compilation.cc
    ${D}/wasm/test-wasm-breakpoints.cc
    ${D}/wasm/test-wasm-codegen.cc
    ${D}/wasm/test-wasm-debug-evaluate.cc
    ${D}/#wasm/test-wasm-debug-evaluate.h
    ${D}/wasm/test-wasm-import-wrapper-cache.cc
    ${D}/wasm/test-wasm-serialization.cc
    ${D}/wasm/test-wasm-shared-engine.cc
    ${D}/wasm/test-wasm-stack.cc
    ${D}/wasm/test-wasm-trap-position.cc
    ${D}/wasm/wasm-atomics-utils.h
    ${D}/wasm/wasm-run-utils.cc
    ${D}/wasm/wasm-run-utils.h
    $<${is-arm}:
      ${D}/assembler-helper-arm.cc
      ${D}/assembler-helper-arm.h
      ${D}/test-assembler-arm.cc
      ${D}/test-disasm-arm.cc
      ${D}/test-macro-assembler-arm.cc
      ${D}/test-poison-disasm-arm.cc
      ${D}/test-sync-primitives-arm.cc
    >
    $<${is-arm64}:
      ${D}/test-assembler-arm64.cc
      ${D}/test-disasm-arm64.cc
      ${D}/test-fuzz-arm64.cc
      ${D}/test-javascript-arm64.cc
      ${D}/test-js-arm64-variables.cc
      ${D}/test-macro-assembler-arm64.cc
      ${D}/test-pointer-auth-arm64.cc
      ${D}/test-poison-disasm-arm64.cc
      ${D}/test-sync-primitives-arm64.cc
      ${D}/test-utils-arm64.cc
      ${D}/test-utils-arm64.h
    >
    $<$<AND:${is-arm64},${is-win}>:
      ${D}/test-stack-unwinding-win64.cc
    >
    $<${is-ia32}:
      ${D}/test-assembler-ia32.cc
      ${D}/test-disasm-ia32.cc
      ${D}/test-log-stack-tracer.c
    >
    $<${is-mips}:
      ${D}/test-assembler-mips.cc
      ${D}/test-disasm-mips.cc
      ${D}/test-macro-assembler-mips.cc
    >
    $<${is-mipsel}:
      ${D}/test-assembler-mips.cc
      ${D}/test-disasm-mips.cc
      ${D}/test-macro-assembler-mips.cc
    >
    $<${is-mips64}:
      ${D}/test-assembler-mips64.cc
      ${D}/test-disasm-mips64.cc
      ${D}/test-macro-assembler-mips64.cc
    >
    $<${is-mips64el}:
      ${D}/test-assembler-mips64.cc
      ${D}/test-disasm-mips64.cc
      ${D}/test-macro-assembler-mips64.cc
    >
    $<${is-x64}:
      ${D}/test-assembler-x64.cc
      ${D}/test-disasm-x64.cc
      ${D}/test-log-stack-tracer.cc
      ${D}/test-macro-assembler-x64.cc
    >
    $<$<AND:${is-x64},${is-win}>:
      ${D}/test-stack-unwinding-win64.cc
    >
    $<$<OR:${is-ppc},${is-ppc64}>:
      ${D}/test-assembler-ppc.cc
      ${D}/test-disasm-ppc.cc
    >
    $<$<OR:${is-s390},${is-s390x}>:
      ${D}/test-assembler-s390.cc
      ${D}/test-disasm-s390.cc
    >
  )
target_config(cctest_sources
  PRIVATE
    v8_features
    v8_disable_exceptions
    cppgc_base_config
    external_config
    internal_config_base
    v8_tracing_config
  )
target_compile_options(cctest_sources PRIVATE $<${is-clang}:-Wno-narrowing>)
target_include_directories(cctest_sources PRIVATE ${PROJECT_BINARY_DIR}/inspector)

target_link_libraries(cctest_sources
  PUBLIC
    #v8_for_testing
    v8_initializers # via v8_for_testing
    v8_libbase
    v8_libplatform
    #wasm_module_runner
    #wasm_test_common
    v8_debug_helper
  )
add_dependencies(cctest_sources v8_inspector)