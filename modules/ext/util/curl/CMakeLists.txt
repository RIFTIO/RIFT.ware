include(rift_externalproject)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(PKG_DEBUG_FLAGS "CFLAGS=-g -O0")
endif()

# add an external project target for curl
rift_externalproject_add(
  curl
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/curl-7.35.0
  CONFIGURE_COMMAND
    ./configure
      --prefix=${CMAKE_INSTALL_PREFIX}/usr
      ${PKG_DEBUG_FLAGS}
)
