# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
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
  COMPONENT ${INSTALL_COMPONENT}
)

set (EXTRA_LIBS
     rwmsg
     ${CMAKE_INSTALL_PREFIX}/usr/lib/librwyang.so
     ${CMAKE_INSTALL_PREFIX}/usr/lib/librwlib.so
     ${CMAKE_INSTALL_PREFIX}/usr/lib/librw_schema_pb.so
     protobuf-c
     glib-2.0
     gobject-2.0
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
  BUILD_ALL
)

rift_install_vala_artifacts(
  HEADER_FILES ${GI_INCLUDE_HFILES}
  COMPONENT ${INSTALL_COMPONENT}
  GIR_FILES ${GI_NAMESPACE}-${GI_VERSION}.gir
  TYPELIB_FILES ${GI_NAMESPACE}-${GI_VERSION}.typelib
  VAPI_FILES ${UNCAMEL_NAMESPACE}-${GI_VERSION}.vapi
  LUA_OVERRIDES ${GI_NAMESPACE}.lua
  PYTHON_OVERRIDES ${GI_NAMESPACE}.py
  DEST_PREFIX .
)

#??add_dependencies(rwdtsproto rwfpathsyscommon)

install(TARGETS rwdtsproto LIBRARY DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})
