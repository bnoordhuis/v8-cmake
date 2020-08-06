#
# From ${D}/third_party/zlib/google/BUILD.gn
#

set(D ${PROJECT_SOURCE_DIR}/v8/third_party/zlib/google)
add_library(compression_utils_portable OBJECT
  ${D}/compression_utils_portable.cc
  ${D}/compression_utils_portable.h
  )
target_link_libraries(compression_utils_portable
  PUBLIC
    zlib)