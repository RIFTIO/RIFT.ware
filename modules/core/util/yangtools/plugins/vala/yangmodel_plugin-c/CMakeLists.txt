# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#                                                                                                                              
# Author(s): Tim Mortsolf
# Creation Date: 8/29/2013
# 

##
# Set some RIFT specific variables for this cmake project
##
set(VALA_NAME yangmodel_plugin)
set(VALA_VERSION 1.0)
set(VALA_LONG_NAME ${VALA_NAME}-${VALA_VERSION})
set(PLUGIN_NAME ${VALA_NAME}-c)

##
# Set the name of the Vala source for this cmake project; for use later in this file
##
set(C_SOURCES ${PLUGIN_NAME}.c)

##
# Adds a library target to be built from the source files listed in the command invocation
# STATIC, SHARED, or MODULE may be given to specify the type of library to be created
##
add_library(${PLUGIN_NAME} SHARED ${C_SOURCES})

##
# Specify libraries or flags to use when linking a given target
# The target must have been created in the current directory by a command such as add_executable or add_library
##
target_link_libraries(${PLUGIN_NAME} PRIVATE
  yangmodel_plugin-1.0
  rwyang
  peas-1.0)


##
# This function creates a vapi2c target to generate Vala source/header files from .in files
##
rift_add_plugin_vapi2c(
  ${PLUGIN_NAME}.vapi2c
  VAPI_FILE ../${VALA_LONG_NAME}.vapi
  PLUGIN_SOURCE_PREFIX ${PLUGIN_NAME}
  DEPENDS ${VALA_LONG_NAME}
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
