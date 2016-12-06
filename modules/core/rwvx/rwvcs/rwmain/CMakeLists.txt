
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

include(rift_pkg_config)

rift_pkg_check_modules(RW_VX REQUIRED rw_vx_plugin>=1.0)
rift_pkg_check_modules(RWLIB REQUIRED rwlib>=1.0)
#rift_pkg_check_modules(CFLITE REQUIRED CFLite>=0.1.2)
rift_pkg_check_modules(GLIB REQUIRED glib-2.0>=2.42.0)
#rift_pkg_check_modules(MSGPACK REQUIRED msgpack>=0.5.9)
rift_pkg_check_modules(PB_C REQUIRED libprotobuf-c>=1.1.0)
rift_pkg_check_modules(TALLOC REQUIRED talloc>=2.1.0)
#rift_pkg_check_modules(COREDUMPER REQUIRED google-coredumper>=1.2.1)
rift_pkg_check_modules(PYTHON3 REQUIRED python3)


add_definitions(-std=gnu99 -D_GNU_SOURCE -D_CF_)

include_directories(
  ${RW_VX_INCLUDE_DIRS}
  ${GLIB_INCLUDE_DIRS}
  ${PB_C_INCLUDE_DIRS}
  ${TALLOC_INCLUDE_DIRS}
  ${RWLIB_INCLUDE_DIRS}

  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwvcs/reaper)

add_library(librwmain SHARED
  action.c
  bootstrap.c
  dts.c
  dts_rpc.c
  heartbeat.c
  proc.c
  rwmain.c
  pacemaker.c
  serf_client.c
  sys.c
  tasklet.c
  redis_client.c)
target_link_libraries(librwmain
  m
  rt
  ${RW_VX_LIBRARIES}
  CoreFoundation
  ${GLIB_LIBRARIES}
  msgpack
  ${PB_C_LIBRARIES}
  ${TALLOC_LIBRARIES}
  ${RWLIB_LIBRARIES}
  coredumper
  ${PYTHON3_LIBRARIES}

  reaper_client
  rwdts_api
  rwdts_yang_gen
  rwdtsproto
  dispatch
  rwlog
  rwmanifest_yang_gen
  rwha_yang_gen
  rwha_dts_api
  rwmsg
  rwnetns
  rwsched
  rwschema_yang_gen
  rwtasklet
  rwtrace
  rwvcs
  rw_tasklet_plugin-1.0
  rw_vcs_yang_gen
  dl

  rwshell-1.0
  rwshell_api)

set_target_properties(librwmain PROPERTIES
  PREFIX "")
install(TARGETS librwmain
  DESTINATION usr/${RIFT_LIBDIR}
  COMPONENT ${INSTALL_COMPONENT})

add_executable(rwmain
  main.c)
target_link_libraries(rwmain
  librwmain)

install(TARGETS rwmain
  DESTINATION usr/bin
  COMPONENT ${INSTALL_COMPONENT})



add_library(librwmain_gi SHARED
  rwmain_gi.c)
target_link_libraries(librwmain_gi
  librwmain)
set_target_properties(librwmain_gi PROPERTIES
  PREFIX "")
install(TARGETS librwmain_gi
  DESTINATION usr/${RIFT_LIBDIR}
  COMPONENT ${INSTALL_COMPONENT})

rift_add_introspection(rwmain_gi
  NAMESPACE RwMain
  VERSION 1.0
  VAPI_PREFIX RwMain
  NO_UNPREFIXED
  INCLUDE_PATHS
    /usr/include/dispatch
    ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/src/
    ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmsg/include/
  HFILES ${CMAKE_CURRENT_SOURCE_DIR}/rwmain_gi.h
  GI_INCLUDE_HFILES rwmain_gi.h
  LIBRARIES rwmain_gi
  GIR_PATHS
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/plugins/yang
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/rwtasklet
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwlog/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwsched/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwschema/yang
  SYMBOL_PREFIX rwmain
  IDENTIFIER_FILTER_CMD "sed -e 's,rwmain_gi_t,RwMainGi,'"
  IDENTIFIER_PREFIX RwMain rwmain
  DEPENDS
    rwtasklet_gi
    librwmain_gi
    rwmanifest_yang
  PACKAGES
    RwTypes-1.0
    RwManifestYang-1.0
    RwTasklet-1.0
  GI_PY_OVERRIDE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/rwmain_gi_override.py"
  BUILD_ALL
)

rift_install_vala_artifacts(
  COMPONENT ${INSTALL_COMPONENT}
  GIR_FILES RwMain-1.0.gir
  TYPELIB_FILES RwMain-1.0.typelib
  VAPI_FILES RwMain-1.0.vapi
  DEST_PREFIX .
)

add_custom_command(TARGET rwmain POST_BUILD
  COMMAND ${CMAKE_COMMAND}
  ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/rwmain.screenrc ${CMAKE_INSTALL_PREFIX}/etc/rwmain.screenrc
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rwmain.screenrc
  )

SET(BUILD_DIR $ENV{RIFT_BUILD})

