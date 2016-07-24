# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Creation Date: Thu Mar 19 13:20:43 IST 2015
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME estw)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

# IMHO this should just be the default
rift_werror()

##
# This appends to the list of directories to search for
# header files. INCLUDE_DIRECTORIES is a DIRECTORY property and
# is inherited from the parent directory.
#
include_directories(BEFORE
  ${RIFT_SUBMODULE_SOURCE_ROOT}/libtimer/estw/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/libtimer/safe_base/include
  ${CMAKE_CURRENT_BINARY_DIR})

set(LIB_SOURCES
  src/stw_system_timer.c
  src/stw_timer.c
  )
##
# Add the target for the estw static library
##
add_library(estw SHARED ${LIB_SOURCES})

##
# The following code installs the targets to
# ${CMAKE_INSTALL_PREFIX}/usr/bin.
# Always use relative path for destination.
set(INSTALL_HFILES
  include/stw_system_timer.h
  include/stw_system_timer.h
  include/stw_timer.h
  ../safe_base/include/safe_limits.h  
  ../safe_base/include/safe_types.h
  )
install(FILES ${INSTALL_HFILES} DESTINATION usr/include COMPONENT ${PKG_LONG_NAME})
install(TARGETS estw LIBRARY DESTINATION usr/lib COMPONENT ${PKG_LONG_NAME})

