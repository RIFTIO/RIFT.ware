include(rift_externalproject)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(PKG_DEBUG_FLAGS "CFLAGS=-g -O0")
endif()

rift_externalproject_add(
  libpeas
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libpeas-${LIBPEAS_VERSION}
  LIST_SEPARATOR ^^
  CONFIGURE_COMMAND
    ./configure
      --prefix=${CMAKE_INSTALL_PREFIX}/usr
      --enable-luajit
      --disable-silent-rules
      --disable-gtk
      --disable-gtk-doc-html
      --enable-python3
      --enable-python
      GLIB_GENMARSHAL=${CMAKE_INSTALL_PREFIX}/usr/bin/glib-genmarshal
      PKG_CONFIG_PATH=${RIFT_PKG_CONFIG_PATH}
      ${PKG_DEBUG_FLAGS}
      LUA_PATH=${CMAKE_INSTALL_PREFIX}/usr/share/lua/5.1/?.lua^^${CMAKE_INSTALL_PREFIX}/usr/share/lua/5.1/?/init.lua
      LUA_CPATH=${CMAKE_INSTALL_PREFIX}/usr/lib/lua/5.1/?.so
  DEPENDS pygobject pygobject3
)

externalproject_add_step(
  libpeas
  autoreconf
  COMMAND
    gtkdocize
    && intltoolize
    && autoreconf --verbose --force --install -I${CMAKE_INSTALL_PREFIX}/usr/share/aclocal
  WORKING_DIRECTORY <BINARY_DIR>
  DEPENDERS configure
  DEPENDEES rsync_to_build
)

