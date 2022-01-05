cmake_minimum_required(VERSION 3.4)
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MSVC.cmake")
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
project(V8 LANGUAGES CXX C ASM)

set(USE_PRECOMPILED_HEADERS 0)
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.16.0") 
set(USE_PRECOMPILED_HEADERS 1)
endif()

if(MSVC)
  enable_language(ASM_MASM)
  set_property(SOURCE ${PROJECT_BINARY_DIR}/embedded.S PROPERTY LANGUAGE ASM_MASM)
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
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
set(is-x64 0)
set(is-s390 $<STREQUAL:${CMAKE_SYSTEM_PROCESSOR},S390>)

if ((${CMAKE_SYSTEM_PROCESSOR} STREQUAL AMD64) OR  (${CMAKE_SYSTEM_PROCESSOR} STREQUAL x86_64))
  if (${CMAKE_SIZEOF_VOID_P} STREQUAL 8) 
    set(is-x64 1)
  else()
    set(is-ia32 1)
  endif()
endif()
if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL x86)
  # its not itanium but there are no other 32bit options in the v8 build.. it works though.
  set(is-ia32 1)
endif()

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
  $<${is-ia32}:V8_TARGET_ARCH_IA32>
  $<${is-ia32}:DISABLE_UNTRUSTED_CODE_MITIGATIONS> # lest we get FATAL("Poisoning is not supported on ia32.");
  $<${is-win}:NOMINMAX>
  # $<$<AND:${is-win},${is-x64}>:V8_OS_WIN_X64> # This is set by v8 sources - should not be needed here.
  $<$<BOOL:${V8_ENABLE_CONCURRENT_MARKING}>:V8_CONCURRENT_MARKING>
  $<${is-win}:V8_OS_WIN32>
)

set(msvc-x86-no-safeseh)
if (${is-ia32}) 
  if (MSVC) 
    set(msvc-x86-no-safeseh /SAFESEH:NO)
  endif()
  if (${is-win})
      # From Build.GN: Ensure no surprising artifacts from 80bit double math with x86.
      if (MSVC) 
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:SSE2")
      else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2")
      endif()
  endif()
endif()

set(disable-exceptions
  $<$<CXX_COMPILER_ID:MSVC>:/EHs-c->
  $<$<CXX_COMPILER_ID:AppleClang>:-fno-exceptions>
  $<$<CXX_COMPILER_ID:Clang>:-fno-exceptions>
  $<$<CXX_COMPILER_ID:GNU>:-fno-exceptions>
)

set(enable-exceptions
  $<$<CXX_COMPILER_ID:MSVC>:/EHsc>
  $<$<CXX_COMPILER_ID:AppleClang>:-fexceptions>
  $<$<CXX_COMPILER_ID:Clang>:-fexceptions>
  $<$<CXX_COMPILER_ID:GNU>:-fexceptions>
)

set(has_control_flow_integrity ${is-arm64})

#
# d8
#

add_executable(
  d8
  $<${is-posix}:v8/src/d8/d8-posix.cc>
  $<${is-win}:v8/src/d8/d8-windows.cc>
  v8/src/d8/async-hooks-wrapper.cc
  v8/src/d8/d8-console.cc
  v8/src/d8/d8-js.cc
  v8/src/d8/d8-platforms.cc
  v8/src/d8/d8.cc
)

target_compile_definitions(d8 PRIVATE $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(d8 PRIVATE ${disable-exceptions})
target_link_options(d8 PRIVATE ${msvc-x86-no-safeseh})

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
    v8_libplatform
)

# more granular library support
add_library(v8-i18n-support OBJECT)
set_property(TARGET v8-i18n-support PROPERTY EXCLUDE_FROM_ALL ON)

list(APPEND i18n-sources
  v8/src/builtins/builtins-intl.cc
  v8/src/objects/intl-objects.cc
  v8/src/objects/js-break-iterator.cc
  v8/src/objects/js-collator.cc
  v8/src/objects/js-date-time-format.cc
  v8/src/objects/js-display-names.cc
  v8/src/objects/js-list-format.cc
  v8/src/objects/js-locale.cc
  v8/src/objects/js-number-format.cc
  v8/src/objects/js-plural-rules.cc
  v8/src/objects/js-relative-time-format.cc
  v8/src/objects/js-segment-iterator.cc
  v8/src/objects/js-segmenter.cc
  v8/src/objects/js-segments.cc
  v8/src/runtime/runtime-intl.cc
  v8/src/strings/char-predicates.cc
)

