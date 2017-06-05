# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Creation Date: 8/29/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

rift_pkg_check_modules(XERCES_C REQUIRED xerces-c>=3.1.1)
rift_pkg_check_modules(GOBJECT REQUIRED gobject-2.0>=2.42.0)
# rift_pkg_check_modules(NCX REQUIRED ncx>=2.2.5)

include_directories(BEFORE
  ${XERCES_C_INCLUDE_DIRS}
  ${GOBJECT_INCLUDE_DIRS}
  #${NCX_INCLUDE_DIRS}
  /usr/include/yuma/ncx
  ../rwyangutil)

###############################################################################
##
# Add a shared libarary target
##
set(rwyang_sources
  rw_app_data.cpp
  rw_json.cpp
  rw_keyspec.cpp
  rw_keyspec_mk.cpp
  rw_ks_helper.cpp
  rw_namespace.cpp
  rw_pb_cli.cpp
  rw_protobuf_c.cpp
  rw_pb_schema.cpp
  rw_xml.cpp
  rw_xml_ximpl.cpp
  rw_yang_pb.cpp
  rw_yang_json.cpp
  rw_yang_mutex.cpp
  yangmodel.cpp
  yangncx.cpp
  yangpbc_enum.cpp
  yangpbc_field.cpp
  yangpbc_message.cpp
  yangpbc_model.cpp
  yangpbc_module.cpp
  yangpbc_msgeq.cpp
  yangpbc_node.cpp
  yangpbc_doc_writer.cpp
  rw_var_root_util.cc
)

include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
add_library(rwyang SHARED $<TARGET_OBJECTS:rwmemlogobjlib> ${rwyang_sources})
target_link_libraries(rwyang PRIVATE
  rwlib
  rwtrace
  rw_schema_pb
  rwyangutil_lib
  ${XERCES_C_LIBRARIES}
  xalan-c
  ${GOBJECT_LIBRARIES}
  #${NCX_LIBRARIES}
  -lncx

  # RIFT-5092 - both should be using rift_pkg_check_modules
  CoreFoundation
  protobuf-c

  dl)

add_dependencies(rwyang
  rw_schema_pb
)

include_directories(
  /usr/include/libxml2/libxml
  /usr/include/libxml2/
  /usr/include/yuma/ncx
  /usr/include/yuma/platform
  )

install(TARGETS rwyang LIBRARY DESTINATION usr/lib
  COMPONENT ${INSTALL_COMPONENT})

rift_make_pc(rwyang
  LIBS -lrwyang
  COMPONENT ${INSTALL_COMPONENT})

install(FILES
  ../include/riftware/rw_gi.h
  ../include/riftware/rw_json.h
  ../include/riftware/rw_keyspec.h
  ../include/riftware/rw_keyspec_gi.h
  ../include/riftware/rw_keyspec_mk.h
  ../include/riftware/rw_namespace.h
  ../include/riftware/rw_pb.h
  ../include/riftware/rw_pb_schema.h
  ../include/riftware/rw_protobuf_c.h
  ../include/riftware/rw_schema_defs.h
  ../include/riftware/rw_xml.h
  ../include/riftware/rw_yang_json.h
  ../include/riftware/rw_yang_pb.h
  ../include/riftware/yangmodel.h
  ../include/riftware/yangmodel_gi.h
  ../include/riftware/yangpb_gi.h
  ../include/riftware/rw_var_root.h
  ../include/riftware/yuma-ncx.pb-c.h
  ../include/riftware/yuma-ncx.pb2c.h
  DESTINATION usr/include COMPONENT ${INSTALL_COMPONENT})

install(FILES
  ../include/riftware/rw_app_data.hpp
  ../include/riftware/rw_yang_json.h
  ../include/riftware/rw_yang_mutex.hpp
  ../include/riftware/util.hpp
  ../include/riftware/yangncx.hpp
  ../include/riftware/yangpbc.hpp
  DESTINATION usr/include/c++ COMPONENT ${INSTALL_COMPONENT})


###############################################################################
# confd-related
# ATTN: Should have special package name!

