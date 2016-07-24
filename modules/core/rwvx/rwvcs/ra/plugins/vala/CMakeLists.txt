# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Austin Cormier
# Creation Date: 2014/04/30
# 

cmake_minimum_required(VERSION 2.8)
add_definitions(-std=gnu99 -D_GNU_SOURCE)

##
# rwvcs_api-1.0
#
# Set some RIFT specific VALA variables for this cmake project
##
set(VALA_NAME rwvcs_api)
set(VALA_FILES ${VALA_NAME}.vala)
set(VALA_VERSION 1.0)
set(VALA_RELEASE 1)
set(VALA_LONG_NAME ${VALA_NAME}-${VALA_VERSION})
set(VALA_TYPELIB_PREFIX RwvcsApi-${VALA_VERSION})

##
# rwvcs_api-1.0
#
# This function compiles all the vala sources
# Generates the package .h/.c sources, .gir file, .vapi file, and the .so
##
rift_add_vala(
  ${VALA_LONG_NAME}
  VALA_FILES ${VALA_FILES}
  VAPI_DIRS ${RIFT_VAPI_DIRS}
  VALA_PACKAGES rw_types-1.0
  GENERATE_HEADER_FILE ${VALA_NAME}.h
  GENERATE_SO_FILE lib${VALA_LONG_NAME}.so
  GENERATE_VAPI_FILE ${VALA_LONG_NAME}.vapi
  GENERATE_GIR_FILE ${VALA_TYPELIB_PREFIX}.gir
  GENERATE_TYPELIB_FILE ${VALA_TYPELIB_PREFIX}.typelib
  DEPENDS libpeas-${LIBPEAS_VERSION}
  )

##
# rwvcs_api-1.0
#
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

set(subdirs ${VALA_NAME}-c)
rift_add_subdirs(SUBDIR_LIST ${subdirs})
