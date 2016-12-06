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
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# 

cmake_minimum_required(VERSION 2.8)

# include(rift_logging)

set(PKG_NAME rwdts)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

add_definitions(-D_GNU_SOURCE)
# Why is this not the default
rift_werror()

rift_allow_compiler_warning(maybe-uninitialized)

##
# This appends to the list of directories to search for
# header files. INCLUDE_DIRECTORIES is a DIRECTORY property and
# is inherited from the parent directory.
#
include_directories(BEFORE
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmsg/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdts/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmemlogdts/include/riftware
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/src
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwschema/src
  /usr/include/libxml2
  /usr/include/libxml2/libxml
  /usr/include/yuma/ncx
  /usr/include/yuma/platform)

set(LIB_SOURCES_API
  src/rwdts_query.c
  src/rwdts_api.c
  src/rwdts_member_service.c
  src/rwdts_member.c
  src/rwdts_member_registrations.c
  src/rwdts_member_data_api.c
  src/rwdts_member_kv.c
  src/rwdts_member_xact.c
  src/rwdts_appconf_api.c
  src/rwdts_util.c
  src/rwdts_appstats_api.c
  src/rwdts_audit.c
  src/rwdts_shard.c
  src/rwdts_appdata_api.c
  src/rwdts_xpath.c
  src/rwdts_xpath_ncx.c
  )
add_library(rwdts_api SHARED ${LIB_SOURCES_API} $<TARGET_OBJECTS:rwmemlogdts> )
target_link_libraries(rwdts_api PRIVATE
  kv_light_api
  rwdtsproto
  rwlib
  rwmsg
  rwsched
  rwtasklet
  rwtrace
  rwyang
  rwvcs
  rwdts_yang_gen
  rwha_yang_gen
  rwmemlog
  dispatch
  glib-2.0
  protobuf-c
  talloc
  rwlog
  rwmemlog
  rwmemlog_yang_gen
  rwschema_yang_gen
  rwgeneric_yang_gen
  gobject-2.0
  rt
  ssl
  pthread
  crypto
  ncx
)
add_dependencies(rwdts_api rwdts_yang.headers rw_tasklet_plugin-1.0 rw_vcs_yang.headers)

# Redis cluster is a plugin ?
set(REDIS_CLUSTER_SOURCES
    src/rwdts_redis.c)
add_library(rwdts_redis SHARED ${REDIS_CLUSTER_SOURCES})
target_link_libraries(rwdts_redis PRIVATE
  rwdtsproto
  rwlib
  rwsched

  dispatch
  glib-2.0
  gobject-2.0
  hiredis
  talloc)
install(TARGETS rwdts_redis LIBRARY DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})

set(KV_API_SOURCES
    src/rwdts_kv_light_api.c
    src/rwdts_kv_redis_interface.c
    src/rwdts_kv_bkd_interface.c)
add_library(kv_light_api SHARED ${KV_API_SOURCES})
target_link_libraries(kv_light_api PRIVATE
  rwdts_redis
  rwlib
  rwsched
  dispatch
  talloc
  db
  glib-2.0
  gobject-2.0)
add_dependencies(kv_light_api rw_vcs_yang.headers)
install(TARGETS kv_light_api LIBRARY DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})


# Router, really it's a plugin
set(ROUTER_SOURCES
  src/rwdts_router.c
  src/rwdts_router_service.c
  src/rwdts_router_xact.c
  )
add_library(rwdts_router SHARED ${ROUTER_SOURCES})
install(TARGETS rwdts_router LIBRARY DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})
target_link_libraries(rwdts_router PRIVATE
  rwdts_api
  rwdtsproto
  rwdts_yang_gen
  rwlib
  rwmsg
  rwsched
  rw_tasklet_plugin-1.0
  rwvcs
  rwvcs_zk-1.0
  rwyang
  rwlog
  rwmemlog
  CoreFoundation
  dispatch
  glib-2.0
  protobuf-c
  talloc)


set(C_SOURCES test/rwdts_links.c)
set(rwdts_link_libs
  rwdts_api
  rwdtsproto
  rw_schema_pb
  rwlib
  rwmemlog
  rwlog
  rwmsg
  rwsched
  rwtrace
  rwyang

  BlocksRuntime
  ck
  dispatch
  event
  glib-2.0
  nanomsg
  protobuf-c
  talloc
  unwind)
add_executable(rwdts_testlink ${C_SOURCES})
target_link_libraries(rwdts_testlink ${rwdts_link_libs})

