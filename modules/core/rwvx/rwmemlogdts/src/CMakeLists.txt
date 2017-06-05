# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Creation Date: 9/11/15
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

##
# Add a shared libarary target
##

set(
  C_SOURCES
    rwmemlogdts.c 
)

add_library(
  rwmemlogdts
  OBJECT 
    ${C_SOURCES}
    ${auto_c_files}
)

include_directories(BEFORE
${RIFT_SUBMODULE_SOURCE_ROOT}/rwmsg/include
${RIFT_SUBMODULE_SOURCE_ROOT}/rwdts/include
${RIFT_SUBMODULE_SOURCE_ROOT}/rwdtsperf/include
${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/src
${RIFT_SUBMODULE_SOURCE_ROOT}/rwschema/src
${RIFT_SUBMODULE_BINARY_ROOT}/rwdts/plugins/proto
)

add_dependencies(
  rwmemlogdts
  rwmemlog
  rwdtsproto
  rwdts_yang.headers
  rwgeneric_yang.headers
  rwschema_yang.headers
  rwlog_event_auto
  process_rwlog_evt_files
)


##
# Install the header files and targets
##
set(INSTALL_HFILES
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmemlogdts/include/riftware/rwmemlogdts.h
)

install(FILES ${INSTALL_HFILES} DESTINATION usr/include COMPONENT ${INSTALL_COMPONENT})
install(FILES ${auto_h_files} DESTINATION usr/include/rwmemlogdts COMPONENT ${INSTALL_COMPONENT})

##
# documentation target
##
rift_add_doxygen_target(${PKG_NAME}_doxygen
  DOXY_NAME "RW.MemlogDts QuickStart and API Reference"
  DOXY_FILES 
  ../include/riftware/memlogdts.h 
  DEST_DIR ${PKG_NAME})


##
# Add the cmake magic to generate the GI bindings
# Note that the g-ir-scanner scans the annotations
# in HFILES and generates the gir and typelib files
##
set(GI_NAME rwmemlogdts)
set(GI_VERSION 1.0)
set(GI_NAMESPACE RwMemlog)
rift_uncamel(${GI_NAMESPACE} UNCAMEL_GI_NAMESPACE)


rift_add_introspection(${GI_NAME}_gi
  NAMESPACE ${GI_NAMESPACE}
  VERSION ${GI_VERSION}
  HFILES
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmemlogdts/include/riftware/rwmemlogdts.h
  LIBRARIES rwdts_api rwdts_yang_gen rwvcs kv_light_api
  SYMBOL_PREFIX "rwmemlog"
  IDENTIFIER_FILTER_CMD "python ${CMAKE_CURRENT_SOURCE_DIR}/rwmemlogdts_gi_filter.py"
  IDENTIFIER_PREFIX "RWMemlog" "RwMemlog"
  GI_INCLUDE_HFILES rwmemlog.h
  GIR_PATHS
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/rwtasklet
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/plugins/vala
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwlog/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwsched/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/plugins/yang
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwschema/yang
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwdts
  LIBRARY_PATHS
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwdts/plugins/yang
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwdts
  PACKAGES RwTypes-1.0 RwTasklet-1.0 RwYang-1.0 RwKeyspec-1.0 ProtobufC-1.0 RwDts-1.0
  DEPENDS rwdts_gi rwdts_api rwdts_api rwtasklet_gi rwdts_yang kv_light_api
  VAPI_PREFIX ${UNCAMEL_GI_NAMESPACE}
  GENERATE_GI_OVERRIDE
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
