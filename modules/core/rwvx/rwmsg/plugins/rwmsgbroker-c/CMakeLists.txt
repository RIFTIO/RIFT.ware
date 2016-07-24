# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tim Mortsolf / Anil Gunturu
# Creation Date: 3/17/2014
# 

##
# Set some RIFT specific variables for this cmake project
##
set(PLUGIN_NAME rwmsgbroker-c)

##
# Set the name of the Vala source for this cmake project; for use later in this file
##
set(C_SOURCES rwmsgbroker-c.c)

include_directories(BEFORE ${RIFT_SUBMODULE_SOURCE_ROOT}/rwvcs/include)

##
# Adds a library target to be built from the source files listed in the command invocation
# STATIC, SHARED, or MODULE may be given to specify the type of library to be created
##
add_library(${PLUGIN_NAME} SHARED ${C_SOURCES})

target_link_libraries(${PLUGIN_NAME} PRIVATE
  rwmsgbroker
  rwmsgbrokeruagent
  rwlib
  rwmsg_yang_gen
  rw_tasklet_plugin-1.0
  rwtasklet
  rwtrace
  rwvcs

  talloc
  glib-2.0
  peas-1.0)

##
# This function creates a vapi2c target to generate Vala source/header files from .in files
##
rift_add_plugin_vapi2c(
  ${PLUGIN_NAME}.vapi2c
  VAPI_FILE ../../../rwvcs/plugins/vala/rw_tasklet_plugin-1.0.vapi
  PLUGIN_SOURCE_PREFIX ${PLUGIN_NAME}
  PLUGIN_PREFIX Rwmsgbroker
  DEPENDS rw_tasklet_plugin-1.0
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
