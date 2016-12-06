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
# Author(s): Tom Seidenberg
# Creation Date: 2014/04/03
# 

##
# Supress these warnings from plugins
##
rift_allow_compiler_warning(unused-parameter)
rift_allow_compiler_warning(missing-field-initializers)

##
# Set some RIFT specific variables for this cmake project
##
set(PLUGIN_NAME rwuagent-c)

##
# Set the name of the Vala source for this cmake project; for use later in this file
##
set(C_SOURCES rwuagent-c.c)

##
# Add the given directories to those searched by the compiler for include files
# By default the directories are appended onto the current list of directories
##
add_definitions(-DINSTALLDIR="${CMAKE_INSTALL_PREFIX}")

##
# Adds a library target to be built from the source files listed in the command invocation
# STATIC, SHARED, or MODULE may be given to specify the type of library to be created
##
add_library(${PLUGIN_NAME} SHARED ${C_SOURCES})

##
# Specify libraries or flags to use when linking a given target
# The target must have been created in the current directory by a command such as add_executable or add_library
##
target_link_libraries(${PLUGIN_NAME}
  rwuagent
  rwuagent_yang_gen
  rwuagentproto
  rwmsg
  rwyang
  ncx
  nanomsg
  rwdts_api
  CoreFoundation
  ${CMAKE_INSTALL_PREFIX}/usr/lib/rift/plugins/librw_tasklet_plugin-1.0.so
  rwvcs
  rwtasklet
  rwsched

  # System libraries
  xml2
  )

if(NOT RIFT_AGENT_BUILD STREQUAL "XML_ONLY")
  target_link_libraries(${PLUGIN_NAME} 
    rwconfd
    ${CMAKE_INSTALL_PREFIX}/usr/local/confd/lib/libconfd.so
  )
endif()


add_dependencies(${PLUGIN_NAME} rwuagent rwuagentproto rwuagent_yang_pb )

##
# This function creates a vapi2c target to generate Vala source/header files from .in files
##
rift_add_plugin_vapi2c(
  ${PLUGIN_NAME}.vapi2c
  VAPI_FILE ${CMAKE_INSTALL_PREFIX}/usr/share/rift/vapi/rw_tasklet_plugin-1.0.vapi
  PLUGIN_SOURCE_PREFIX ${PLUGIN_NAME}
  PLUGIN_PREFIX Rwuagent
  )

##
# This function builds the plugin schema files
##
rift_add_c_plugin_schema(
  ${PLUGIN_NAME}
  GSCHEMA ${PLUGIN_NAME}.gschema.xml
  PLUGIN ${PLUGIN_NAME}.plugin
  )

##
# This function creates an install target for the plugin artifacts
##
rift_install_plugin_artifacts(
  ${PLUGIN_NAME}
  SOTARGETS ${PLUGIN_NAME}
  GSCHEMAFILES ${CMAKE_CURRENT_BINARY_DIR}/${PLUGIN_NAME}.gschema.xml
  PLUGINFILES ${CMAKE_CURRENT_BINARY_DIR}/${PLUGIN_NAME}.plugin
  COMPONENT ${INSTALL_COMPONENT}
  )
