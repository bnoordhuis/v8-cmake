#
# From test/mkgrokdump/BUILD.gn
#

set(D ${PROJECT_SOURCE_DIR}/v8/test/mkgrokdump)

add_executable(mkgrokdump
  ${D}/mkgrokdump.cc
  )
target_config(mkgrokdump
  PRIVATE v8_features v8_disable_exceptions internal_config_base
  )
target_link_libraries(mkgrokdump
  PRIVATE
    v8_compiler
    v8_snapshot
    v8_libbase
    v8_libplatform
  )