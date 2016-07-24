include(rift_externalproject)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(PKG_DEBUG_FLAGS "CFLAGS=-g -O0")
endif()


rift_externalproject_add(
  nanomsg
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/nanomsg
  CONFIGURE_COMMAND
    ./configure --prefix=${CMAKE_INSTALL_PREFIX}/usr ${PKG_DEBUG_FLAGS}
)
