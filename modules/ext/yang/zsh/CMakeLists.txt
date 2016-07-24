include(rift_externalproject)
include(rift_pkg_config)


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(PKG_DEBUG_FLAGS "--enable-zsh-debug")
endif()

rift_externalproject_add(
  zsh
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/zsh-5.0.7
  CONFIGURE_COMMAND ./configure
    --with-tcsetpgrp
    --disable-gdbm
    --prefix=${CMAKE_INSTALL_PREFIX}/usr
    ${PKG_DEBUG_FLAGS}
  BUILD_COMMAND $(MAKE) ${make_args}
  INSTALL_COMMAND $(MAKE) ${make_args} install
  BCACHE_COMMAND
    $(MAKE) DESTDIR=${RIFT_SUBMODULE_INSTALL_PREFIX}/zsh ${make_args} install
)

externalproject_add_step(
  zsh
  autoreconf
  COMMAND autoreconf -i -f
  WORKING_DIRECTORY <BINARY_DIR>
  DEPENDERS configure
  DEPENDEES rsync_to_build
)