if(NOT RIFT_AGENT_BUILD STREQUAL "XML_ONLY")

    set (rwconfd_sources
         confd_xml.cpp
         rw_ut_confd.cpp
         rw_tree_iter.cpp
         rw_confd_upgrade.cpp
         rw_confd_annotate.cpp
    )

    include_directories(BEFORE ${CMAKE_BINARY_DIR}/confd/confd-build/libconfd/include)

    find_library(boost_filesystem_path NAMES boost_filesystem)
    if (NOT boost_filesystem_path)
       message (FATAL_ERROR "Could not find library ${boost_filesystem_path}")
    endif ()
    find_library(boost_system_path NAMES boost_system)
    if (NOT boost_system_path)
       message (FATAL_ERROR "Could not find library ${boost_system_path}")
    endif ()

    add_library(rwconfd SHARED ${rwconfd_sources})
    target_link_libraries(rwconfd PRIVATE
         ${CMAKE_BINARY_DIR}/confd/confd-build/libconfd/src/libconfd.so
         rwyang
         rwlib
         rw_schema_pb
         ${boost_filesystem_path}
         ${boost_system_path}
         glib-2.0
        )
    add_dependencies(rwconfd build_confd)
      
   rift_make_pc(rwconfd
        LIBS -lrwconfd
        COMPONENT ${INSTALL_COMPONENT}
   )

  install(TARGETS rwconfd LIBRARY
       DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})

  install(FILES
       ../include/riftware/rw_confd_upgrade.hpp
       ../include/riftware/rw_confd_annotate.hpp
       ../include/riftware/rw_ut_confd.hpp
       DESTINATION usr/include/c++ COMPONENT ${INSTALL_COMPONENT})

  install(FILES
       ../include/riftware/rw_tree_iter.h
       ../include/riftware/confd_xml.h
       DESTINATION usr/include COMPONENT ${INSTALL_COMPONENT})

  install(FILES confd.conf.harness
      DESTINATION usr/local/confd/etc/confd COMPONENT ${INSTALL_COMPONENT})

endif()


###############################################################################
# RwKeyspec Introspection Bindings

set(RWTYPES_PKG RwTypes-1.0)
set(RWTYPES_PATH ${RIFT_SUBMODULE_BINARY_ROOT}/rwtypes/src)

set(RWSCHEMA_PKG RwSchemaProto-1.0)
set(RWSCHEMA_PATH ${RIFT_SUBMODULE_BINARY_ROOT}/yangtools/proto)

set(RWKEYSPEC_PKG RwKeyspec-1.0)
set(RWKEYSPEC_PATH ${RIFT_SUBMODULE_BINARY_ROOT}/yangtools/lib)

set(RWYANGPB_PKG RwYangPb-1.0)
set(RWYANGPB_PATH ${RIFT_SUBMODULE_BINARY_ROOT}/yangtools/lib)

set(PROTOBUFC_PKG ProtobufC-1.0)
set(PROTOBUFC_PATH ${RIFT_SUBMODULE_BINARY_ROOT}/yangtools/lib)



###############################################################################
# ProtobufC Introspection Bindings

set(GI_VERSION 1.0)
set(GI_NAMESPACE ProtobufC)
rift_uncamel(${GI_NAMESPACE} UNCAMEL_GI_NAMESPACE)

rift_add_introspection(${GI_NAMESPACE}_gi
  NAMESPACE ${GI_NAMESPACE}
  VERSION ${GI_VERSION}
  VAPI_PREFIX ${UNCAMEL_GI_NAMESPACE}
  IDENTIFIER_PREFIX ProtobufC protobuf_c
  HFILES
    /usr/include/protobuf-c/rift-protobuf-c.h
    /usr/include/protobuf-c/rift/rw_pb_delta.h
    ${RIFT_SUBMODULE_SOURCE_ROOT}/yangtools/include/riftware/rw_protobuf_c.h
  LIBRARIES
    protobuf-c
    rwyang
  GI_INCLUDE_HFILES
    rift-protobuf-c.h
    rw_pb_delta.h
    rw_protobuf_c.h
  DEPENDS
    /usr/include/protobuf-c/rift-protobuf-c.h
    /usr/include/protobuf-c/rift/rw_pb_delta.h
    ${RIFT_SUBMODULE_SOURCE_ROOT}/yangtools/include/riftware/rw_protobuf_c.h
  GENERATE_GI_OVERRIDE
  DEPENDS
    rwyang
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
  PYTHON_OVERRIDES ${GI_NAMESPACE}.py
  DEST_PREFIX .
)


###############################################################################
# RwYangYpbc Introspection Bindings

set(GI_VERSION 1.0)
set(GI_NAMESPACE RwYangPb)
rift_uncamel(${GI_NAMESPACE} UNCAMEL_GI_NAMESPACE)

rift_add_introspection(${GI_NAMESPACE}_gi
  NAMESPACE ${GI_NAMESPACE}
  VERSION ${GI_VERSION}
  VAPI_PREFIX ${UNCAMEL_GI_NAMESPACE}
  IDENTIFIER_PREFIX RwYangPb rw_yang_pb
  PACKAGES
    ${RWTYPES_PKG}
  HFILES
    ${RIFT_SUBMODULE_SOURCE_ROOT}/yangtools/include/riftware/yangpb_gi.h
  LIBRARIES
    rwyang
  IDENTIFIER_FILTER_CMD "python ${CMAKE_CURRENT_SOURCE_DIR}/yangtools_gi_filter.py"
  GI_INCLUDE_HFILES yangpb_gi.h
  GIR_PATHS
    ${RWTYPES_PATH}
  LIBRARY_PATHS
    ${RWTYPES_PATH}
  DEPENDS
    rwyang
    rwtypes_gi
    ${CMAKE_CURRENT_SOURCE_DIR}/yangtools_gi_filter.py
  GENERATE_GI_OVERRIDE
)

