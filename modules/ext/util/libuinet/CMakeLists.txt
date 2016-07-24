include(rift_externalproject)

rift_externalproject_add(
  libuinet
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libuinet
  CONFIGURE_COMMAND echo
  BUILD_COMMAND make -C lib/libuinet
  INSTALL_COMMAND make -C lib/libuinet UINET_DESTDIR=${CMAKE_INSTALL_PREFIX}/usr install
  BCACHE_COMMAND
    make
      -C lib/libuinet
      UINET_DESTDIR=${RIFT_SUBMODULE_INSTALL_PREFIX}/libuinet/${CMAKE_INSTALL_PREFIX}/usr
      install
)


##
# Add the cmake magic to generate the GI bindings
# Note that the g-ir-scanner scans the annotations
# in HFILES and generates the gir and typelib files
# It turns out that GIT scanner requires a shared library,
# but the libuinet generates a static library
##
set(GI_NAME mbuf)
set(GI_VERSION 1.0)
set(GI_NAMESPACE Mbuf)

rift_add_introspection(${GI_NAME}_gi
  NAMESPACE ${GI_NAMESPACE}
  VERSION ${GI_VERSION}
  HFILES 
  ${RIFT_SUBMODULE_SOURCE_ROOT}/libuinet/libuinet/sys/sys/${GI_NAME}.h
  LIBRARY_PATHS ${CMAKE_CURRENT_BINARY_DIR}/src/libuinet/lib/libuinet
  LIBRARIES uinet pcap ssl
  DEPENDS libuinet 
  )

##
# Install the generated gir and typelib files
##
rift_install_vala_artifacts(
  COMPONENT libuinet
  GIR_FILES ${GI_NAMESPACE}-${GI_VERSION}.gir
  TYPELIB_FILES ${GI_NAMESPACE}-${GI_VERSION}.typelib
  LUA_OVERRIDES ${GI_NAMESPACE}.lua
  DEST_PREFIX .
  )

