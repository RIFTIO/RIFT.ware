# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf / Anil Gunturu
# Creation Date: 3/17/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

##
# Set some RIFT specific variables for this cmake project
##
set(PLUGIN_NAME rwlogd-c)

##
# Set the name of the Vala source for this cmake project; for use later in this file
##
set(C_SOURCES rwlogd-c.c)

##
# Add the given directories to those searched by the compiler for include files
# By default the directories are appended onto the current list of directories
##
include_directories(BEFORE
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdts/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/src
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmsg/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwvcs/plugins/vala
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwlog/rwlogd/include)

##
# Adds a library target to be built from the source files listed in the command invocation
# STATIC, SHARED, or MODULE may be given to specify the type of library to be created
##
add_library(${PLUGIN_NAME} SHARED ${C_SOURCES})

add_definitions(-std=gnu99 -D_GNU_SOURCE)
##
# Specify libraries or flags to use when linking a given target
# The target must have been created in the current directory by a command such as add_executable or add_library
##
target_link_libraries(${PLUGIN_NAME}
  rwlogdproto
  rwdts_api
  rwlogd_api
  rwschema_yang_gen
  rwlib
  rwmsg
  rwsched
  rwtasklet
  rwvcs
  rw_tasklet_plugin-1.0
  rwtrace
  CoreFoundation
  glib-2.0
  protobuf-c
  dispatch
  peas-1.0)

##
# libpeas must be compiled before this target
##
add_dependencies(${PLUGIN_NAME} rwlogd_api rwmsgbroker)

##
# This function creates a vapi2c target to generate Vala source/header files from .in files
##
rift_add_plugin_vapi2c(
  ${PLUGIN_NAME}.vapi2c
  VAPI_FILE ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/plugins/vala/rw_tasklet_plugin-1.0.vapi
  PLUGIN_SOURCE_PREFIX ${PLUGIN_NAME}
  PLUGIN_PREFIX Rwlogd
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
  COMPONENT ${INSTALL_COMPONENT}
  )
