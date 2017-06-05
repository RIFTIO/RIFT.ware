# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tom Seidenberg
# Creation Date: 2014/08/29
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

set(proto_files
  rw_schema.proto
)

set(EXTRA_LIBS
  rwlib
  rwtypes
  pthread_workqueue
  kqueue
  protobuf-c
  glib-2.0
  gobject-2.0
  )

rift_add_proto_target(
  WITH_GI
  TARGET rw_schema_proto
  PROTO_FILES ${proto_files}
  OUT_C_FILES_VAR proto_c_files
  COMPONENT ${INSTALL_COMPONENT}
  DEPENDS pb2c_proto
)

add_library(rw_schema_pb SHARED ${proto_c_files})
target_link_libraries(rw_schema_pb ${EXTRA_LIBS})

# install the libraries
install(TARGETS rw_schema_pb LIBRARY
  DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})


set(GI_GIR_PKGS RwTypes-1.0 ProtobufC-1.0)
set(GI_GIR_PATHS ${RIFT_SUBMODULE_BINARY_ROOT}/rwtypes/src 
                 ${RIFT_SUBMODULE_BINARY_ROOT}/yangtools/lib)

set(GI_NAMESPACE RwSchemaProto)
set(GI_VERSION 1.0)
rift_uncamel(${GI_NAMESPACE} UNCAMEL_NAMESPACE)
set(gi_h_files ${CMAKE_CURRENT_BINARY_DIR}/rw_schema.pb-c.h)
get_filename_component(GI_INCLUDE_HFILES ${gi_h_files} NAME)

rift_add_introspection(
  ${GI_NAMESPACE}_gi
  NAMESPACE ${GI_NAMESPACE}
  IDENTIFIER_PREFIX ${GI_NAMESPACE} rwpb_gi_RwSchema rw_schema_
  VAPI_PREFIX ${UNCAMEL_NAMESPACE}
  PACKAGES ${GI_GIR_PKGS}
  VERSION ${GI_VERSION}
  GI_INCLUDE_HFILES ${GI_INCLUDE_HFILES}
  GIR_PATHS ${GI_GIR_PATHS}
  CFILES ${gi_c_files}
  HFILES ${gi_h_files}
  LIBRARIES rw_schema_pb protobuf-c rwtypes
  LIBRARY_PATHS
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwtypes/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/yangtools/lib
  DEPENDS rw_schema_pb ProtobufC_gi
)

rift_symlink_python_pbcm_override(
    ${GI_NAMESPACE}
    COMPONENT ${INSTALL_COMPONENT}
    )

rift_install_vala_artifacts(
  HEADER_FILES ${GI_INCLUDE_HFILES}
  COMPONENT ${INSTALL_COMPONENT}
  GIR_FILES ${GI_NAMESPACE}-${GI_VERSION}.gir
  TYPELIB_FILES ${GI_NAMESPACE}-${GI_VERSION}.typelib
  VAPI_FILES ${UNCAMEL_NAMESPACE}-${GI_VERSION}.vapi
  LUA_OVERRIDES ${GI_NAMESPACE}.lua
  DEST_PREFIX .
)
