# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Aniruddha Atale
# Creation Date: 05/26/2014
# 

cmake_minimum_required(VERSION 2.8)

set(proto_files rw_dts_int.proto)

rift_add_proto_target(
  TARGET rwdts_proto_parse
  WITH_GI
  WITH_DSO
  PROTO_FILES ${proto_files}
  OUT_C_FILES_VAR proto_c_files
  COMPONENT ${PKG_LONG_NAME}
)

set (EXTRA_LIBS
     rwmsg
     ${CMAKE_INSTALL_PREFIX}/usr/lib/librwyang.so
     ${CMAKE_INSTALL_PREFIX}/usr/lib/librwlib.so
     ${CMAKE_INSTALL_PREFIX}/usr/lib/librw_schema_pb.so
     ${CMAKE_INSTALL_PREFIX}/usr/lib/libprotobuf-c.so
     ${CMAKE_INSTALL_PREFIX}/usr/lib/libglib-2.0.so
     ${CMAKE_INSTALL_PREFIX}/usr/lib/libgobject-2.0.so
     )
##
# Add the target for the  library
##
add_library(rwdtsproto SHARED ${proto_c_files})
add_dependencies(rwdtsproto rwdts_proto_parse)
target_link_libraries(rwdtsproto PRIVATE ${EXTRA_LIBS})


set(RWTYPES_PKG RwTypes-1.0)
set(RWTYPES_PATH ${RIFT_SUBMODULE_BINARY_ROOT}/rwtypes/src)

set(GI_NAMESPACE RwDtsProto)
set(GI_VERSION 1.0)
rift_uncamel(${GI_NAMESPACE} UNCAMEL_NAMESPACE)
set(gi_h_files ${CMAKE_CURRENT_BINARY_DIR}/rw_dts_int.pb-c.h)
get_filename_component(GI_INCLUDE_HFILES ${gi_h_files} NAME)

rift_add_introspection(
  ${GI_NAMESPACE}_gi
  NAMESPACE ${GI_NAMESPACE}
  IDENTIFIER_PREFIX ${GI_NAMESPACE} rwpb_gi_RwDts rw_dts_
  VAPI_PREFIX ${UNCAMEL_NAMESPACE}
  PACKAGES ${RWTYPES_PKG} ProtobufC-1.0 RwSchemaProto-1.0
  VERSION ${GI_VERSION}
  GI_INCLUDE_HFILES ${GI_INCLUDE_HFILES}
  GIR_PATHS ${RWTYPES_PATH}
  CFILES ${gi_c_files}
  HFILES ${gi_h_files}
  LIBRARIES rwdtsproto protobuf-c rwtypes rw_schema_pb
  LIBRARY_PATHS
    ${RWTYPES_PATH}
    ${RIFT_SUBMODULE_BINARY_ROOT}/yangtools/lib
  DEPENDS rwdtsproto
  GENERATE_GI_OVERRIDE
)

rift_install_vala_artifacts(
  HEADER_FILES ${GI_INCLUDE_HFILES}
  COMPONENT ${PKG_LONG_NAME}
  GIR_FILES ${GI_NAMESPACE}-${GI_VERSION}.gir
  TYPELIB_FILES ${GI_NAMESPACE}-${GI_VERSION}.typelib
  VAPI_FILES ${UNCAMEL_NAMESPACE}-${GI_VERSION}.vapi
  LUA_OVERRIDES ${GI_NAMESPACE}.lua
  PYTHON_OVERRIDES ${GI_NAMESPACE}.py
  DEST_PREFIX .
)

#??add_dependencies(rwdtsproto rwfpathsyscommon)

install(TARGETS rwdtsproto LIBRARY DESTINATION usr/lib COMPONENT ${PKG_LONG_NAME})
