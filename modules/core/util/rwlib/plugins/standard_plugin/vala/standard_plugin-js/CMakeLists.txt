# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 09/01/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

##
# Set some RIFT specific variables for this cmake project
##
set(VALA_NAME standard_plugin)
set(VALA_VERSION 1.0)
set(VALA_LONG_NAME ${VALA_NAME}-${VALA_VERSION})
set(PLUGIN_NAME ${VALA_NAME}-js)
set(PLUGIN_PREFIX ${VALA_NAME})

##
# This function builds the plugin schema files
##
rift_add_js_plugin_schema(
  ${PLUGIN_PREFIX}
  TARGET ${PLUGIN_NAME}
  JAVASCRIPT ${PLUGIN_NAME}.js
  GSCHEMA ${PLUGIN_NAME}.gschema.xml
  PLUGIN ${PLUGIN_NAME}.plugin
  )

