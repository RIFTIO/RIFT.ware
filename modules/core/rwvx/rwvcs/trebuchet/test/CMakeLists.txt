
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

include(rift_unittest)

include_directories(BEFORE
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwvcs/trebuchet/src)

rift_pkg_check_modules(RWLIB REQUIRED rwlib>=1.0)
rift_pkg_check_modules(GLIB REQUIRED glib-2.0>=2.42.0)

rift_gtest(unittest_trebuchet
  TEST_SRCS trebuchet_test.cpp
  TEST_LIBS
    trebuchet
    ${RWLIB_LIBRARIES}
    ${GLIB_LIBRARIES}
  ENVSET_ARGS
    TREBUCHET_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../
)
