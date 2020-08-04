#
# From testing/gmock/BUILD.gn
#

set(D ${PROJECT_SOURCE_DIR}/v8//testing/gmock)

add_library(gmock STATIC)
target_sources(gmock
  PRIVATE
  ${D}/include/gmock/gmock-actions.h
  ${D}/include/gmock/gmock-matchers.h
  ${D}/include/gmock/gmock.h
  ${D}/../gtest/empty.cc
  )
target_config(gmock PUBLIC gmock_config gtest_config)
target_include_directories(gmock PUBLIC ${PROJECT_SOURCE_DIR}/v8/third_party/googletest/src/googlemock/include)
target_link_libraries(gmock
  PUBLIC
    googletest_gmock
  )
