include(rift_externalproject)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(PKG_DEBUG_FLAGS "CFLAGS=-g -O0")
endif()

rift_externalproject_add(
  pygobject
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/pygobject-${PYGOBJECT_VERSION}
  CONFIGURE_COMMAND
    ./configure
      --prefix=${CMAKE_INSTALL_PREFIX}/usr
      --disable-cairo
      PKG_CONFIG_PATH=${RIFT_PKG_CONFIG_PATH}
      PYTHON=python
      ${PKG_DEBUG_FLAGS}
  DEPENDS glib gobject-introspection vala
)

externalproject_add_step(
  pygobject
  autoreconf
  COMMAND
    autoreconf --verbose --force --install -I${CMAKE_INSTALL_PREFIX}/usr/share/aclocal
  WORKING_DIRECTORY <BINARY_DIR>
  DEPENDERS configure
  DEPENDEES rsync_to_build
)

rift_externalproject_add(
  pygobject3
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/pygobject-${PYGOBJECT_VERSION}
  CONFIGURE_COMMAND
    ./configure
      --prefix=${CMAKE_INSTALL_PREFIX}/usr
      --disable-cairo
      PKG_CONFIG_PATH=${RIFT_PKG_CONFIG_PATH}
      PYTHON=python3
      ${PKG_DEBUG_FLAGS}
  DEPENDS glib gobject-introspection vala
)

externalproject_add_step(
  pygobject3
  autoreconf
  COMMAND
    autoreconf --verbose --force --install -I${CMAKE_INSTALL_PREFIX}/usr/share/aclocal
  WORKING_DIRECTORY <BINARY_DIR>
  DEPENDERS configure
  DEPENDEES rsync_to_build
)
