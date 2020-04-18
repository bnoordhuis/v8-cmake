cmake_minimum_required(VERSION 3.4)
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MSVC.cmake")
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
project(V8 LANGUAGES CXX C ASM)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_ASM_MASM_SOURCE_FILE_EXTENSIONS S)
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Python 3 REQUIRED QUIET COMPONENTS Interpreter)
find_package(Threads QUIET)

include(CMakeDependentOption)
include(GNUInstallDirs)

include(CheckPythonModuleExists)
include(GenerateBuiltinsList)

check_python_module_exists(PYTHON_HAVE_MARKUPSAFE markupsafe)

if (NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if (NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 90)
endif()

set(is-freebsd $<PLATFORM_ID:FreeBSD>)
set(is-darwin $<PLATFORM_ID:Darwin>)
set(is-linux $<PLATFORM_ID:Linux>)
set(is-win $<PLATFORM_ID:Windows>)
set(is-aix $<PLATFORM_ID:AIX>)
string(CONCAT is-posix $<OR:
  ${is-freebsd},
  ${is-darwin},
  ${is-linux},
  ${is-aix}
>)

set(is-msvc $<CXX_COMPILER_ID:MSVC>)

# FIXME obviously
set(is-arm 0)
set(is-arm64 0)
set(is-ia32 0)
set(is-mips 0)
set(is-mips64 0)
set(is-ppc 0)

set(is-s390 $<STREQUAL:${CMAKE_SYSTEM_PROCESSOR},S390>)
string(CONCAT is-x64 $<OR:
  $<STREQUAL:${CMAKE_SYSTEM_PROCESSOR},AMD64>,
  $<STREQUAL:${CMAKE_SYSTEM_PROCESSOR},x86_64>
>)

option(V8_ENABLE_CONCURRENT_MARKING "Enable concurrent marking" ON)
option(V8_ENABLE_I18N "Enable Internationalization support")

set(
  v8_defines
  $<${is-darwin}:V8_HAVE_TARGET_OS>
  $<${is-darwin}:V8_TARGET_OS_MACOSX>
  $<${is-linux}:V8_HAVE_TARGET_OS>
  $<${is-linux}:V8_TARGET_OS_LINUX>
  $<${is-win}:V8_HAVE_TARGET_OS>
  $<${is-win}:V8_TARGET_OS_WIN>
  $<${is-x64}:V8_TARGET_ARCH_X64>
  $<${is-win}:NOMINMAX>
  $<$<OR:${is-win},${is-x64}>:V8_OS_WINX64>
  $<$<BOOL:${V8_ENABLE_CONCURRENT_MARKING}>:V8_CONCURRENT_MARKING>
  $<${is-win}:V8_OS_WIN32>
)

set(D ${PROJECT_SOURCE_DIR}/v8)

set(disable-exceptions
  $<$<CXX_COMPILER_ID:MSVC>:/EHs-c->
  $<$<CXX_COMPILER_ID:AppleClang>:-fno-exceptions>
  $<$<CXX_COMPILER_ID:Clang>:-fno-exceptions>
  $<$<CXX_COMPILER_ID:GNU>:-fno-exceptions>)

set(enable-exceptions
  $<$<CXX_COMPILER_ID:MSVC>:/EHsc>
  $<$<CXX_COMPILER_ID:AppleClang>:-fexceptions>
  $<$<CXX_COMPILER_ID:Clang>:-fexceptions>
  $<$<CXX_COMPILER_ID:GNU>:-fexceptions>)

#
# d8
#

add_executable(
  d8
  $<${is-posix}:${D}/src/d8/d8-posix.cc>
  $<${is-win}:${D}/src/d8/d8-windows.cc>
  ${D}/src/d8/async-hooks-wrapper.cc
  ${D}/src/d8/d8-console.cc
  ${D}/src/d8/d8-js.cc
  ${D}/src/d8/d8-platforms.cc
  ${D}/src/d8/d8.cc)

target_compile_definitions(d8 PRIVATE $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(d8 PRIVATE ${disable-exceptions})
target_include_directories(d8
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/v8/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/v8>
)

target_link_libraries(d8
  PRIVATE
    v8_base_without_compiler
    v8_compiler
    v8_snapshot
    v8_inspector
    v8_libplatform)

# more granular library support
add_library(v8-i18n-support OBJECT)
set_property(TARGET v8-i18n-support PROPERTY EXCLUDE_FROM_ALL ON)

list(APPEND i18n-sources
  ${PROJECT_SOURCE_DIR}/v8/src/builtins/builtins-intl.cc
  ${PROJECT_SOURCE_DIR}/v8/src/objects/intl-objects.cc
  ${PROJECT_SOURCE_DIR}/v8/src/objects/js-break-iterator.cc
  ${PROJECT_SOURCE_DIR}/v8/src/objects/js-collator.cc
  ${PROJECT_SOURCE_DIR}/v8/src/objects/js-date-time-format.cc
  ${PROJECT_SOURCE_DIR}/v8/src/objects/js-display-names.cc
  ${PROJECT_SOURCE_DIR}/v8/src/objects/js-list-format.cc
  ${PROJECT_SOURCE_DIR}/v8/src/objects/js-locale.cc
  ${PROJECT_SOURCE_DIR}/v8/src/objects/js-number-format.cc
  ${PROJECT_SOURCE_DIR}/v8/src/objects/js-plural-rules.cc
  ${PROJECT_SOURCE_DIR}/v8/src/objects/js-relative-time-format.cc
  ${PROJECT_SOURCE_DIR}/v8/src/objects/js-segment-iterator.cc
  ${PROJECT_SOURCE_DIR}/v8/src/objects/js-segmenter.cc
  ${PROJECT_SOURCE_DIR}/v8/src/runtime/runtime-intl.cc
  ${PROJECT_SOURCE_DIR}/v8/src/strings/char-predicates.cc)

target_sources(v8-i18n-support PRIVATE ${i18n-sources})
target_compile_definitions(v8-i18n-support PRIVATE $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(v8-i18n-support PRIVATE ${disable-exceptions})
target_link_libraries(v8-i18n-support PRIVATE v8_torque_generated)

target_include_directories(v8-i18n-support
  PRIVATE
    ${D}
    ${D}/src/objects
    ${D}/include)

file(GLOB api-sources CONFIGURE_DEPENDS v8/src/api/*.cc)
file(GLOB asmjs-sources CONFIGURE_DEPENDS v8/src/asmjs/*.cc)
file(GLOB ast-sources CONFIGURE_DEPENDS v8/src/ast/*.cc)
file(GLOB builtin-sources CONFIGURE_DEPENDS v8/src/builtins/*.cc)
file(GLOB codegen-sources CONFIGURE_DEPENDS v8/src/codegen/*.cc)
file(GLOB debug-sources CONFIGURE_DEPENDS v8/src/debug/*.cc)
file(GLOB deoptimizer-sources CONFIGURE_DEPENDS v8/src/deoptimizer/*.cc)
file(GLOB diagnostic-sources CONFIGURE_DEPENDS v8/src/diagnostics/*.cc)
file(GLOB execution-sources CONFIGURE_DEPENDS v8/src/execution/*.cc)
file(GLOB extensions-sources CONFIGURE_DEPENDS v8/src/extensions/*.cc)
file(GLOB handles-sources CONFIGURE_DEPENDS v8/src/handles/*.cc)
file(GLOB interpreter-sources CONFIGURE_DEPENDS v8/src/interpreter/*.cc)
file(GLOB json-sources CONFIGURE_DEPENDS v8/src/json/*.cc)
file(GLOB logging-sources CONFIGURE_DEPENDS v8/src/logging/*.cc)
file(GLOB heap-sources CONFIGURE_DEPENDS v8/src/heap/*.cc v8/src/heap/third-party/*.cc)
file(GLOB numbers-sources CONFIGURE_DEPENDS v8/src/numbers/*.cc)
file(GLOB object-sources CONFIGURE_DEPENDS v8/src/objects/*.cc)
file(GLOB parsing-sources CONFIGURE_DEPENDS v8/src/parsing/*.cc)
file(GLOB profiler-sources CONFIGURE_DEPENDS v8/src/profiler/*.cc)
file(GLOB regexp-sources CONFIGURE_DEPENDS v8/src/regexp/*.cc)
file(GLOB runtime-sources CONFIGURE_DEPENDS v8/src/runtime/*.cc)
file(GLOB snapshot-sources CONFIGURE_DEPENDS v8/src/snapshot/*.cc)
file(GLOB strings-sources CONFIGURE_DEPENDS v8/src/strings/*.cc)
file(GLOB utils-sources CONFIGURE_DEPENDS v8/src/utils/*.cc)
file(GLOB wasm-sources CONFIGURE_DEPENDS v8/src/wasm/*.cc)
file(GLOB wasm-baseline-sources CONFIGURE_DEPENDS v8/src/wasm/baseline/*.cc)
file(GLOB zone-sources CONFIGURE_DEPENDS v8/src/zone/*.cc)

list(REMOVE_ITEM builtin-sources
  ${PROJECT_SOURCE_DIR}/v8/src/builtins/builtins-intl-gen.cc
  ${PROJECT_SOURCE_DIR}/v8/src/builtins/builtins-intl.cc)
list(REMOVE_ITEM diagnostic-sources
  ${PROJECT_SOURCE_DIR}/v8/src/diagnostics/unwinding-info-win64.cc)
list(REMOVE_ITEM object-sources ${i18n-sources})
list(REMOVE_ITEM regexp-sources
  ${PROJECT_SOURCE_DIR}/v8/src/regexp/gen-regexp-special-case.cc)
list(REMOVE_ITEM runtime-sources
  ${PROJECT_SOURCE_DIR}/v8/src/runtime/runtime-intl.cc)
list(REMOVE_ITEM snapshot-sources
  ${PROJECT_SOURCE_DIR}/v8/src/snapshot/mksnapshot.cc
  ${PROJECT_SOURCE_DIR}/v8/src/snapshot/snapshot-empty.cc
  ${PROJECT_SOURCE_DIR}/v8/src/snapshot/snapshot-external.cc)
list(REMOVE_ITEM strings-sources
  ${PROJECT_SOURCE_DIR}/v8/src/strings/char-predicates.cc)
# wasm-memory.h is currently missing??
list(REMOVE_ITEM wasm-sources ${PROJECT_SOURCE_DIR}/v8/src/wasm/wasm-memory.cc)

list(APPEND snapshot-sources
  ${PROJECT_SOURCE_DIR}/v8/src/snapshot/embedded/embedded-data.cc)
list(APPEND wasm-sources ${wasm-baseline-sources})

add_library(v8_base_without_compiler STATIC
  $<$<AND:${is-x64},${is-posix}>:${D}/src/trap-handler/handler-inside-posix.cc>
  $<$<AND:${is-x64},${is-posix}>:${D}/src/trap-handler/handler-outside-posix.cc>
  $<$<AND:${is-x64},${is-win}>:${D}/src/diagnostics/unwinding-info-win64.cc>
  $<$<AND:${is-x64},${is-win}>:${D}/src/trap-handler/handler-inside-win.cc>
  $<$<AND:${is-x64},${is-win}>:${D}/src/trap-handler/handler-outside-win.cc>
  $<${is-x64}:${D}/src/codegen/x64/assembler-x64.cc>
  $<${is-x64}:${D}/src/codegen/x64/cpu-x64.cc>
  $<${is-x64}:${D}/src/codegen/x64/interface-descriptors-x64.cc>
  $<${is-x64}:${D}/src/codegen/x64/macro-assembler-x64.cc>
  $<${is-x64}:${D}/src/compiler/backend/x64/code-generator-x64.cc>
  $<${is-x64}:${D}/src/compiler/backend/x64/instruction-scheduler-x64.cc>
  $<${is-x64}:${D}/src/compiler/backend/x64/instruction-selector-x64.cc>
  $<${is-x64}:${D}/src/compiler/backend/x64/unwinding-info-writer-x64.cc>
  $<${is-x64}:${D}/src/debug/x64/debug-x64.cc>
  $<${is-x64}:${D}/src/deoptimizer/x64/deoptimizer-x64.cc>
  $<${is-x64}:${D}/src/diagnostics/x64/disasm-x64.cc>
  $<${is-x64}:${D}/src/diagnostics/x64/eh-frame-x64.cc>
  $<${is-x64}:${D}/src/execution/x64/frame-constants-x64.cc>
  $<${is-x64}:${D}/src/regexp/x64/regexp-macro-assembler-x64.cc>
  $<$<BOOL:${V8_ENABLE_I18N}>:$<TARGET_OBJECTS:v8-i18n-support>>
  ${api-sources}
  ${asmjs-sources}
  ${ast-sources}
  ${builtin-sources}
  ${codegen-sources}
  ${D}/src/common/assert-scope.cc
  ${D}/src/compiler-dispatcher/compiler-dispatcher.cc
  ${D}/src/compiler-dispatcher/optimizing-compile-dispatcher.cc
  ${D}/src/date/date.cc
  ${D}/src/date/dateparser.cc
  ${debug-sources}
  ${deoptimizer-sources}
  ${diagnostic-sources}
  ${execution-sources}
  ${extensions-sources}
  ${D}/src/flags/flags.cc
  ${D}/src/handles/global-handles.cc
  ${D}/src/handles/handles.cc
  ${heap-sources}
  ${D}/src/ic/call-optimization.cc
  ${D}/src/ic/handler-configuration.cc
  ${D}/src/ic/ic-stats.cc
  ${D}/src/ic/ic.cc
  ${D}/src/ic/stub-cache.cc
  ${D}/src/init/bootstrapper.cc
  ${D}/src/init/icu_util.cc
  ${D}/src/init/isolate-allocator.cc
  ${D}/src/init/startup-data-util.cc
  ${D}/src/init/v8.cc
  ${handles-sources}
  ${interpreter-sources}
  ${json-sources}
  ${logging-sources}
  ${numbers-sources}
  ${object-sources}
  ${parsing-sources}
  ${profiler-sources}
  ${regexp-sources}
  ${D}/src/roots/roots.cc
  ${runtime-sources}
  ${D}/src/sanitizer/lsan-page-allocator.cc
  ${snapshot-sources}
  ${strings-sources}
  ${D}/src/tasks/cancelable-task.cc
  ${D}/src/tasks/task-utils.cc
  ${D}/src/third_party/siphash/halfsiphash.cc
  ${D}/src/tracing/trace-event.cc
  ${D}/src/tracing/traced-value.cc
  ${D}/src/tracing/tracing-category-observer.cc
  ${D}/src/trap-handler/handler-inside.cc
  ${D}/src/trap-handler/handler-outside.cc
  ${D}/src/trap-handler/handler-shared.cc
  ${utils-sources}
  ${wasm-sources}
  ${zone-sources}
  $<TARGET_OBJECTS:v8-adler32>
)
if (WIN32)
  if (CMAKE_SYSTEM_VERSION VERSION_GREATER 10) # Windows 10
    set(windows-version 0x0A00)
  elseif (CMAKE_SYSTEM_VERSION VERSION_GREATER 6.3) # Windows 8.1
    set(windows-version 0x0603)
  elseif (CMAKE_SYSTEM_VERSION VERSION_GREATER 6.2) # Windows 8
    set(windows-version 0x0602)
  elseif (CMAKE_SYSTEM_VERSION VERSION_GREATER 6.1) # Windows 7
    set(windows-version 0x0601)
  elseif (CMAKE_SYSTEM_VERSION VERSION_GREATER 6.0) # Windows Vista
    set(windows-version 0x0600)
  endif()
endif()

set_property(SOURCE ${D}/src/diagnostics/unwinding-info-win64.cc
  APPEND PROPERTY
    COMPILE_DEFINITIONS
      UNICODE
      _WIN32_WINNT=${windows-version})

target_compile_definitions(v8_base_without_compiler PRIVATE ${v8_defines} $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(v8_base_without_compiler PRIVATE ${disable-exceptions})

target_include_directories(
  v8_base_without_compiler
  PRIVATE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/v8/include>
    $<BUILD_INTERFACE:$<TARGET_PROPERTY:v8-bytecodes-builtin-list,INTERFACE_INCLUDE_DIRECTORIES>>
    ${D}
    ${PROJECT_BINARY_DIR}
)

target_link_libraries(
  v8_base_without_compiler
  PRIVATE
    v8_libbase
    v8_libsampler
    v8_torque_generated
  PUBLIC
    v8-bytecodes-builtin-list
)

#
# v8_compiler
#
file(GLOB_RECURSE compiler-sources CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/v8/src/compiler/*.cc")
list(FILTER compiler-sources EXCLUDE REGEX ".*backend/.*/.*[.]cc$")


add_library(v8_compiler STATIC)
target_sources(v8_compiler PRIVATE ${compiler-sources})
target_compile_definitions(v8_compiler PRIVATE ${v8_defines} $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(v8_compiler PRIVATE ${disable-exceptions})

target_include_directories(
  v8_compiler
  PUBLIC
    ${PROJECT_BINARY_DIR}
  PRIVATE
    ${PROJECT_BINARY_DIR}
    ${D}
    $<BUILD_INTERFACE:$<TARGET_PROPERTY:v8-bytecodes-builtin-list,INTERFACE_INCLUDE_DIRECTORIES>>
)

target_link_libraries(v8_compiler
  PRIVATE
    v8_libbase
    v8_base_without_compiler
    v8_torque_generated
  PUBLIC
    v8-bytecodes-builtin-list
)

#
# v8_initializers
#

add_library(
  v8_initializers STATIC
  $<${is-arm64}:${D}/src/builtins/arm64/builtins-arm64.cc>
  $<${is-arm}:${D}/src/builtins/arm/builtins-arm.cc>
  $<${is-ia32}:${D}/src/builtins/ia32/builtins-ia32.cc>
  $<${is-mips64}:${D}/src/builtins/mips64/builtins-mips64.cc>
  $<${is-mips}:${D}/src/builtins/mips/builtins-mips.cc>
  $<${is-ppc}:${D}/src/builtins/ppc/builtins-ppc.cc>
  $<${is-s390}:${D}/src/builtins/s390/builtins-s390.cc>
  $<${is-x64}:${D}/src/builtins/x64/builtins-x64.cc>
  $<$<BOOL:${V8_ENABLE_I18N}>:${D}/src/builtins/builtins-intl-gen.cc>
  ${D}/src/builtins/builtins-array-gen.cc
  ${D}/src/builtins/builtins-async-function-gen.cc
  ${D}/src/builtins/builtins-async-gen.cc
  ${D}/src/builtins/builtins-async-generator-gen.cc
  ${D}/src/builtins/builtins-async-iterator-gen.cc
  ${D}/src/builtins/builtins-bigint-gen.cc
  ${D}/src/builtins/builtins-call-gen.cc
  ${D}/src/builtins/builtins-collections-gen.cc
  ${D}/src/builtins/builtins-constructor-gen.cc
  ${D}/src/builtins/builtins-conversion-gen.cc
  ${D}/src/builtins/builtins-date-gen.cc
  ${D}/src/builtins/builtins-debug-gen.cc
  ${D}/src/builtins/builtins-function-gen.cc
  ${D}/src/builtins/builtins-generator-gen.cc
  ${D}/src/builtins/builtins-global-gen.cc
  ${D}/src/builtins/builtins-handler-gen.cc
  ${D}/src/builtins/builtins-ic-gen.cc
  ${D}/src/builtins/builtins-internal-gen.cc
  ${D}/src/builtins/builtins-interpreter-gen.cc
  ${D}/src/builtins/builtins-iterator-gen.cc
  ${D}/src/builtins/builtins-lazy-gen.cc
  ${D}/src/builtins/builtins-microtask-queue-gen.cc
  ${D}/src/builtins/builtins-number-gen.cc
  ${D}/src/builtins/builtins-object-gen.cc
  ${D}/src/builtins/builtins-promise-gen.cc
  ${D}/src/builtins/builtins-proxy-gen.cc
  ${D}/src/builtins/builtins-regexp-gen.cc
  ${D}/src/builtins/builtins-sharedarraybuffer-gen.cc
  ${D}/src/builtins/builtins-string-gen.cc
  ${D}/src/builtins/builtins-typed-array-gen.cc
  ${D}/src/builtins/builtins-wasm-gen.cc
  ${D}/src/builtins/growable-fixed-array-gen.cc
  ${D}/src/builtins/setup-builtins-internal.cc
  ${D}/src/codegen/code-stub-assembler.cc
  ${D}/src/heap/setup-heap-internal.cc
  ${D}/src/ic/accessor-assembler.cc
  ${D}/src/ic/binary-op-assembler.cc
  ${D}/src/ic/keyed-store-generic.cc
  ${D}/src/interpreter/interpreter-assembler.cc
  ${D}/src/interpreter/interpreter-generator.cc
  ${D}/src/interpreter/interpreter-intrinsics-generator.cc
)

target_compile_definitions(v8_initializers PRIVATE ${v8_defines} $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(v8_initializers PRIVATE ${disable-exceptions})

target_include_directories(
  v8_initializers
  PRIVATE ${D} ${PROJECT_BINARY_DIR}
)

target_link_libraries(v8_initializers PRIVATE v8_torque_generated v8-bytecodes-builtin-list)

#
# v8_snapshot
#

# Note: v8_use_external_startup_data not currently supported.
# Note: v8_use_multi_snapshots not currently supported.
add_library(
  v8_snapshot STATIC
  ${PROJECT_BINARY_DIR}/embedded.S
  ${PROJECT_BINARY_DIR}/snapshot.cc
  ${D}/src/init/setup-isolate-deserialize.cc
)

target_compile_definitions(v8_snapshot PRIVATE $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(v8_snapshot PRIVATE ${disable-exceptions})
target_include_directories(v8_snapshot PRIVATE ${D})
target_link_libraries(v8_snapshot
  PRIVATE
    v8_torque_generated
    v8-bytecodes-builtin-list)

# Note: allow passing in v8_random_seed
add_custom_command(
  COMMAND
    mksnapshot
    --embedded_src ${PROJECT_BINARY_DIR}/embedded.S
    --startup_src ${PROJECT_BINARY_DIR}/snapshot.cc
    $<${is-x64}:--target_arch=x64>
    $<$<PLATFORM_ID:Darwin>:--target_os=mac>
    $<$<PLATFORM_ID:Linux>:--target_os=linux>
    --turbo_instruction_scheduling
  DEPENDS
    mksnapshot
  OUTPUT
    ${PROJECT_BINARY_DIR}/embedded.S
    ${PROJECT_BINARY_DIR}/snapshot.cc
)

#
# v8_inspector
#

# Note: this is the inspector, broken out for ease of maintenance.
# The GN files are conceptually structured the same but they don't
# build it as a separate library. We do because it's easier that way.
set(inspector_files
  ${PROJECT_BINARY_DIR}/inspector/include/inspector/Debugger.h
  ${PROJECT_BINARY_DIR}/inspector/include/inspector/Runtime.h
  ${PROJECT_BINARY_DIR}/inspector/include/inspector/Schema.h
  ${PROJECT_BINARY_DIR}/inspector/src/inspector/protocol/Console.cpp
  ${PROJECT_BINARY_DIR}/inspector/src/inspector/protocol/Console.h
  ${PROJECT_BINARY_DIR}/inspector/src/inspector/protocol/Debugger.cpp
  ${PROJECT_BINARY_DIR}/inspector/src/inspector/protocol/Debugger.h
  ${PROJECT_BINARY_DIR}/inspector/src/inspector/protocol/Forward.h
  ${PROJECT_BINARY_DIR}/inspector/src/inspector/protocol/HeapProfiler.cpp
  ${PROJECT_BINARY_DIR}/inspector/src/inspector/protocol/HeapProfiler.h
  ${PROJECT_BINARY_DIR}/inspector/src/inspector/protocol/Profiler.cpp
  ${PROJECT_BINARY_DIR}/inspector/src/inspector/protocol/Profiler.h
  ${PROJECT_BINARY_DIR}/inspector/src/inspector/protocol/Protocol.cpp
  ${PROJECT_BINARY_DIR}/inspector/src/inspector/protocol/Protocol.h
  ${PROJECT_BINARY_DIR}/inspector/src/inspector/protocol/Runtime.cpp
  ${PROJECT_BINARY_DIR}/inspector/src/inspector/protocol/Runtime.h
  ${PROJECT_BINARY_DIR}/inspector/src/inspector/protocol/Schema.cpp
  ${PROJECT_BINARY_DIR}/inspector/src/inspector/protocol/Schema.h
)

foreach(filename IN LISTS inspector_files)
  get_filename_component(directory ${filename} DIRECTORY)
  list(APPEND inspector_dirs ${directory})
endforeach()
list(REMOVE_DUPLICATES inspector_dirs)

file(GLOB inspector-protocol-sources CONFIGURE_DEPENDS v8/third_party/inspector_protocol/crdtp/*.cc)
file(GLOB inspector-sources CONFIGURE_DEPENDS v8/src/inspector/*.cc)

list(FILTER inspector-protocol-sources EXCLUDE REGEX "_test[.]cc$")
list(FILTER inspector-protocol-sources EXCLUDE REGEX "crdtp/test_.*[.]cc$")

add_library(v8_inspector STATIC
  ${inspector-protocol-sources}
  ${inspector-sources}
  ${inspector_files})

target_compile_features(v8_inspector PUBLIC cxx_std_17)
target_compile_options(v8_inspector PRIVATE ${disable-exceptions})
target_compile_definitions(v8_inspector PUBLIC $<${is-msvc}:_HAS_EXCEPTIONS=0>)

target_include_directories(v8_inspector
  PRIVATE
    ${PROJECT_BINARY_DIR}/inspector
    ${D}
    ${D}/include
    ${D}/third_party/googletest/src/googlemock/include
    ${D}/third_party/googletest/src/googletest/include)

target_link_libraries(v8_inspector PRIVATE v8_torque_generated)

file(GLOB inspector-templates
  CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/v8/third_party/inspector_protocol/lib/*.template"
    "${PROJECT_SOURCE_DIR}/v8/third_party/inspector_protocol/templates/*.template")

add_custom_command(
  COMMAND ${CMAKE_COMMAND} -E make_directory ${inspector_dirs}
  OUTPUT ${inspector_dirs}
)

add_custom_command(
  COMMAND
    Python::Interpreter
    ${D}/third_party/inspector_protocol/code_generator.py
    --config ${D}/src/inspector/inspector_protocol_config.json
    --jinja_dir ${D}/third_party
    --output_base ${PROJECT_BINARY_DIR}/inspector/src/inspector
    --inspector_protocol_dir ${D}/third_party/inspector_protocol
  DEPENDS
    ${D}/third_party/inspector_protocol/code_generator.py
    ${D}/src/inspector/inspector_protocol_config.json
    ${inspector-templates}
    ${inspector_dirs}
  OUTPUT
    ${inspector_files}
)

#
# v8_libplatform
#

add_library(v8_libplatform STATIC)
target_sources(v8_libplatform
  PRIVATE
    ${D}/src/libplatform/default-foreground-task-runner.cc
    ${D}/src/libplatform/default-platform.cc
    ${D}/src/libplatform/default-worker-threads-task-runner.cc
    ${D}/src/libplatform/delayed-task-queue.cc
    ${D}/src/libplatform/task-queue.cc
    ${D}/src/libplatform/tracing/trace-buffer.cc
    ${D}/src/libplatform/tracing/trace-config.cc
    ${D}/src/libplatform/tracing/trace-object.cc
    ${D}/src/libplatform/tracing/trace-writer.cc
    ${D}/src/libplatform/tracing/tracing-controller.cc
    ${D}/src/libplatform/worker-thread.cc)

target_compile_definitions(v8_libplatform PRIVATE $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(v8_libplatform PRIVATE ${disable-exceptions})
target_include_directories(
  v8_libplatform
  PUBLIC ${D}/include
  PRIVATE ${D}
)

target_link_libraries(v8_libplatform PRIVATE v8_libbase)

#
# v8_libsampler
#

add_library(v8_libsampler STATIC
  ${D}/src/libsampler/sampler.cc)

target_compile_definitions(v8_libsampler PRIVATE $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(v8_libsampler PRIVATE ${disable-exceptions})
target_include_directories(v8_libsampler PRIVATE ${D})
target_link_libraries(v8_libsampler PRIVATE v8_libbase)

#
# v8_libbase
#

add_library(v8_libbase STATIC
  ${D}/src/base/bits.cc
  ${D}/src/base/bounded-page-allocator.cc
  ${D}/src/base/cpu.cc
  ${D}/src/base/debug/stack_trace.cc
  ${D}/src/base/division-by-constant.cc
  ${D}/src/base/file-utils.cc
  ${D}/src/base/functional.cc
  ${D}/src/base/ieee754.cc
  ${D}/src/base/logging.cc
  ${D}/src/base/once.cc
  ${D}/src/base/page-allocator.cc
  ${D}/src/base/platform/condition-variable.cc
  ${D}/src/base/platform/mutex.cc
  ${D}/src/base/platform/semaphore.cc
  ${D}/src/base/platform/time.cc
  ${D}/src/base/region-allocator.cc
  ${D}/src/base/sys-info.cc
  ${D}/src/base/utils/random-number-generator.cc
  ${D}/src/base/vlq-base64.cc
  $<$<NOT:$<OR:${is-win},${is-aix}>>:${D}/src/base/platform/platform-posix-time.cc>
  $<${is-win}:${D}/src/base/platform/platform-win32.cc>
  $<${is-win}:${D}/src/base/debug/stack_trace_win.cc>
  $<${is-aix}:${D}/src/base/debug/stack_trace_posix.cc>
  $<${is-aix}:${D}/src/base/platform/platform-aix.cc>
  $<${is-darwin}:${D}/src/base/debug/stack_trace_posix.cc>
  $<${is-darwin}:${D}/src/base/platform/platform-macos.cc>
  $<${is-linux}:${D}/src/base/debug/stack_trace_posix.cc>
  $<${is-linux}:${D}/src/base/platform/platform-linux.cc>
  $<${is-posix}:${D}/src/base/platform/platform-posix.cc>
)

set_property(SOURCE ${D}/src/base/utils/random-number-generator.cc
  APPEND PROPERTY COMPILE_DEFINITIONS _CRT_RAND_S)

target_compile_definitions(v8_libbase PRIVATE $<${is-win}:UNICODE> $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(v8_libbase PRIVATE ${disable-exceptions})
target_include_directories(v8_libbase PRIVATE ${D})
target_link_libraries(v8_libbase
  PRIVATE
    Threads::Threads
    $<${is-linux}:${CMAKE_DL_LIBS}>
    $<${is-linux}:rt>
    $<${is-win}:winmm>
    $<${is-win}:dbghelp>
)

#
# bytecode_builtins_list_generator
#
v8_generate_builtins_list(${PROJECT_BINARY_DIR}/generated)

target_include_directories(v8-bytecodes-builtin-list
  INTERFACE
    ${PROJECT_BINARY_DIR}/generated)


add_executable(
  bytecode_builtins_list_generator
  ${D}/src/builtins/generate-bytecodes-builtins-list.cc
  ${D}/src/interpreter/bytecode-operands.cc
  ${D}/src/interpreter/bytecodes.cc
)

target_include_directories(bytecode_builtins_list_generator PRIVATE ${D})
target_link_libraries(bytecode_builtins_list_generator v8_libbase)

#
# v8_torque_generated
#

# Note: torque does not like absolute paths.
file(GLOB_RECURSE torque-builtins
  RELATIVE "${PROJECT_SOURCE_DIR}/v8"
  CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/v8/src/builtins/*.tq")

file(GLOB_RECURSE torque-objects
  RELATIVE "${PROJECT_SOURCE_DIR}/v8"
  CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/v8/src/objects/*.tq")

list(APPEND torque_files ${torque-builtins})
list(APPEND torque_files ${torque-objects})

list(APPEND torque_files
  src/ic/handler-configuration.tq
  src/wasm/wasm-objects.tq
  test/torque/test-torque.tq
  third_party/v8/builtins/array-sort.tq)

if(NOT V8_ENABLE_I18N)
  list(REMOVE_ITEM torque_files src/objects/intl-objects.tq)
endif()

list(TRANSFORM torque_files PREPEND ${D}/ OUTPUT_VARIABLE torque_files_abs)

# Note: target v8_compiler depends on ${torque_outputs}. Maybe this should be
# a source-only dependency.
set(torque_outputs
  ${PROJECT_BINARY_DIR}/torque-generated/interface-descriptors-tq.inc
  ${PROJECT_BINARY_DIR}/torque-generated/class-verifiers-tq.cc
  ${PROJECT_BINARY_DIR}/torque-generated/objects-printer-tq.cc
  ${PROJECT_BINARY_DIR}/torque-generated/class-definitions-tq.cc
  ${PROJECT_BINARY_DIR}/torque-generated/class-debug-readers-tq.cc
  ${PROJECT_BINARY_DIR}/torque-generated/exported-macros-assembler-tq.cc)

list(TRANSFORM torque_files
  REPLACE "[.]tq$" "-tq-csa.cc"
  OUTPUT_VARIABLE torque-outputs)
list(TRANSFORM torque-outputs
  REPLACE "[.]cc$" ".h"
  OUTPUT_VARIABLE torque-headers)
list(APPEND torque-outputs ${torque-headers})
list(TRANSFORM torque-outputs PREPEND "${PROJECT_BINARY_DIR}/torque-generated/")

foreach(filename IN LISTS torque_files)
  get_filename_component(directory ${filename} DIRECTORY)
  list(APPEND torque_dirs ${PROJECT_BINARY_DIR}/torque-generated/${directory})
endforeach()

list(REMOVE_DUPLICATES torque_dirs)

add_library(
  v8_torque_generated STATIC
  ${torque-outputs}
  ${torque_outputs})

target_compile_definitions(v8_torque_generated PRIVATE $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(v8_torque_generated PRIVATE ${disable-exceptions})
target_include_directories(v8_torque_generated
  PUBLIC
    ${PROJECT_BINARY_DIR}
    ${D})
target_link_libraries(v8_torque_generated PRIVATE v8-bytecodes-builtin-list)

add_custom_command(
  COMMAND
    torque
    -o ${PROJECT_BINARY_DIR}/torque-generated
    -v8-root ${D}
    ${torque_files}
  DEPENDS
    torque
    ${torque_dirs}
    ${torque_files_abs}
  OUTPUT
    ${torque-outputs}
    ${torque_outputs}
)

add_custom_command(
  COMMAND
    ${CMAKE_COMMAND} -E make_directory ${torque_dirs}
  OUTPUT
    ${torque_dirs}
)

#
# torque
#
file(GLOB torque-program-sources CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/v8/src/torque/*.cc)

add_executable(torque)
target_sources(torque PRIVATE ${torque-program-sources})
target_compile_options(torque PRIVATE ${enable-exceptions})
target_include_directories(torque PRIVATE ${D})
target_link_libraries(torque PRIVATE v8_libbase)

#
# mksnapshot
#

add_executable(mksnapshot
  ${D}/src/init/setup-isolate-full.cc
  ${D}/src/snapshot/embedded/embedded-empty.cc
  ${D}/src/snapshot/embedded/embedded-file-writer.cc
  ${D}/src/snapshot/embedded/platform-embedded-file-writer-aix.cc
  ${D}/src/snapshot/embedded/platform-embedded-file-writer-base.cc
  ${D}/src/snapshot/embedded/platform-embedded-file-writer-generic.cc
  ${D}/src/snapshot/embedded/platform-embedded-file-writer-mac.cc
  ${D}/src/snapshot/embedded/platform-embedded-file-writer-win.cc
  ${D}/src/snapshot/mksnapshot.cc
  ${D}/src/snapshot/snapshot-empty.cc
)

target_compile_definitions(mksnapshot PRIVATE $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(mksnapshot PRIVATE ${disable-exceptions})
target_include_directories(mksnapshot PRIVATE ${D})
target_link_libraries(mksnapshot
  PRIVATE
    v8_libbase
    v8_libplatform
    v8_base_without_compiler
    v8_compiler
    v8_initializers
    v8-bytecodes-builtin-list
    v8_torque_generated
)

add_library(v8-adler32 OBJECT ${D}/third_party/zlib/adler32.c)
target_include_directories(v8-adler32 PUBLIC $<BUILD_INTERFACE:${D}/third_party/zlib>)
