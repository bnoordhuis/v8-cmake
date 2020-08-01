#
# From third_party/googletest/BUILD.gn
#

set(D ${PROJECT_SOURCE_DIR}/v8/third_party/googletest)

set(gtest_config_defines
  GTEST_API_=
  GTEST_HAS_POSIX_RE=0
  GTEST_LANG_CXX11=1
  GTEST_HAS_TR1_TUPLE=0
  )

set(gtest_config_includes
  ${D}/custom
  ${D}/src/googletest/include
  )

if(${is-win})
  set(gtest_config_cflags /wd4800)
endif()

set(gmock_config_includes
  ${D}/src/googlemock/include
  )

add_library(googletest_gtest STATIC)
target_sources(googletest_gtest
  # Note the gmock_custom prefix
  PRIVATE
    ${D}/gmock_custom/gmock/internal/custom/gmock-port.h
    ${D}/src/googletest/include/gtest/gtest-death-test.h
    ${D}/src/googletest/include/gtest/gtest-matchers.h
    ${D}/src/googletest/include/gtest/gtest-message.h
    ${D}/src/googletest/include/gtest/gtest-param-test.h
    ${D}/src/googletest/include/gtest/gtest-printers.h
    ${D}/src/googletest/include/gtest/gtest-spi.h
    ${D}/src/googletest/include/gtest/gtest-test-part.h
    ${D}/src/googletest/include/gtest/gtest-typed-test.h
    ${D}/src/googletest/include/gtest/gtest.h
    ${D}/src/googletest/include/gtest/gtest_pred_impl.h
    ${D}/src/googletest/include/gtest/internal/gtest-death-test-internal.h
    ${D}/src/googletest/include/gtest/internal/gtest-filepath.h
    ${D}/src/googletest/include/gtest/internal/gtest-internal.h
    #${D}/src/googletest/include/gtest/internal/gtest-linked_ptr.h
    #${D}/src/googletest/include/gtest/internal/gtest-param-util-generated.h
    ${D}/src/googletest/include/gtest/internal/gtest-param-util.h
    ${D}/src/googletest/include/gtest/internal/gtest-port.h
    ${D}/src/googletest/include/gtest/internal/gtest-string.h
    #${D}/src/googletest/include/gtest/internal/gtest-tuple.h
    ${D}/src/googletest/include/gtest/internal/gtest-type-util.h

    #${D}/src/googletest/src/gtest-all.cc  # Not needed by our build.
    ${D}/src/googletest/src/gtest-death-test.cc
    ${D}/src/googletest/src/gtest-filepath.cc
    ${D}/src/googletest/src/gtest-internal-inl.h
    ${D}/src/googletest/src/gtest-matchers.cc
    ${D}/src/googletest/src/gtest-port.cc
    ${D}/src/googletest/src/gtest-printers.cc
    ${D}/src/googletest/src/gtest-test-part.cc
    ${D}/src/googletest/src/gtest-typed-test.cc
    ${D}/src/googletest/src/gtest.cc
  )

target_compile_definitions(googletest_gtest
  PRIVATE
    ${v8_defines}
    ${disable-exceptions-defines}
    ${gtest_config_defines}
  )

target_include_directories(googletest_gtest
  PRIVATE
    ${v8_includes}
    ${gtest_config_includes}
    ${D}/src/googletest
  )

add_library(googletest_gmock STATIC)
target_sources(googletest_gmock
  PRIVATE
    ${D}/src/googlemock/include/gmock/gmock-actions.h
    ${D}/src/googlemock/include/gmock/gmock-cardinalities.h
    ${D}/src/googlemock/include/gmock/gmock-function-mocker.h
    ${D}/src/googlemock/include/gmock/gmock-generated-actions.h
    #${D}/src/googlemock/include/gmock/gmock-generated-nice-strict.h
    ${D}/src/googlemock/include/gmock/gmock-matchers.h
    ${D}/src/googlemock/include/gmock/gmock-more-actions.h
    ${D}/src/googlemock/include/gmock/gmock-more-matchers.h
    ${D}/src/googlemock/include/gmock/gmock-nice-strict.h
    ${D}/src/googlemock/include/gmock/gmock-spec-builders.h
    ${D}/src/googlemock/include/gmock/gmock.h
    #${D}/src/googlemock/include/gmock/internal/gmock-generated-internal-utils.h
    ${D}/src/googlemock/include/gmock/internal/gmock-internal-utils.h
    ${D}/src/googlemock/include/gmock/internal/gmock-port.h
    ${D}/src/googlemock/include/gmock/internal/gmock-pp.h

    # gmock helpers.
    ${D}/gmock_custom/gmock/internal/custom/gmock-port.h

    #${D}/src/googlemock/src/gmock-all.cc  # Not needed by our build.
    ${D}/src/googlemock/src/gmock-cardinalities.cc
    ${D}/src/googlemock/src/gmock-internal-utils.cc
    ${D}/src/googlemock/src/gmock-matchers.cc
    ${D}/src/googlemock/src/gmock-spec-builders.cc
    ${D}/src/googlemock/src/gmock.cc
  )

target_compile_definitions(googletest_gmock
  PRIVATE
    ${v8_defines}
    ${disable-exceptions-defines}
    ${gmock_config_defines}
    ${gtest_config_defines}
  )
target_include_directories(googletest_gmock
  PRIVATE
    ${v8_includes}
    ${gmock_config_includes}
    ${gtest_config_includes}
  )
