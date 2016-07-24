include(rift_externalproject)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(PKG_DEBUG_FLAGS "CFLAGS=-g -O0")
endif()

rift_externalproject_add(
  xerces-c
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/xerces-c-3.1.1
  CONFIGURE_COMMAND
    ./configure
      --prefix=${CMAKE_INSTALL_PREFIX}/usr
      ${PKG_DEBUG_FLAGS}
)

