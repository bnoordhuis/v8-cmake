#
# From third_party/zlib/BUILD.gn
#

set(D ${PROJECT_SOURCE_DIR}/v8/third_party/zlib)

config(zlib_config INCLUDES ${D})
config(zlib_internal_config DEFINES ZLIB_IMPLEMENTATION)

set(use_x86_x64_optimizations $<AND:$<OR:${is-ia32},${is-x64}>,$<NOT:${is-ios}>>)

config(zlib_adler32_simd_config
  DEFINES
    $<${use_x86_x64_optimizations}:ADLER32_SIMD_SSSE3>
    $<$<AND:${use_x86_x64_optimizations},${is-win}>:X86_WINDOWS>
    $<$<AND:${use_x86_x64_optimizations},$<NOT:${is-win}>>:X86_NOT_WINDOWS>
  )

add_library(zlib_adler32_simd OBJECT
  $<${use_x86_x64_optimizations}:
    ${D}/adler32_simd.c
    ${D}/adler32_simd.h
  >
  )
target_compile_options(zlib_adler32_simd
  PRIVATE
    $<$<AND:${use_x86_x64_optimizations},$<OR:$<NOT:${is-win}>,${is-clang}>>:-mssse3>
  )
target_config(zlib_adler32_simd
  PRIVATE zlib_internal_config
  PUBLIC zlib_adler32_simd_config
  )

config(zlib_inflate_chunk_simd_config
  DEFINES
    $<${use_x86_x64_optimizations}:INFLATE_CHUNK_SIMD_SSE2>
    $<$<AND:${use_x86_x64_optimizations},${is-x64}>:INFLATE_CHUNK_READ_64LE>
  )

add_library(zlib_inflate_chunk_simd OBJECT
  $<${use_x86_x64_optimizations}:
    ${D}/contrib/optimizations/chunkcopy.h
    ${D}/contrib/optimizations/inffast_chunk.c
    ${D}/contrib/optimizations/inffast_chunk.h
    ${D}/contrib/optimizations/inflate.c
  >
  )
target_config(zlib_inflate_chunk_simd
  PRIVATE zlib_internal_config
  PUBLIC zlib_inflate_chunk_simd_config
  )
target_include_directories(zlib_inflate_chunk_simd
  PRIVATE
    $<${use_x86_x64_optimizations}:${D}>
  )


config(zlib_crc32_simd_config
  DEFINES $<${use_x86_x64_optimizations}:CRC32_SIMD_SSE42_PCLMUL>
  )

add_library(zlib_crc32_simd OBJECT
  $<${use_x86_x64_optimizations}:
    ${D}/crc32_simd.c
    ${D}/crc32_simd.h
  >
  )
target_config(zlib_crc32_simd PRIVATE zlib_internal_config PUBLIC zlib_crc32_simd_config)
target_compile_options(zlib_crc32_simd
  PRIVATE
    $<$<OR:$<NOT:${is-win}>,${is-clang}>:
      -msse4.2
      -mpclmul
    >
  )

config(zlib_x86_simd_config
  DEFINES 
    $<${use_x86_x64_optimizations}:
      CRC32_SIMD_SSE42_PCLMUL 
      DEFLATE_FILL_WINDOW_SSE2
    >
  )

add_library(zlib_x86_simd OBJECT
  $<${use_x86_x64_optimizations}:
    ${D}/crc_folding.c
    ${D}/fill_window_sse.c
  >
  )
target_config(zlib_x86_simd
  PRIVATE zlib_internal_config
  PUBLIC zlib_x86_simd_config
  )
target_compile_options(zlib_x86_simd
  PRIVATE
    $<$<AND:${use_x86_x64_optimizations},$<OR:$<NOT:${is-win}>,${is-clang}>>:
      -msse4.2
      -mpclmul
    >
  )

config(zlib_warnings
  CFLAGS $<$<AND:${is-clang},${use_x86_x64_optimizations}>:-Wno-incompatible-pointer-types>
  )

add_library(zlib OBJECT
  ${D}/adler32.c
  ${D}/chromeconf.h
  ${D}/compress.c
  ${D}/contrib/optimizations/insert_string.h
  ${D}/cpu_features.c
  ${D}/cpu_features.h
  ${D}/crc32.c
  ${D}/crc32.h
  ${D}/deflate.c
  ${D}/deflate.h
  ${D}/gzclose.c
  ${D}/gzguts.h
  ${D}/gzlib.c
  ${D}/gzread.c
  ${D}/gzwrite.c
  ${D}/infback.c
  ${D}/inffast.c
  ${D}/inffast.h
  ${D}/inffixed.h
  ${D}/inflate.h
  ${D}/inftrees.c
  ${D}/inftrees.h
  ${D}/trees.c
  ${D}/trees.h
  ${D}/uncompr.c
  ${D}/zconf.h
  ${D}/zlib.h
  ${D}/zutil.c
  ${D}/zutil.h
  $<$<NOT:${use_x86_x64_optimizations}>:
    ${D}/inflate.c
  >
  )

target_config(zlib PUBLIC zlib_config PRIVATE zlib_internal_config zlib_warnings)
target_compile_definitions(zlib
  PRIVATE
    $<$<NOT:${use_x86_x64_optimizations}>:CPU_NO_SIMD>
  )
target_link_libraries(zlib
  PRIVATE
    $<${use_x86_x64_optimizations}:
      zlib_adler32_simd
      zlib_inflate_chunk_simd
    >
    $<${use_x86_x64_optimizations}:
      zlib_crc32_simd
    >
  )