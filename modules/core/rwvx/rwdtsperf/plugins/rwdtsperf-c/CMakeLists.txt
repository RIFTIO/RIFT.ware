# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Baiju Parameswaran
# Creation Date: 5/8/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

##
# Include rift specific plugin definitions
##
include(rift_plugin)

##
# Supress these warnings from plugins
##
rift_allow_compiler_warning(unused-parameter)
rift_allow_compiler_warning(missing-field-initializers)

rift_pkg_check_modules(GLIB REQUIRED glib-2.0>=2.42.0)
#rift_pkg_check_modules(CFLITE REQUIRED CFLite>=0.1.2)
rift_pkg_check_modules(PEAS REQUIRED libpeas-1.0>=1.12.2)

include_directories(${GLIB_INCLUDE_DIRS} 
  ${PEAS_INCLUDE_DIRS})
##
# Set some RIFT specific variables for this cmake project
##
set(PLUGIN_NAME rwdtsperf-c)

##
# Set the name of the Vala source for this cmake project; for use later in this file
##
set(C_SOURCES rwdtsperf-c.c)

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
target_link_libraries(${PLUGIN_NAME} PRIVATE
  rwdtsperf
  rwlib
  rwmsg
  rwsched
  rw_tasklet_plugin-1.0
  rwtrace
  rwvcs
  CoreFoundation
  ${GLIB_LIBRARIES}
  # RIFT-5092 - this should be using pkg_config
  protobuf-c
  #  ${PROTOBUF_C_LIBRARIES}
  # RIFT-5112 - using the libpeas suggested libraries causes
  # downstream failures in unittest.
  peas-1.0
  #${PEAS_LIBRARIES}
  ${RIFT_SUBMODULE_BINARY_ROOT}/rwdtsperf/src/librwdtsperf.a)


#target_link_libraries(${PLUGIN_NAME} nanomsg)

##
# This function creates a vapi2c target to generate Vala source/header files from .in files
##
rift_add_plugin_vapi2c(
  ${PLUGIN_NAME}.vapi2c
  VAPI_FILE ../../../rwvcs/plugins/vala/rw_tasklet_plugin-1.0.vapi
  PLUGIN_SOURCE_PREFIX ${PLUGIN_NAME}
  PLUGIN_PREFIX Rwdtsperf
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
