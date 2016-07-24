include(rift_externalproject)
include(rift_pkg_config)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(PKG_DEBUG_FLAGS "CFLAGS=-g -O0")
endif()

set(PROJ_LDFLAGS
  "LDFLAGS=-L${CMAKE_INSTALL_PREFIX}/usr/${RIFT_LIBDIR}")

#
# compose the CPPFLAGS. This is mainly for compiling the test code inside the
# protobuf-c directory which uses the generated .pb-c.h files.  The generated
# files includes rift specific headers.
##
set(PROJ_CPPFLAGS
  "CPPFLAGS=-I${RIFT_SUBMODULE_SOURCE_ROOT}/rwlib/include/riftware")

rift_externalproject_add(
  protobuf-c
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/protobuf-c
  CONFIGURE_COMMAND
    ./configure
      --prefix=${CMAKE_INSTALL_PREFIX}/usr
      --program-prefix=rift-
      ${PROJ_LDFLAGS}
      ${PKG_DEBUG_FLAGS}
      ${PROJ_CPPFLAGS}
)

externalproject_add_step(
  protobuf-c
  autoreconf
  COMMAND
    autoreconf --force --install --verbose
  WORKING_DIRECTORY <BINARY_DIR>
  DEPENDERS configure
  DEPENDEES rsync_to_build
)

# There is an ugly hack here, to overcome the issue with symlinks
# when CPACK_INSTALLED_DIRECTORIES. Presence of symlinks causes the CPACK
# to abort with "CPack Error: 443 Cannot create symlink"
set(bcache_dest
  ${RIFT_SUBMODULE_INSTALL_PREFIX}/protobuf-c/${CMAKE_INSTALL_PREFIX}/usr)

externalproject_add_step(
  protobuf-c
  remove_symlink
  COMMAND rm ${bcache_dest}/include/google/protobuf-c/rift-protobuf-c.h
  COMMAND cp
    ${bcache_dest}/include/protobuf-c/rift-protobuf-c.h
    ${bcache_dest}/include/google/protobuf-c/rift-protobuf-c.h
  DEPENDEES bcache)

