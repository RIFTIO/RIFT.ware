# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tim Mortsolf
# Creation Date: 2014/05/01
# 

##
# Supress these warnings from plugins
##
rift_allow_compiler_warning(unused-parameter)
rift_allow_compiler_warning(missing-field-initializers)

##
# Set some RIFT specific variables for this cmake project
##
set(PLUGIN_NAME rwvcs_api-c)

##
# Set the name of the Vala source for this cmake project; for use later in this file
##
set(C_SOURCES rwvcs_api-c.c)

include_directories(BEFORE
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwvcs/rwmain)

##
# Adds a library target to be built from the source files listed in the command invocation
# STATIC, SHARED, or MODULE may be given to specify the type of library to be created
##
add_library(${PLUGIN_NAME} SHARED ${C_SOURCES})

target_link_libraries(${PLUGIN_NAME} PRIVATE
  librwmain
  rwcal_api
  rwdts_api
  rwlib
  rwmanifest_yang_gen
  rwsched
  rwschema_yang_gen
  rw_tasklet_plugin-1.0
  rwtrace
  rwvcs
  rwvcs_api-1.0
  rw_vcs_yang_gen

  CoreFoundation
  glib-2.0
  peas-1.0)

##
# This function creates a vapi2c target to generate Vala source/header files from .in files
##
rift_add_plugin_vapi2c(
  ${PLUGIN_NAME}.vapi2c
  VAPI_FILE ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/ra/plugins/vala/rwvcs_api-1.0.vapi
  PLUGIN_SOURCE_PREFIX ${PLUGIN_NAME}
  DEPENDS rwvcs_api-1.0
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
