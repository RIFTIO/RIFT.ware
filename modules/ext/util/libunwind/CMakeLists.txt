include(rift_externalproject)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(PKG_DEBUG_FLAGS "CFLAGS=-g -O0")
endif()

rift_externalproject_add(
  libunwind
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libunwind-1.1
  CONFIGURE_COMMAND
      ./configure
        --prefix=${CMAKE_INSTALL_PREFIX}/usr
        ${PKG_DEBUG_FLAGS}
)

externalproject_add_step(libunwind
  autoreconf
  COMMAND autoreconf -i
  WORKING_DIRECTORY <BINARY_DIR>
  DEPENDERS update patch configure
  DEPENDEES rsync_to_build
)