target_sources(v8-i18n-support PRIVATE ${i18n-sources})
target_compile_definitions(v8-i18n-support PRIVATE $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(v8-i18n-support PRIVATE ${disable-exceptions})
target_link_libraries(v8-i18n-support PRIVATE v8_torque_generated)

target_include_directories(v8-i18n-support
  PRIVATE
    ${PROJECT_SOURCE_DIR}/v8
    ${PROJECT_SOURCE_DIR}/v8/src/objects
    ${PROJECT_SOURCE_DIR}/v8/include
)

file(GLOB api-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/api/*.cc)
file(GLOB asmjs-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/asmjs/*.cc)
file(GLOB ast-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/ast/*.cc)
file(GLOB builtin-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/builtins/*.cc)
file(GLOB codegen-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/codegen/*.cc)
file(GLOB debug-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/debug/*.cc)
file(GLOB diagnostic-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/diagnostics/*.cc)
file(GLOB execution-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/execution/*.cc)
file(GLOB extensions-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/extensions/*.cc)
file(GLOB handles-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/handles/*.cc)
file(GLOB interpreter-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/interpreter/*.cc)
file(GLOB json-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/json/*.cc)
file(GLOB logging-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/logging/*.cc)
file(GLOB numbers-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/numbers/*.cc)
file(GLOB object-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/objects/*.cc)
file(GLOB parsing-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/parsing/*.cc)
file(GLOB profiler-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/profiler/*.cc)
file(GLOB regexp-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/regexp/*.cc v8/src/regexp/experimental/*.cc)
file(GLOB runtime-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/runtime/*.cc)
file(GLOB snapshot-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/snapshot/*.cc)
file(GLOB strings-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/strings/*.cc)
file(GLOB utils-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/utils/*.cc)
file(GLOB wasm-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/wasm/*.cc)
file(GLOB wasm-baseline-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/wasm/baseline/*.cc)
file(GLOB zone-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS v8/src/zone/*.cc)

file(GLOB heap-sources RELATIVE ${PROJECT_SOURCE_DIR} CONFIGURE_DEPENDS
  v8/src/heap/*.cc
  v8/src/heap/base/*.cc
  v8/src/heap/third-party/*.cc
)

set(deoptimizer-sources
  "v8/src/deoptimizer/deoptimizer.cc"
  "v8/src/deoptimizer/deoptimize-reason.cc"
  $<${has_control_flow_integrity}:"v8/src/deoptimizer/deoptimizer-cfi-builtins.cc">
  $<${has_control_flow_integrity}:"v8/src/deoptimizer/deoptimizer-cfi-empty.cc">
)

list(REMOVE_ITEM builtin-sources
  v8/src/builtins/builtins-intl-gen.cc
  v8/src/builtins/builtins-intl.cc
)

list(REMOVE_ITEM diagnostic-sources v8/src/diagnostics/unwinding-info-win64.cc)
list(REMOVE_ITEM heap-sources v8/src/heap/conservative-stack-visitor.cc)
list(REMOVE_ITEM object-sources ${i18n-sources})
list(REMOVE_ITEM regexp-sources v8/src/regexp/gen-regexp-special-case.cc)
list(REMOVE_ITEM runtime-sources v8/src/runtime/runtime-intl.cc)
list(REMOVE_ITEM snapshot-sources
  v8/src/snapshot/mksnapshot.cc
  v8/src/snapshot/snapshot-empty.cc
  v8/src/snapshot/snapshot-external.cc
)
list(REMOVE_ITEM strings-sources v8/src/strings/char-predicates.cc)

list(APPEND snapshot-sources v8/src/snapshot/embedded/embedded-data.cc)
list(APPEND wasm-sources ${wasm-baseline-sources})

add_library(v8_base_without_compiler STATIC
  $<$<AND:${is-x64},${is-posix}>:v8/src/trap-handler/handler-inside-posix.cc>
  $<$<AND:${is-x64},${is-posix}>:v8/src/trap-handler/handler-outside-posix.cc>
  $<$<AND:${is-x64},${is-win}>:v8/src/diagnostics/unwinding-info-win64.cc>
  $<$<AND:${is-x64},${is-win}>:v8/src/trap-handler/handler-inside-win.cc>
  $<$<AND:${is-x64},${is-win}>:v8/src/trap-handler/handler-outside-win.cc>
  $<${is-x64}:v8/src/codegen/x64/assembler-x64.cc>
  $<${is-x64}:v8/src/codegen/x64/cpu-x64.cc>
  $<${is-x64}:v8/src/codegen/x64/interface-descriptors-x64.cc>
  $<${is-x64}:v8/src/codegen/x64/macro-assembler-x64.cc>
  $<${is-x64}:v8/src/compiler/backend/x64/code-generator-x64.cc>
  $<${is-x64}:v8/src/compiler/backend/x64/instruction-scheduler-x64.cc>
  $<${is-x64}:v8/src/compiler/backend/x64/instruction-selector-x64.cc>
  $<${is-x64}:v8/src/compiler/backend/x64/unwinding-info-writer-x64.cc>
  $<${is-x64}:v8/src/debug/x64/debug-x64.cc>
  $<${is-x64}:v8/src/deoptimizer/x64/deoptimizer-x64.cc>
  $<${is-x64}:v8/src/diagnostics/x64/disasm-x64.cc>
  $<${is-x64}:v8/src/diagnostics/x64/eh-frame-x64.cc>
  $<${is-x64}:v8/src/execution/x64/frame-constants-x64.cc>
  $<${is-x64}:v8/src/regexp/x64/regexp-macro-assembler-x64.cc>
  $<${is-ia32}:v8/src/codegen/ia32/assembler-ia32.cc>
  $<${is-ia32}:v8/src/codegen/ia32/cpu-ia32.cc>
  $<${is-ia32}:v8/src/codegen/ia32/interface-descriptors-ia32.cc>
  $<${is-ia32}:v8/src/codegen/ia32/macro-assembler-ia32.cc>
  $<${is-ia32}:v8/src/compiler/backend/ia32/code-generator-ia32.cc>
  $<${is-ia32}:v8/src/compiler/backend/ia32/instruction-scheduler-ia32.cc>
  $<${is-ia32}:v8/src/compiler/backend/ia32/instruction-selector-ia32.cc>
  $<${is-ia32}:v8/src/debug/ia32/debug-ia32.cc>
  $<${is-ia32}:v8/src/deoptimizer/ia32/deoptimizer-ia32.cc>
  $<${is-ia32}:v8/src/diagnostics/ia32/disasm-ia32.cc>
  $<${is-ia32}:v8/src/execution/ia32/frame-constants-ia32.cc>
  $<${is-ia32}:v8/src/regexp/ia32/regexp-macro-assembler-ia32.cc>
  $<$<BOOL:${V8_ENABLE_I18N}>:$<TARGET_OBJECTS:v8-i18n-support>>
  ${api-sources}
  ${asmjs-sources}
  ${ast-sources}
  ${builtin-sources}
  ${codegen-sources}
  v8/src/common/assert-scope.cc
  v8/src/compiler-dispatcher/compiler-dispatcher.cc
  v8/src/compiler-dispatcher/optimizing-compile-dispatcher.cc
  v8/src/date/date.cc
  v8/src/date/dateparser.cc
  ${debug-sources}
  ${deoptimizer-sources}
  ${diagnostic-sources}
  ${execution-sources}
  ${extensions-sources}
  v8/src/flags/flags.cc
  v8/src/handles/global-handles.cc
  v8/src/handles/handles.cc
  ${heap-sources}
  v8/src/ic/call-optimization.cc
  v8/src/ic/handler-configuration.cc
  v8/src/ic/ic-stats.cc
  v8/src/ic/ic.cc
  v8/src/ic/stub-cache.cc
  v8/src/init/bootstrapper.cc
  v8/src/init/icu_util.cc
  v8/src/init/isolate-allocator.cc
  v8/src/init/startup-data-util.cc
  v8/src/init/v8.cc
  ${handles-sources}
  ${interpreter-sources}
  ${json-sources}
  ${logging-sources}
  ${numbers-sources}
  ${object-sources}
  ${parsing-sources}
  ${profiler-sources}
  ${regexp-sources}
  v8/src/roots/roots.cc
  ${runtime-sources}
  v8/src/sanitizer/lsan-page-allocator.cc
  ${snapshot-sources}
  ${strings-sources}
  v8/src/tasks/cancelable-task.cc
  v8/src/tasks/task-utils.cc
  v8/src/third_party/siphash/halfsiphash.cc
  v8/src/tracing/trace-event.cc
  v8/src/tracing/traced-value.cc
  v8/src/tracing/tracing-category-observer.cc
  v8/src/trap-handler/handler-inside.cc
  v8/src/trap-handler/handler-outside.cc
  v8/src/trap-handler/handler-shared.cc
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

#if using preompiled headers, _WIN32_WINNT needs to be set globally or compilation fails.
set(v8_define_winnt)
if (WIN32)
set(v8_define_winnt -DUNICODE -D_WIN32_WINNT=${windows-version})
endif()

set_property(SOURCE v8/src/diagnostics/unwinding-info-win64.cc
  APPEND PROPERTY
    COMPILE_DEFINITIONS
      UNICODE
      _WIN32_WINNT=${windows-version}
)

target_compile_definitions(v8_base_without_compiler PRIVATE ${v8_defines} ${v8_define_winnt} $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(v8_base_without_compiler PRIVATE ${disable-exceptions})

target_include_directories(v8_base_without_compiler
  PRIVATE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/v8/include>
    $<BUILD_INTERFACE:$<TARGET_PROPERTY:v8-bytecodes-builtin-list,INTERFACE_INCLUDE_DIRECTORIES>>
    ${PROJECT_SOURCE_DIR}/v8
    ${PROJECT_SOURCE_DIR}/v8/third_party/zlib
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
file(GLOB_RECURSE compiler-sources
  RELATIVE ${PROJECT_SOURCE_DIR}
  CONFIGURE_DEPENDS v8/src/compiler/*.cc
)
list(FILTER compiler-sources EXCLUDE REGEX ".*backend/.*/.*[.]cc$")

add_library(v8_compiler STATIC)
target_sources(v8_compiler PRIVATE ${compiler-sources})
target_compile_definitions(v8_compiler PRIVATE ${v8_defines} $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(v8_compiler PRIVATE ${disable-exceptions})

target_include_directories(v8_compiler
  PUBLIC
    ${PROJECT_BINARY_DIR}
  PRIVATE
    ${PROJECT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/v8
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
  $<${is-arm64}:v8/src/builtins/arm64/builtins-arm64.cc>
  $<${is-arm}:v8/src/builtins/arm/builtins-arm.cc>
  $<${is-ia32}:v8/src/builtins/ia32/builtins-ia32.cc>
  $<${is-mips64}:v8/src/builtins/mips64/builtins-mips64.cc>
  $<${is-mips}:v8/src/builtins/mips/builtins-mips.cc>
  $<${is-ppc}:v8/src/builtins/ppc/builtins-ppc.cc>
  $<${is-s390}:v8/src/builtins/s390/builtins-s390.cc>
  $<${is-x64}:v8/src/builtins/x64/builtins-x64.cc>
  $<$<BOOL:${V8_ENABLE_I18N}>:v8/src/builtins/builtins-intl-gen.cc>
  v8/src/builtins/builtins-array-gen.cc
  v8/src/builtins/builtins-async-function-gen.cc
  v8/src/builtins/builtins-async-gen.cc
  v8/src/builtins/builtins-async-generator-gen.cc
  v8/src/builtins/builtins-async-iterator-gen.cc
  v8/src/builtins/builtins-bigint-gen.cc
  v8/src/builtins/builtins-call-gen.cc
  v8/src/builtins/builtins-collections-gen.cc
  v8/src/builtins/builtins-constructor-gen.cc
  v8/src/builtins/builtins-conversion-gen.cc
  v8/src/builtins/builtins-date-gen.cc
  v8/src/builtins/builtins-debug-gen.cc
  v8/src/builtins/builtins-generator-gen.cc
  v8/src/builtins/builtins-global-gen.cc
  v8/src/builtins/builtins-handler-gen.cc
  v8/src/builtins/builtins-ic-gen.cc
  v8/src/builtins/builtins-internal-gen.cc
  v8/src/builtins/builtins-interpreter-gen.cc
  v8/src/builtins/builtins-iterator-gen.cc
  v8/src/builtins/builtins-lazy-gen.cc
  v8/src/builtins/builtins-microtask-queue-gen.cc
  v8/src/builtins/builtins-number-gen.cc
  v8/src/builtins/builtins-object-gen.cc
  v8/src/builtins/builtins-promise-gen.cc
  v8/src/builtins/builtins-proxy-gen.cc
  v8/src/builtins/builtins-regexp-gen.cc
  v8/src/builtins/builtins-sharedarraybuffer-gen.cc
  v8/src/builtins/builtins-string-gen.cc
  v8/src/builtins/builtins-typed-array-gen.cc
  v8/src/builtins/builtins-wasm-gen.cc
  v8/src/builtins/growable-fixed-array-gen.cc
  v8/src/builtins/setup-builtins-internal.cc
  v8/src/codegen/code-stub-assembler.cc
  v8/src/heap/setup-heap-internal.cc
  v8/src/ic/accessor-assembler.cc
  v8/src/ic/binary-op-assembler.cc
  v8/src/ic/keyed-store-generic.cc
  v8/src/ic/unary-op-assembler.cc
  v8/src/interpreter/interpreter-assembler.cc
  v8/src/interpreter/interpreter-generator.cc
  v8/src/interpreter/interpreter-intrinsics-generator.cc
)

target_compile_definitions(v8_initializers PRIVATE ${v8_defines} $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(v8_initializers PRIVATE ${disable-exceptions})

target_include_directories(v8_initializers
  PRIVATE
    ${PROJECT_SOURCE_DIR}/v8
    ${PROJECT_BINARY_DIR}
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
  v8/src/init/setup-isolate-deserialize.cc
)

target_compile_definitions(v8_snapshot PRIVATE $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(v8_snapshot PRIVATE ${disable-exceptions})
target_include_directories(v8_snapshot PRIVATE ${PROJECT_SOURCE_DIR}/v8)

target_link_libraries(v8_snapshot
  PRIVATE
    v8_torque_generated
    v8-bytecodes-builtin-list
)

# Note: allow passing in v8_random_seed
add_custom_command(
  COMMAND
    mksnapshot
    --embedded_src ${PROJECT_BINARY_DIR}/embedded.S
    --startup_src ${PROJECT_BINARY_DIR}/snapshot.cc
    $<${is-x64}:--target_arch=x64>
    $<$<PLATFORM_ID:Darwin>:--target_os=mac>
    $<$<PLATFORM_ID:Linux>:--target_os=linux>
    $<$<PLATFORM_ID:Windows>:--target_os=win>
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

file(GLOB inspector-sources
  RELATIVE ${PROJECT_SOURCE_DIR}
  CONFIGURE_DEPENDS v8/src/inspector/*.cc
)

file(GLOB inspector-protocol-sources
  RELATIVE ${PROJECT_SOURCE_DIR}
  CONFIGURE_DEPENDS v8/third_party/inspector_protocol/crdtp/*.cc
)

list(FILTER inspector-protocol-sources EXCLUDE REGEX "_test[.]cc$")
list(FILTER inspector-protocol-sources EXCLUDE REGEX "crdtp/test_.*[.]cc$")

add_library(v8_inspector STATIC
  ${inspector-protocol-sources}
  ${inspector-sources}
  ${inspector_files}
)

target_compile_features(v8_inspector PUBLIC cxx_std_17)
target_compile_options(v8_inspector PRIVATE ${disable-exceptions})
target_compile_definitions(v8_inspector PUBLIC $<${is-msvc}:_HAS_EXCEPTIONS=0>)

target_include_directories(v8_inspector
  PRIVATE
    ${PROJECT_BINARY_DIR}/inspector
    ${PROJECT_SOURCE_DIR}/v8
    ${PROJECT_SOURCE_DIR}/v8/include
    ${PROJECT_SOURCE_DIR}/v8/third_party/googletest/src/googlemock/include
    ${PROJECT_SOURCE_DIR}/v8/third_party/googletest/src/googletest/include
)

target_link_libraries(v8_inspector PRIVATE v8_torque_generated)

file(GLOB inspector-templates
  RELATIVE ${PROJECT_SOURCE_DIR}
  CONFIGURE_DEPENDS
    v8/third_party/inspector_protocol/lib/*.template
    v8/third_party/inspector_protocol/templates/*.template
)

add_custom_command(
  COMMAND ${CMAKE_COMMAND} -E make_directory ${inspector_dirs}
  OUTPUT ${inspector_dirs}
)

add_custom_command(
  COMMAND
    Python::Interpreter
    v8/third_party/inspector_protocol/code_generator.py
    --config v8/src/inspector/inspector_protocol_config.json
    --jinja_dir v8/third_party
    --output_base ${PROJECT_BINARY_DIR}/inspector/src/inspector
    --inspector_protocol_dir third_party/inspector_protocol
  WORKING_DIRECTORY
    ${PROJECT_SOURCE_DIR}
  DEPENDS
    v8/third_party/inspector_protocol/code_generator.py
    v8/src/inspector/inspector_protocol_config.json
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
    v8/src/libplatform/default-foreground-task-runner.cc
    v8/src/libplatform/default-job.cc
    v8/src/libplatform/default-platform.cc
    v8/src/libplatform/default-worker-threads-task-runner.cc
    v8/src/libplatform/delayed-task-queue.cc
    v8/src/libplatform/task-queue.cc
    v8/src/libplatform/tracing/trace-buffer.cc
    v8/src/libplatform/tracing/trace-config.cc
    v8/src/libplatform/tracing/trace-object.cc
    v8/src/libplatform/tracing/trace-writer.cc
    v8/src/libplatform/tracing/tracing-controller.cc
    v8/src/libplatform/worker-thread.cc
)

target_compile_definitions(v8_libplatform PRIVATE $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(v8_libplatform PRIVATE ${disable-exceptions})
target_include_directories(v8_libplatform
  PUBLIC
    ${PROJECT_SOURCE_DIR}/v8/include
  PRIVATE
    ${PROJECT_SOURCE_DIR}/v8
)

target_link_libraries(v8_libplatform PRIVATE v8_libbase)

#
# v8_libsampler
#

add_library(v8_libsampler STATIC v8/src/libsampler/sampler.cc)

target_include_directories(v8_libsampler
  PRIVATE
    ${PROJECT_SOURCE_DIR}/v8
    ${PROJECT_SOURCE_DIR}/v8/include
)

target_compile_definitions(v8_libsampler PRIVATE $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(v8_libsampler PRIVATE ${disable-exceptions})
target_include_directories(v8_libsampler PRIVATE ${PROJECT_SOURCE_DIR}/v8)
target_link_libraries(v8_libsampler PRIVATE v8_libbase)

#
# v8_libbase
#

add_library(v8_libbase STATIC
  v8/src/base/bits.cc
  v8/src/base/bounded-page-allocator.cc
  v8/src/base/cpu.cc
  v8/src/base/debug/stack_trace.cc
  v8/src/base/division-by-constant.cc
  v8/src/base/file-utils.cc
  v8/src/base/functional.cc
  v8/src/base/ieee754.cc
  v8/src/base/logging.cc
  v8/src/base/once.cc
  v8/src/base/page-allocator.cc
  v8/src/base/platform/condition-variable.cc
  v8/src/base/platform/mutex.cc
  v8/src/base/platform/semaphore.cc
  v8/src/base/platform/time.cc
  v8/src/base/region-allocator.cc
  v8/src/base/sys-info.cc
  v8/src/base/utils/random-number-generator.cc
  v8/src/base/vlq-base64.cc
  $<$<NOT:$<OR:${is-win},${is-aix}>>:v8/src/base/platform/platform-posix-time.cc>
  $<${is-win}:v8/src/base/platform/platform-win32.cc>
  $<${is-win}:v8/src/base/debug/stack_trace_win.cc>
  $<${is-aix}:v8/src/base/debug/stack_trace_posix.cc>
  $<${is-aix}:v8/src/base/platform/platform-aix.cc>
  $<${is-darwin}:v8/src/base/debug/stack_trace_posix.cc>
  $<${is-darwin}:v8/src/base/platform/platform-macos.cc>
  $<${is-linux}:v8/src/base/debug/stack_trace_posix.cc>
  $<${is-linux}:v8/src/base/platform/platform-linux.cc>
  $<${is-posix}:v8/src/base/platform/platform-posix.cc>
)

# previously, _CRT_RAND_S was defined only for random-number-generator.cc
# This prevents precompiled headers working well - so apply globally instead.
target_compile_definitions(v8_libbase PRIVATE $<${is-win}:UNICODE> $<${is-msvc}:_HAS_EXCEPTIONS=0> _CRT_RAND_S)
target_compile_options(v8_libbase PRIVATE ${disable-exceptions})
target_include_directories(v8_libbase PRIVATE ${PROJECT_SOURCE_DIR}/v8)
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
    ${PROJECT_BINARY_DIR}/generated
)

add_executable(
  bytecode_builtins_list_generator
  v8/src/builtins/generate-bytecodes-builtins-list.cc
  v8/src/interpreter/bytecode-operands.cc
  v8/src/interpreter/bytecodes.cc
)

target_include_directories(bytecode_builtins_list_generator
  PRIVATE
    ${PROJECT_SOURCE_DIR}/v8
)

target_link_libraries(bytecode_builtins_list_generator v8_libbase)

#
# v8_torque_generated
#

# Note: torque does not like absolute paths.
file(GLOB_RECURSE torque-builtins
  RELATIVE ${PROJECT_SOURCE_DIR}/v8
  CONFIGURE_DEPENDS v8/src/builtins/*.tq
)

file(GLOB_RECURSE torque-objects
  RELATIVE ${PROJECT_SOURCE_DIR}/v8
  CONFIGURE_DEPENDS v8/src/objects/*.tq
)

list(APPEND torque_files ${torque-builtins})
list(APPEND torque_files ${torque-objects})

list(APPEND torque_files
  src/ic/handler-configuration.tq
  src/wasm/wasm-objects.tq
  test/torque/test-torque.tq
  third_party/v8/builtins/array-sort.tq
)

if(NOT V8_ENABLE_I18N)
  list(REMOVE_ITEM torque_files src/objects/intl-objects.tq)
endif()

list(TRANSFORM torque_files PREPEND v8/ OUTPUT_VARIABLE torque_files_abs)

# Note: target v8_compiler depends on ${torque_outputs}. Maybe this should be
# a source-only dependency.
set(torque_outputs
  ${PROJECT_BINARY_DIR}/torque-generated/class-debug-readers-tq.cc
  ${PROJECT_BINARY_DIR}/torque-generated/class-debug-readers.cc
  ${PROJECT_BINARY_DIR}/torque-generated/class-definitions-tq.cc
  ${PROJECT_BINARY_DIR}/torque-generated/class-definitions.cc
  ${PROJECT_BINARY_DIR}/torque-generated/class-verifiers-tq.cc
  ${PROJECT_BINARY_DIR}/torque-generated/class-verifiers.cc
  ${PROJECT_BINARY_DIR}/torque-generated/exported-macros-assembler-tq.cc
  ${PROJECT_BINARY_DIR}/torque-generated/exported-macros-assembler.cc
  ${PROJECT_BINARY_DIR}/torque-generated/factory-tq.cc
  ${PROJECT_BINARY_DIR}/torque-generated/factory-tq.inc
  ${PROJECT_BINARY_DIR}/torque-generated/factory.cc
  ${PROJECT_BINARY_DIR}/torque-generated/factory.inc
  ${PROJECT_BINARY_DIR}/torque-generated/interface-descriptors-tq.inc
  ${PROJECT_BINARY_DIR}/torque-generated/interface-descriptors.inc
  ${PROJECT_BINARY_DIR}/torque-generated/objects-body-descriptors-inl.inc
  ${PROJECT_BINARY_DIR}/torque-generated/objects-body-descriptors-tq-inl.inc
  ${PROJECT_BINARY_DIR}/torque-generated/objects-printer-tq.cc
  ${PROJECT_BINARY_DIR}/torque-generated/objects-printer.cc
)

list(TRANSFORM torque_files
  REPLACE "[.]tq$" "-tq-csa.cc"
  OUTPUT_VARIABLE torque-outputs
)

list(TRANSFORM torque-outputs
  REPLACE "[.]cc$" ".h"
  OUTPUT_VARIABLE torque-headers
)

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
  ${torque_outputs}
)

target_compile_definitions(v8_torque_generated PRIVATE $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(v8_torque_generated PRIVATE ${disable-exceptions})
target_link_libraries(v8_torque_generated PRIVATE v8-bytecodes-builtin-list)

target_include_directories(v8_torque_generated
  PUBLIC
    ${PROJECT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/v8/include
    ${PROJECT_SOURCE_DIR}/v8
)

add_custom_command(
  COMMAND
    torque
    -o ${PROJECT_BINARY_DIR}/torque-generated
    -v8-root ${PROJECT_SOURCE_DIR}/v8
    ${torque_files}
  COMMAND
    ${CMAKE_COMMAND} -E touch ${torque-outputs} ${torque_outputs}
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
file(GLOB torque-program-sources
  RELATIVE ${PROJECT_SOURCE_DIR}
  CONFIGURE_DEPENDS v8/src/torque/*.cc
)

add_executable(torque)
target_sources(torque PRIVATE ${torque-program-sources})
target_compile_options(torque PRIVATE ${enable-exceptions})
target_include_directories(torque PRIVATE ${PROJECT_SOURCE_DIR}/v8)
target_link_libraries(torque PRIVATE v8_libbase)

#
# mksnapshot
#

add_executable(mksnapshot
  v8/src/init/setup-isolate-full.cc
  v8/src/snapshot/embedded/embedded-empty.cc
  v8/src/snapshot/embedded/embedded-file-writer.cc
  v8/src/snapshot/embedded/platform-embedded-file-writer-aix.cc
  v8/src/snapshot/embedded/platform-embedded-file-writer-base.cc
  v8/src/snapshot/embedded/platform-embedded-file-writer-generic.cc
  v8/src/snapshot/embedded/platform-embedded-file-writer-mac.cc
  v8/src/snapshot/embedded/platform-embedded-file-writer-win.cc
  v8/src/snapshot/mksnapshot.cc
  v8/src/snapshot/snapshot-empty.cc
)

target_compile_definitions(mksnapshot PRIVATE $<${is-msvc}:_HAS_EXCEPTIONS=0>)
target_compile_options(mksnapshot PRIVATE ${disable-exceptions})
target_link_options(mksnapshot PRIVATE ${msvc-x86-no-safeseh})
target_include_directories(mksnapshot PRIVATE ${PROJECT_SOURCE_DIR}/v8)
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

add_library(v8-adler32 OBJECT v8/third_party/zlib/adler32.c)
target_include_directories(v8-adler32
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/v8/third_party/zlib>
)


# Precompiled headers to reduce lengthy compile times, where available (e.g. MSVC)
if(${USE_PRECOMPILED_HEADERS}) 
  # cant include everything in base
  set(H_BASE 
    "include/precomp_stl.h"
    "v8/src/base/compiler-specific.h"
    "v8/src/base/memory.h"
    "v8/src/base/build_config.h"
    "v8/src/base/atomicops.h"
    "v8/src/base/base-export.h"
    "v8/src/base/bits.h"
    "v8/src/base/bounds.h"
    "v8/src/base/bounded-page-allocator.h"
    "v8/src/base/file-utils.h"
    "v8/src/base/flags.h"
    "v8/src/base/functional.h"
    "v8/src/base/lazy-instance.h"
    "v8/src/base/v8-fallthrough.h"
  )
  file(GLOB H_API "v8/src/api/*.h")
  file(GLOB H_BUILTINS "v8/src/builtins/*.h")
  file(GLOB H_COMMON "v8/src/common/*.h")
  file(GLOB H_COMPILER "v8/src/compiler/*.h")
  file(GLOB H_FLAGS "v8/src/flags/*.h")
  file(GLOB H_HEAP "v8/src/heap/*.h")
  file(GLOB H_LOGGING "v8/src/logging/*.h")
  file(GLOB H_REGEXP "v8/src/regexp/*.h")
  file(GLOB H_RUNTIME "v8/src/runtime/*.h")
  file(GLOB H_STRINGS "v8/src/strings/*.h")
  file(GLOB H_UTILS "v8/src/utils/*.h")
  file(GLOB H_WASM "v8/src/wasm/*.h")

  # Can't GLOB this one..
  set(H_OBJECTS 
    "v8/src/objects/compilation-cache-inl.h"
    "v8/src/objects/elements.h"
    "v8/src/objects/heap-number-inl.h"
    "v8/src/objects/objects.h"
    "v8/src/objects/objects-inl.h"
    "v8/src/objects/slots.h"    
    "v8/src/objects/smi.h"
    "v8/src/objects/string.h"
    "v8/src/objects/visitors.h"
  )
  set(H_PRECOMP 
    ${H_BASE}
    ${H_BUILTINS}
    ${H_API}
    ${H_COMMON}
    ${H_COMPILER}
    ${H_HEAP}
    ${H_LOGGING}
    ${H_OBJECTS}
    ${H_REGEXP}
    ${H_RUNTIME}
    ${H_STRINGS}
    ${H_UTILS}
    ${H_WASM}
    )
  target_precompile_headers(v8_libplatform PRIVATE 
    ${H_BASE}
    "v8/src/libplatform/default-foreground-task-runner.h"
    "v8/src/libplatform/default-job.h"
    "v8/src/libplatform/default-worker-threads-task-runner.h"
  )
  target_precompile_headers(v8_libbase PRIVATE ${H_BASE})
  target_precompile_headers(bytecode_builtins_list_generator PRIVATE ${H_BASE})

  target_precompile_headers(v8_base_without_compiler PRIVATE   
      "v8/include/v8.h"
      "v8/include/v8config.h"
      "v8/src/common/globals.h"
      "v8/src/objects/js-array-buffer.h"
      ${H_BASE}
      ${H_BUILTINS}
      ${H_API}
      ${H_UTILS}
      ${H_WASM}
  )
  target_precompile_headers(v8_initializers PRIVATE ${H_BASE})
  target_precompile_headers(v8_compiler PRIVATE ${H_PRECOMP})
  target_precompile_headers(mksnapshot PRIVATE ${H_PRECOMP})
  
  # Some surprising issues with explicit template instantiations
  # going on for torque build if files in v8/src/torque included in PCH.
  target_precompile_headers(torque
     PRIVATE
     ${H_BASE}
     #${H_COMMON}
  )
  target_precompile_headers(v8_torque_generated PRIVATE ${H_PRECOMP}
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/aggregate-error-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-copywithin-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-every-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-filter-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-find-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-findindex-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-foreach-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-from-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-isarray-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-join-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-lastindexof-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-map-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-of-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-reduce-right-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-reduce-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-reverse-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-shift-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-slice-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-some-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-splice-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-unshift-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/array-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/arraybuffer-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/base-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/bigint-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/boolean-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/builtins-string-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/cast-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/collections-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/console-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/constructor-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/conversion-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/convert-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/data-view-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/finalization-registry-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/frame-arguments-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/frames-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/function-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/growable-fixed-array-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/ic-callable-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/ic-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/internal-coverage-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/internal-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/iterator-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/math-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/number-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/object-fromentries-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/object-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/promise-abstract-operations-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/promise-all-element-closure-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/promise-all-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/promise-any-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/promise-constructor-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/promise-finally-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/promise-jobs-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/promise-misc-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/promise-race-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/promise-reaction-job-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/promise-resolve-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/promise-then-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/proxy-constructor-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/proxy-delete-property-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/proxy-get-property-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/proxy-get-prototype-of-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/proxy-has-property-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/proxy-is-extensible-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/proxy-prevent-extensions-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/proxy-revocable-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/proxy-revoke-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/proxy-set-property-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/proxy-set-prototype-of-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/proxy-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/reflect-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/regexp-exec-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/regexp-match-all-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/regexp-match-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/regexp-replace-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/regexp-search-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/regexp-source-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/regexp-split-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/regexp-test-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/regexp-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/string-endswith-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/string-html-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/string-iterator-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/string-pad-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/string-repeat-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/string-replaceall-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/string-slice-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/string-startswith-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/string-substr-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/string-substring-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/symbol-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/torque-internal-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/typed-array-createtypedarray-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/typed-array-entries-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/typed-array-every-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/typed-array-filter-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/typed-array-find-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/typed-array-findindex-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/typed-array-foreach-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/typed-array-from-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/typed-array-keys-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/typed-array-of-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/typed-array-reduce-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/typed-array-reduceright-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/typed-array-set-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/typed-array-slice-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/typed-array-some-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/typed-array-sort-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/typed-array-subarray-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/typed-array-values-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/typed-array-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/wasm-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/builtins/weak-ref-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/allocation-site-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/api-callbacks-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/arguments-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/cell-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/code-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/contexts-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/data-handler-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/debug-objects-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/descriptor-array-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/embedder-data-array-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/feedback-cell-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/feedback-vector-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/fixed-array-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/foreign-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/free-space-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/heap-number-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/heap-object-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/js-array-buffer-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/js-array-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/js-collection-iterator-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/js-collection-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/js-generator-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/js-objects-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/js-promise-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/js-proxy-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/js-regexp-string-iterator-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/js-regexp-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/js-weak-refs-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/literal-objects-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/map-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/microtask-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/module-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/name-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/oddball-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/ordered-hash-table-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/primitive-heap-object-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/promise-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/property-array-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/property-cell-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/property-descriptor-object-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/prototype-info-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/regexp-match-info-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/scope-info-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/script-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/shared-function-info-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/source-text-module-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/stack-frame-info-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/string-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/struct-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/synthetic-module-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/template-objects-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/objects/template-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/ic/handler-configuration-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/src/wasm/wasm-objects-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/test/torque/test-torque-tq-csa.h"
    "${PROJECT_BINARY_DIR}/torque-generated/third_party/v8/builtins/array-sort-tq-csa.h"
  )
endif()