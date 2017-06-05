# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Srinivas Akkipeddi
# Creation Date: 12/09/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

# search for libnl
find_package(RIFT_LIBNL REQUIRED)        # libnl-3.0

# include directories to search for header files
include_directories(BEFORE ${RIFT_LIBNL_INCLUDE_DIRS})


##
# rwnetns library targets
##
set(C_SOURCES rwnetns.c rwnetlink.c rwudev.c)

add_library(rwnetns SHARED ${C_SOURCES})
target_link_libraries(rwnetns PRIVATE
  rwlib
  glib-2.0
  nl-3
  udev)
add_dependencies(rwnetns rwnetns_yang.headers)

##
# install the rwnetns library
##
install(TARGETS rwnetns  LIBRARY
  DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})

set(INSTALL_HFILES
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwnetns/include/riftware/rwnetns.h
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwnetns/include/riftware/rwudev.h
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwnetns/include/riftware/rwnetlink.h)

install(FILES ${INSTALL_HFILES} 
  DESTINATION usr/include COMPONENT ${INSTALL_COMPONENT})

##
# Add the cmake magic to generate the GI bindings
# Note that the g-ir-scanner scans the annotations
# in HFILES and generates the gir and typelib files
##
set(GI_NAME rwnetns)
set(GI_VERSION 1.0)
set(GI_NAMESPACE RwNetns)

rift_uncamel(${GI_NAMESPACE} UNCAMEL_GI_NAMESPACE)

##
# Add introspection target
##
rift_add_introspection(${GI_NAME}_gi
  NAMESPACE ${GI_NAMESPACE}
  VERSION ${GI_VERSION}
  VAPI_PREFIX ${UNCAMEL_GI_NAMESPACE}
  HFILES
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwnetns/include/riftware/${GI_NAME}.h
  SYMBOL_PREFIX "rwnetns"
  LIBRARIES rwnetns
  DEPENDS rwnetns
  BUILD_ALL
  )

##
# Install the generated gir and typelib files
##
rift_install_vala_artifacts(
  COMPONENT ${INSTALL_COMPONENT}
  GIR_FILES ${GI_NAMESPACE}-${GI_VERSION}.gir
  VAPI_FILES ${UNCAMEL_GI_NAMESPACE}-${GI_VERSION}.vapi
  TYPELIB_FILES ${GI_NAMESPACE}-${GI_VERSION}.typelib
  LUA_OVERRIDES ${GI_NAMESPACE}.lua
  DEST_PREFIX .
  )

