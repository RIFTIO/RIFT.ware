# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 8/26/2013
# 

cmake_minimum_required(VERSION 2.8)

##
# standard_plugin.vala
#
# Set some RIFT specific VALA variables for this cmake project
##
set(VALA_NAME standard_plugin)
set(VALA_FILES ${VALA_NAME}.vala)
set(VALA_VERSION 1.0)
set(VALA_RELEASE 1)
set(VALA_LONG_NAME ${VALA_NAME}-${VALA_VERSION})
set(VALA_TYPELIB_PREFIX StandardPlugin-${VALA_VERSION})

##
# This function compiles all the vala sources
# Generates the package .h/.c sources, .gir file, .vapi file, and the .so
##
rift_add_vala(
  ${VALA_LONG_NAME}
  VALA_FILES ${VALA_FILES}
  VALA_PACKAGES rwplugin-1.0
  VAPI_DIRS ${RIFT_SUBMODULE_BINARY_ROOT}/rwlib/plugins/rwplugin/vala
  GIR_PATHS ${RIFT_SUBMODULE_BINARY_ROOT}/rwlib/plugins/rwplugin/vala
  GENERATE_HEADER_FILE ${VALA_NAME}.h
  GENERATE_SO_FILE lib${VALA_LONG_NAME}.so
  GENERATE_VAPI_FILE ${VALA_LONG_NAME}.vapi
  GENERATE_GIR_FILE ${VALA_TYPELIB_PREFIX}.gir
  GENERATE_TYPELIB_FILE ${VALA_TYPELIB_PREFIX}.typelib
  )

##
# This function creates an install target for artifacts generated from the vala files
##
rift_install_vala_artifacts(
  HEADER_FILES ${VALA_NAME}.h
  SO_FILES lib${VALA_LONG_NAME}.so
  VAPI_FILES ${VALA_LONG_NAME}.vapi
  GIR_FILES ${VALA_TYPELIB_PREFIX}.gir
  TYPELIB_FILES ${VALA_TYPELIB_PREFIX}.typelib
  COMPONENT ${PKG_LONG_NAME}
  DEST_PREFIX .
  )

# Recurse into the subdirectories 
# This does not actually cause another cmake executable to run
# The same process will walk through the project's entire directory structure
set(subdirs 
  ${VALA_NAME}-c 
  ${VALA_NAME}-python 
  # ${VALA_NAME}-js
  )
rift_add_subdirs(SUBDIR_LIST ${subdirs})

