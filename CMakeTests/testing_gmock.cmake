#
# From testing/gmock/BUILD.gn
#

set(D ${PROJECT_SOURCE_DIR}/v8//testing/gmock)

add_library(gmock STATIC)
set(gmock_files
  ${D}/include/gmock/gmock-actions.h
  ${D}/include/gmock/gmock-matchers.h
  ${D}/include/gmock/gmock.h
  )
target_sources(gmock
  PRIVATE
    ${gmock_files}
    ${D}/../gtest/empty.cc
  )
target_compile_definitions(gmock
  PRIVATE
    ${v8_defines}
  PUBLIC
    ${gtest_direct_config_defines_public}
  )
target_include_directories(gmock
  PRIVATE
    ${v8_includes}
  PUBLIC
    ${PROJECT_SOURCE_DIR}/v8/third_party/googletest/src/googlemock/include
  )
target_link_libraries(gmock
  PUBLIC
  googletest_gmock
  )
