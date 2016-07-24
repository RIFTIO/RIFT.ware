# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tim Mortsolf / Anil Gunturu
# Creation Date: 3/17/2014
# 

##
# Include rift specific plugin definitions
##
include(rift_plugin)

##
# Supress these warnings from plugins
##
rift_allow_compiler_warning(unused-parameter)
rift_allow_compiler_warning(missing-field-initializers)

##
# Set some RIFT specific variables for this cmake project
##
set(PLUGIN_NAME rwcli-c)

##
# Set the name of the Vala source for this cmake project; for use later in this file
##
set(C_SOURCES rwcli-c.c)

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
  ${RIFT_SUBMODULE_BINARY_ROOT}/rwcli/src/librwclitasklet.a
  ${RIFT_SUBMODULE_BINARY_ROOT}/rwcli/src/librwcli.so
  ${RIFT_SUBMODULE_BINARY_ROOT}/rwcli/proto/librwcliproto.a
  ${RIFT_SUBMODULE_BINARY_ROOT}/rwcli/vala/librwcli_plugin-1.0.so
  ${RIFT_SUBMODULE_BINARY_ROOT}/rwuagent/client/librwuagent_msg_client.so
  ${CMAKE_INSTALL_PREFIX}/usr/lib/librwmsg.so
  ${CMAKE_INSTALL_PREFIX}/usr/lib/librwlogdproto.so
  ${CMAKE_INSTALL_PREFIX}/usr/lib/libnanomsg.so)

add_dependencies(${PLUGIN_NAME} rwclitasklet rwcliproto rwlogdproto rwuagent_msg_client)

##
# This function creates a vapi2c target to generate Vala source/header files from .in files
##
rift_add_plugin_vapi2c(
  ${PLUGIN_NAME}.vapi2c
  VAPI_FILE ${CMAKE_INSTALL_PREFIX}/usr/share/rift/vapi/rw_tasklet_plugin-1.0.vapi
  PLUGIN_SOURCE_PREFIX ${PLUGIN_NAME}
  PLUGIN_PREFIX Rwcli
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
  COMPONENT ${PKG_LONG_NAME}
  )