rift_symlink_python_pbcm_override(${GI_NAMESPACE} COMPONENT ${INSTALL_COMPONENT})

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


###############################################################################
# RwKeyspec Introspection Bindings

set(GI_VERSION 1.0)
set(GI_NAMESPACE RwKeyspec)
rift_uncamel(${GI_NAMESPACE} UNCAMEL_GI_NAMESPACE)

rift_add_introspection(${GI_NAMESPACE}_gi
  NAMESPACE ${GI_NAMESPACE}
  VERSION ${GI_VERSION}
  VAPI_PREFIX ${UNCAMEL_GI_NAMESPACE}
  IDENTIFIER_PREFIX RwKeyspec rw_keyspec rw_schema_minikey
  PACKAGES
    ${RWTYPES_PKG}
    ${RWSCHEMA_PKG}
    ${RWYANGPB_PKG}
    ${PROTOBUFC_PKG}
  HFILES
    ${RIFT_SUBMODULE_SOURCE_ROOT}/yangtools/include/riftware/rw_keyspec_gi.h
  LIBRARIES
    rwyang
  IDENTIFIER_FILTER_CMD "python ${CMAKE_CURRENT_SOURCE_DIR}/yangtools_gi_filter.py"
  GI_INCLUDE_HFILES rw_keyspec_gi.h
  GIR_PATHS
    ${RWTYPES_PATH}
    ${RWSCHEMA_PATH}
    ${RWYANGPB_PATH}
    ${PROTOBUFC_PATH}
  LIBRARY_PATHS
    ${RWTYPES_PATH}
    ${RWSCHEMA_PATH}
    ${RWYANGPB_PATH}
    ${PROTOBUFC_PATH}
  DEPENDS
    rwyang
    RwSchemaProto_gi
    RwYangPb_gi
    rwtypes_gi
    ProtobufC_gi
    ${CMAKE_CURRENT_SOURCE_DIR}/yangtools_gi_filter.py
  GENERATE_GI_OVERRIDE
)

rift_symlink_python_pbcm_override(${GI_NAMESPACE})

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


###############################################################################
# Yang model Introspection Bindings

##
# Add the cmake magic to generate the GI bindings
# Note that the g-ir-scanner scans the annotations
# in HFILES and generates the gir and typelib files
##
set(GI_VERSION 1.0)
set(GI_NAMESPACE RwYang)
rift_uncamel(${GI_NAMESPACE} UNCAMEL_GI_NAMESPACE)

rift_add_introspection(${GI_NAMESPACE}_gi
  NAMESPACE ${GI_NAMESPACE}
  VERSION ${GI_VERSION}
  VAPI_PREFIX ${UNCAMEL_GI_NAMESPACE}
  IDENTIFIER_PREFIX RwYang rw_yang
  PACKAGES
    ${RWTYPES_PKG}
    ${RWSCHEMA_PKG}
    ${RWEYSPEC_PKG}
    ${RWYANGPB_PKG}
  HFILES
    ${RIFT_SUBMODULE_SOURCE_ROOT}/yangtools/include/riftware/yangmodel_gi.h
  LIBRARIES
    rwyang
  IDENTIFIER_FILTER_CMD "python ${CMAKE_CURRENT_SOURCE_DIR}/yangtools_gi_filter.py"
  GI_INCLUDE_HFILES yangmodel_gi.h
  GIR_PATHS
    ${RWTYPES_PATH}
    ${RWSCHEMA_PATH}
    ${RWKEYSPEC_PATH}
    ${RWYANGPB_PATH}
  LIBRARY_PATHS
    ${RWTYPES_PATH}
    ${RWSCHEMA_PATH}
    ${RWKEYSPEC_PATH}
    ${RWYANGPB_PATH}
  DEPENDS
    RwSchemaProto_gi
    RwKeyspec_gi
    RwYangPb_gi
    rwtypes_gi
    ${CMAKE_CURRENT_SOURCE_DIR}/yangtools_gi_filter.py
  GENERATE_GI_OVERRIDE
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
  PYTHON_OVERRIDES ${GI_NAMESPACE}.py
  DEST_PREFIX .
)

##
# Compile and install the xerces libcurl disable preload
##
add_library(rwxercespreload SHARED rw_xerces_netaccessor_preload.cpp)
install(
  TARGETS rwxercespreload
  LIBRARY
  DESTINATION usr/lib/rift/preloads
  COMPONENT ${INSTALL_COMPONENT}
  )