set(REDIS_C_SOURCES test/rwdts_redis_client.c)
set(KV_C_SOURCES test/rwdts_kv_light_client.c)
set(KV_C_TAB_SOURCES test/rwdts_kv_light_tab_client.c)
set(KV_C_SCR_SOURCES test/rwdts_kv_light_script_client.c)
set(rwdts_redis_libs
  rwdts_api
  rwdtsproto
  rwdts_redis
  kv_light_api
  hiredis
  rwmsg
  rwsched

  protobuf-c
  ck
  ncx
  rwmsg
  rwtrace
  rwlib
  talloc
  unwind
  nanomsg
  event
  glib-2.0
  rwmemlog
  rwlog
  pthread
  dispatch
  BlocksRuntime)

add_executable(rwdts_redis_client ${REDIS_C_SOURCES})
add_executable(rwdts_kv_light_client ${KV_C_SOURCES})
add_executable(rwdts_kv_light_tab_client ${KV_C_TAB_SOURCES})
add_executable(rwdts_kv_light_script_client ${KV_C_SCR_SOURCES})
target_link_libraries(rwdts_redis_client ${rwdts_redis_libs})
target_link_libraries(rwdts_kv_light_client ${rwdts_redis_libs})
target_link_libraries(rwdts_kv_light_tab_client ${rwdts_redis_libs})
target_link_libraries(rwdts_kv_light_script_client ${rwdts_redis_libs})
add_dependencies(rwdts_redis_client rwdts_api rwmsg rwtrace rwlog rwdtsproto kv_light_api)



##
# Install the header files and targets
##
set(INSTALL_HFILES
    include/rwdts.h
    include/rwdts_keyspec.h
    include/rwdts_api_gi.h
    include/rwdts_query_api.h
    include/rwdts_member_api.h
    include/rwdts_kv_light_common.h
    include/rwdts_kv_redis_interface.h
    include/rwdts_kv_light_api.h
    include/rwdts_appconf_api.h
    include/rwdts_appstats_api.h
    include/rwdts_api_gi_enum.h
    include/rwdts_xpath.h
    include/rwdts_kv_light_api_gi.h
    )

set(INSTALL_BFILES
    test/redis-trib.rb
    test/redis.conf
    test/redis_cluster.py
    )

install(FILES ${INSTALL_HFILES} DESTINATION usr/include COMPONENT ${INSTALL_COMPONENT})
install(FILES ${INSTALL_BFILES} DESTINATION usr/bin COMPONENT ${INSTALL_COMPONENT})
install(TARGETS rwdts_api LIBRARY DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})
install(TARGETS rwdts_router LIBRARY DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})
install(TARGETS rwdts_redis LIBRARY DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})
install(TARGETS kv_light_api LIBRARY DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})

##
# documentation target
##
rift_add_doxygen_target(${PKG_NAME}_doxygen
  DOXY_NAME "RW.DTS QuickStart and Reference"
  DOXY_FILES ${INSTALL_HFILES}
  DEST_DIR ${PKG_NAME})

set(subdirs plugins test ra)
rift_add_subdirs(SUBDIR_LIST ${subdirs})

##
# Add the cmake magic to generate the GI bindings
# Note that the g-ir-scanner scans the annotations
# in HFILES and generates the gir and typelib files
##
set(GI_NAME rwdts)
set(GI_VERSION 1.0)
set(GI_NAMESPACE RwDts)
rift_uncamel(${GI_NAMESPACE} UNCAMEL_GI_NAMESPACE)

rift_add_introspection(${GI_NAME}_gi
  NAMESPACE ${GI_NAMESPACE}
  VERSION ${GI_VERSION}
  VAPI_PREFIX ${UNCAMEL_GI_NAMESPACE}
  GIR_PATHS
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/rwtasklet
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/plugins/vala
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwlog/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwsched/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/plugins/yang
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwschema/yang
  LIBRARY_PATHS
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwdts/plugins/yang
  PACKAGES RwTypes-1.0 RwTasklet-1.0 RwYang-1.0 RwKeyspec-1.0 ProtobufC-1.0
  HFILES
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdts/include/rwdts_api_gi_enum.h
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdts/include/rwdts_api_gi.h
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdts/include/rwdts_kv_light_api_gi.h
  LIBRARIES rwdts_api rwdts_yang_gen rwvcs kv_light_api
  IDENTIFIER_FILTER_CMD "python ${CMAKE_CURRENT_SOURCE_DIR}/src/rwdts_gi_filter.py"
  IDENTIFIER_PREFIX "RWDts" "RwDts"
  GI_INCLUDE_HFILES rwdts_api_gi_enum.h rwdts_api_gi.h rwdts_kv_light_api_gi.h
  INCLUDE_PATHS  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdts/include
  DEPENDS rwdts_api rwtasklet_gi rwdts_yang kv_light_api
  GENERATE_GI_OVERRIDE
  BUILD_ALL
  )

#  GI_PY_OVERRIDE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/rwdts_gi_override.py"

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

