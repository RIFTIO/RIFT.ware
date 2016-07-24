
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

include(rift_pkg_config)

rift_pkg_check_modules(RW_VX REQUIRED rw_vx_plugin>=1.0)
rift_pkg_check_modules(RWLIB REQUIRED rwlib>=1.0)
rift_pkg_check_modules(CFLITE REQUIRED cflite>=744.12)
rift_pkg_check_modules(GLIB REQUIRED glib-2.0>=2.42.0)
rift_pkg_check_modules(MSGPACK REQUIRED msgpack>=0.5.9)
rift_pkg_check_modules(PB_C REQUIRED libprotobuf-c>=1.1.0)
rift_pkg_check_modules(TALLOC REQUIRED talloc>=2.1.0)
rift_pkg_check_modules(COREDUMPER REQUIRED coredumper>=1.2.1)


add_definitions(-std=gnu99 -D_GNU_SOURCE -D_CF_)

include_directories(
  ${RW_VX_INCLUDE_DIRS}
  ${CFLITE_INCLUDE_DIRS}
  ${GLIB_INCLUDE_DIRS}
  ${MSGPACK_INCLUDE_DIRS}
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
  tasklet.c)
target_link_libraries(librwmain
  m
  rt
  ${RW_VX_LIBRARIES}
  ${CFLITE_LIBRARIES}
  ${GLIB_LIBRARIES}
  ${MSGPACK_LIBRARIES}
  ${PB_C_LIBRARIES}
  ${TALLOC_LIBRARIES}
  ${RWLIB_LIBRARIES}
  ${COREDUMPER_LIBRARIES}
  python3

  reaper_client
  rwdts_api
  rwdtsproto
  dispatch
  rwlog
  rwmanifest_yang_gen
  rwmsg
  rwnetns
  rwsched
  rwschema_yang_gen
  rwtasklet
  rwtrace
  rwvcs
  rw_tasklet_plugin-1.0
  rw_vcs_yang_gen
  kv_light_api
  rwdts_redis

  rwshell-1.0
  rwshell_api)

set_target_properties(librwmain PROPERTIES
  PREFIX "")
install(TARGETS librwmain
  DESTINATION usr/${RIFT_LIBDIR}
  COMPONENT rwmain-1.0)

add_executable(rwmain
  main.c)
target_link_libraries(rwmain
  librwmain)

install(TARGETS rwmain
  DESTINATION usr/bin
  COMPONENT rwmain-1.0)



add_library(librwmain_gi SHARED
  rwmain_gi.c)
target_link_libraries(librwmain_gi
  librwmain
  rwcal_api)
set_target_properties(librwmain_gi PROPERTIES
  PREFIX "")
install(TARGETS librwmain_gi
  DESTINATION usr/${RIFT_LIBDIR}
  COMPONENT rwmain-1.0)

rift_add_introspection(rwmain_gi
  NAMESPACE RwMain
  VERSION 1.0
  VAPI_PREFIX RwMain
  NO_UNPREFIXED
  INCLUDE_PATHS
    ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/src/
    ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmsg/include/
    ${CMAKE_INSTALL_PREFIX}/usr/include/dispatch
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
  DEPENDS rwtasklet_gi librwmain_gi
  PACKAGES RwTypes-1.0 RwManifestYang-1.0 RwTasklet-1.0
  GI_PY_OVERRIDE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/rwmain_gi_override.py"
)

rift_install_vala_artifacts(
  COMPONENT RwMain-1.0
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
