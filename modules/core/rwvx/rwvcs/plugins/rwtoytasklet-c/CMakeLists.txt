
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
  COMPONENT ${INSTALL_COMPONENT})

install(FILES
    rwtoytasklet-c.plugin
  DESTINATION usr/lib/rift/plugins/pytoytasklet
  COMPONENT ${INSTALL_COMPONENT})
