# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tim Mortsolf / Anil Gunturu
# Creation Date: 8/29/2013
# 

##
# Set some RIFT specific VALA variables for this cmake project
##
set(VALA_NAME vala_gi)
set(VALA_FILES ${VALA_NAME}.vala)
set(VALA_VERSION 1.0)
set(VALA_RELEASE 1)
set(VALA_LONG_NAME ${VALA_NAME}-${VALA_VERSION})
set(VALA_TYPELIB_PREFIX ValaGi-${VALA_VERSION})

##
# Add any additional include directories here
##
include_directories(BEFORE ${RIFT_SUBMODULE_SOURCE_ROOT}/rwlib/plugins/example_gi)

##
# This function compiles all the vala sources
# Generates the package .h/.c sources, .gir file, .vapi file, and the .so
##

rift_add_vala(
  ${VALA_LONG_NAME}
  VALA_FILES ${VALA_FILES}
  VALA_PACKAGES example_gi-1.0
  # Need to specify this if accessing the generated VAPIs in the same
  # submodule. For VAPIs from other submodules it will be picked up 
  # auto magically from install directory.
  VAPI_DIRS ${RIFT_SUBMODULE_BINARY_ROOT}/rwlib/plugins/example_gi
  GIR_PATHS  ${RIFT_SUBMODULE_BINARY_ROOT}/rwlib/plugins/example_gi
  GENERATE_HEADER_FILE ${VALA_NAME}.h
  GENERATE_SO_FILE lib${VALA_LONG_NAME}.so
  GENERATE_VAPI_FILE ${VALA_LONG_NAME}.vapi
  GENERATE_GIR_FILE ${VALA_TYPELIB_PREFIX}.gir
  GENERATE_TYPELIB_FILE ${VALA_TYPELIB_PREFIX}.typelib
  DEPENDS example_gi_
  )

##
# This function creates an install target for artifacts generated from the vala files
##
rift_install_vala_artifacts(
  COMPONENT ${PKG_LONG_NAME}
  HEADER_FILES ${VALA_NAME}.h
  SO_FILES lib${VALA_LONG_NAME}.so
  VAPI_FILES ${VALA_LONG_NAME}.vapi
  GIR_FILES ${VALA_TYPELIB_PREFIX}.gir
  TYPELIB_FILES ${VALA_TYPELIB_PREFIX}.typelib
  DEST_PREFIX .
  )

# Recurse into the subdirectories 
# This does not actually cause another cmake executable to run
# The same process will walk through the project's entire directory structure
set(subdirs 
  ${VALA_NAME}-python 
  )
rift_add_subdirs(SUBDIR_LIST ${subdirs})
