include(rift_externalproject)

rift_externalproject_add(
  lgi
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lgi
  CONFIGURE_COMMAND echo
  BUILD_COMMAND
    PKG_CONFIG_PATH=${RIFT_PKG_CONFIG_PATH} make
  INSTALL_COMMAND
    $(MAKE) install PREFIX=${CMAKE_INSTALL_PREFIX}/usr
  BCACHE_COMMAND
    $(MAKE)
      PREFIX=${RIFT_SUBMODULE_INSTALL_PREFIX}/lgi/${CMAKE_INSTALL_PREFIX}/usr
      install

  DEPENDS gobject-introspection
)

