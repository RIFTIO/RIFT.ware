include(rift_externalproject)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(PKG_DEBUG_FLAGS "CFLAGS=-g -O0")
endif()

rift_externalproject_add(
  glib
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/glib-${GLIB_VERSION}
  CONFIGURE_COMMAND
    ./configure
      --prefix=${CMAKE_INSTALL_PREFIX}/usr
      ${PKG_DEBUG_FLAGS}
)

externalproject_add_step(
  glib
  autoreconf
  COMMAND NOCONFIGURE=1 ./autogen.sh
  WORKING_DIRECTORY <BINARY_DIR>
  DEPENDERS configure
  DEPENDEES rsync_to_build
)
