#
# From testing/gtest/BUILD.gn
#

set(D ${PROJECT_SOURCE_DIR}/v8/testing/gtest)

set(gtest_direct_config_defines_public UNIT_TEST)

add_library(gtest STATIC)
set(gtest_files
  ${D}/include/gtest/gtest-death-test.h
  ${D}/include/gtest/gtest-message.h
  ${D}/include/gtest/gtest-param-test.h
  ${D}/include/gtest/gtest-spi.h
  ${D}/include/gtest/gtest.h
  ${D}/include/gtest/gtest_prod.h
  ${D}/empty.cc
  )
target_sources(gtest
  PRIVATE
    ${gtest_files}
  )
target_compile_definitions(gtest
  PRIVATE
    ${v8_defines}
  PUBLIC
    ${gtest_direct_config_defines_public}
  )
target_include_directories(gtest
  PUBLIC
  ${PROJECT_SOURCE_DIR}/third_party/googletest/src/googletest/include
  )
target_link_libraries(gtest
  PUBLIC
    googletest_gtest
  )
