# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf
# Creation Date: 12/2/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwsched)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

add_definitions(-D_CF_)

##
# Add the target for library
##
set(C_SOURCES
    rwsched_main.c
    rwsched_object.c
    rwsched_queue.c
    rwsched_sthread.c
    rwsched_source.c
    rwsched_toysched.c
    cfrunloop.c
    cfrunloop_internal.c
    cfsocket.c)
add_library(rwsched SHARED ${C_SOURCES})
target_link_libraries(rwsched PRIVATE
  ck
  CoreFoundation
  dispatch
  glib-2.0
  gobject-2.0
  talloc
  rwlog
  rwsched_log_yang_gen
  rwlib)

add_library(rwmalloc SHARED rw_malloc_intercepted.c)
target_link_libraries(rwmalloc PRIVATE dl rwsched ck CoreFoundation rwlib talloc glib-2.0)

##
# Install the header files and targets
##
set(INSTALL_HFILES 
  ../include/riftware/rwsched.h
  ../include/riftware/rwsched_gi.h
  ../include/riftware/rwsched_object.h
  ../include/riftware/rwsched_queue.h
  ../include/riftware/rwsched_source.h
  ../include/riftware/rwsched_main.h
  rwsched_internal.h)
install(FILES ${INSTALL_HFILES}
  DESTINATION usr/include COMPONENT ${INSTALL_COMPONENT})

set(INSTALL_HRWFILES
  ../include/riftware/rwsched/cfrunloop.h
  ../include/riftware/rwsched/cfsocket.h
  ../include/riftware/rwsched/core_foundation.h)
install(FILES ${INSTALL_HRWFILES}
  DESTINATION usr/include/rwsched COMPONENT ${INSTALL_COMPONENT})

install(TARGETS rwsched rwmalloc LIBRARY
  DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})

##
# Add the cmake magic to generate the GI bindings
# Note that the g-ir-scanner scans the annotations
# in HFILES and generates the gir and typelib files
##
set(GI_NAME rwsched)
set(GI_VERSION 1.0)
set(GI_NAMESPACE RwSched)
rift_uncamel(${GI_NAMESPACE} UNCAMEL_GI_NAMESPACE)

rift_add_introspection(${GI_NAME}_gi
  NAMESPACE ${GI_NAMESPACE}
  VERSION ${GI_VERSION}
  VAPI_PREFIX ${UNCAMEL_GI_NAMESPACE}
  INCLUDE_PATHS 
    /usr/include/dispatch
  PACKAGES CF-1.0
  HFILES
    ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/include/riftware/rwsched_gi.h
    ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/include/riftware/rwsched/cfrunloop.h
    ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/include/riftware/rwsched/cfsocket.h
  LIBRARIES rwsched
  IDENTIFIER_FILTER_CMD "python ${CMAKE_CURRENT_SOURCE_DIR}/rwsched_gi_filter.py"
  SYMBOL_PREFIX "rwsched"
  GI_INCLUDE_HFILES rwsched_gi.h
  DEPENDS rwsched
  )

##
# Install the generated gir and typelib files
##
rift_install_vala_artifacts(
  COMPONENT ${INSTALL_COMPONENT}
  GIR_FILES ${GI_NAMESPACE}-${GI_VERSION}.gir
  TYPELIB_FILES ${GI_NAMESPACE}-${GI_VERSION}.typelib
  VAPI_FILES ${UNCAMEL_GI_NAMESPACE}-${GI_VERSION}.vapi
  LUA_OVERRIDES ${GI_NAMESPACE}.lua
  DEST_PREFIX .
  )
