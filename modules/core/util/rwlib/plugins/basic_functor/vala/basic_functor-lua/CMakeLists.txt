# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 09/01/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

set(VALA_NAME basic_functor)
set(VALA_VERSION 1.0)
set(VALA_LONG_NAME ${VALA_NAME}-${VALA_VERSION})
set(PLUGIN_NAME ${VALA_NAME}-lua)
set(PLUGIN_PREFIX ${VALA_NAME})

##
# This function builds the plugin schema files
##
rift_add_lua_plugin_schema(
  ${PLUGIN_PREFIX}
  TARGET ${PLUGIN_NAME}
  LUASCRIPT ${PLUGIN_NAME}.lua
  GSCHEMA ${PLUGIN_NAME}.gschema.xml
  PLUGIN ${PLUGIN_NAME}.plugin
  )

##
# This function creates an install target for the plugin artifacts
##
rift_install_plugin_artifacts(
  ${PLUGIN_NAME}
  GSCHEMAFILES 
  ${CMAKE_CURRENT_SOURCE_DIR}/${PLUGIN_NAME}.gschema.xml
  PLUGINFILES 
  ${CMAKE_CURRENT_SOURCE_DIR}/${PLUGIN_NAME}.plugin
  ${CMAKE_CURRENT_SOURCE_DIR}/${PLUGIN_NAME}.lua
  COMPONENT ${INSTALL_COMPONENT}
  )

