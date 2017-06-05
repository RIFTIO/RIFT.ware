# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf
# Creation Date: 12/2/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

set(GI_NAME CF)
set(GI_VERSION 1.0)
set(GI_NAMESPACE CF)
set(VAPI_PREFIX cf-${GI_VERSION})

add_library(cf_gi SHARED cf_gi.c)
target_link_libraries(cf_gi PRIVATE
  CoreFoundation
  glib-2.0
  gobject-2.0)
add_dependencies(cf_gi cflite)

install(
  TARGETS cf_gi
  LIBRARY
  DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})

rift_add_introspection(${GI_NAME}_gi
  NAMESPACE ${GI_NAMESPACE}
  VERSION ${GI_VERSION}
  GI_INCLUDE_HFILES CoreFoundation.h
  HFILES
    /usr/include/CoreFoundation/CFBase.h
    /usr/include/CoreFoundation/CFDate.h
    /usr/include/CoreFoundation/CFData.h
    /usr/include/CoreFoundation/CFSocket.h
  LIBRARIES CoreFoundation cf_gi
  IDENTIFIER_FILTER_CMD "python ${CMAKE_CURRENT_SOURCE_DIR}/cf_gi_filter.py"
  DEPENDS cf_gi
  BUILD_ALL
  )

##
# Install the generated gir and typelib files
##
rift_install_vala_artifacts(
  COMPONENT ${INSTALL_COMPONENT}
  GIR_FILES ${GI_NAMESPACE}-${GI_VERSION}.gir
  TYPELIB_FILES ${GI_NAMESPACE}-${GI_VERSION}.typelib
  LUA_OVERRIDES ${GI_NAMESPACE}.lua
  VAPI_FILES ${VAPI_PREFIX}.vapi
  DEST_PREFIX .
  )
