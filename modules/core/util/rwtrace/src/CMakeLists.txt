# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 02/13/2015
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

##
# Add a shared libarary target
##
set(C_SOURCES rwtrace.c)
add_library(rwtrace SHARED ${C_SOURCES})

rift_pkg_check_modules(GOBJECT REQUIRED gobject-2.0>=2.42.0)
include_directories(BEFORE
  ${GOBJECT_INCLUDE_DIRS})

##
# Specify the libraries that foo app depends on
##
target_link_libraries(rwtrace PRIVATE
  rwlib
  ${GOBJECT_LIBRARIES})
rift_make_pc(rwtrace
  LIBS -lrwtrace
  INCLUDEDIRS ${GOBJECT_INCLUDE_DIRS}
  COMPONENT ${INSTALL_COMPONENT})

set(RWTYPES_PKG RwTypes-1.0)
set(RWTYPES_PATH ${RIFT_SUBMODULE_BINARY_ROOT}/rwtypes/src)

set(GI_NAMESPACE RwTrace)
set(GI_VERSION 1.0)

rift_uncamel(${GI_NAMESPACE} UNCAMEL_GI_NAMESPACE)

rift_add_introspection(rwtrace_gi
  NAMESPACE ${GI_NAMESPACE}
  VERSION ${GI_VERSION}
  VAPI_PREFIX ${UNCAMEL_GI_NAMESPACE}
  HFILES ${RIFT_SUBMODULE_SOURCE_ROOT}/rwtrace/include/riftware/rwtrace_gi.h
  GI_INCLUDE_HFILES rwtrace_gi.h
  LIBRARIES rwtrace rwtypes
  PACKAGES ${RWTYPES_PKG}
  LIBRARY_PATHS 
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwtrace/src
    ${RWTYPES_PATH}
  IDENTIFIER_FILTER_CMD "python ${CMAKE_CURRENT_SOURCE_DIR}/rwtrace_gi_filter.py"
  GIR_PATHS ${RWTYPES_PATH}
  DEPENDS rwtrace
  BUILD_ALL
  )

rift_install_vala_artifacts( 
  COMPONENT ${INSTALL_COMPONENT} 
  HEADER_FILES ${GI_INCLUDE_HFILE}
  GIR_FILES ${GI_NAMESPACE}-${GI_VERSION}.gir
  TYPELIB_FILES ${GI_NAMESPACE}-${GI_VERSION}.typelib
  VAPI_FILES ${UNCAMEL_GI_NAMESPACE}-${GI_VERSION}.vapi
  LUA_OVERRIDES ${GI_NAMESPACE}.lua
  DEST_PREFIX .
  )

set(HFILES 
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwtrace/include/riftware/rwtrace.h
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwtrace/include/riftware/rwtrace_gi.h
  )
install(FILES ${HFILES} DESTINATION usr/include COMPONENT ${INSTALL_COMPONENT})
install(TARGETS rwtrace LIBRARY DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})
