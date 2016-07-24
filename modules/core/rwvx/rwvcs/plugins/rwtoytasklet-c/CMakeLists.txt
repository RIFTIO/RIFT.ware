
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

set(PLUGIN_NAME rwtoytasklet-c)


rift_add_plugin_vapi2c(
  ${PLUGIN_NAME}.vapi2c
  VAPI_FILE
    ../../../rwvcs/plugins/vala/rw_tasklet_plugin-1.0.vapi
  PLUGIN_SOURCE_PREFIX ${PLUGIN_NAME}
  PLUGIN_PREFIX Rwtoytasklet
  DEPENDS rw_tasklet_plugin-1.0
)

add_library(${PLUGIN_NAME} SHARED rwtoytasklet-c.c)
target_link_libraries(${PLUGIN_NAME} PRIVATE
  rwlib
  rwmsg
  rwsched
  rw_tasklet_plugin-1.0
  rwtrace
  rwtoytaskletproto
  rwtasklet
  rwvcs

  CoreFoundation
  glib-2.0
  protobuf-c
  peas-1.0)

install(TARGETS ${PLUGIN_NAME}
  DESTINATION usr/lib/rift/plugins/${PLUGIN_NAME}
  COMPONENT ${PKG_LONG_NAME})

install(FILES
    rwtoytasklet-c.plugin
  DESTINATION usr/lib/rift/plugins/pytoytasklet
  COMPONENT ${PKG_LONG_NAME})
