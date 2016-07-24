# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

include(rift_subdirs)
include(ExternalProject)

set(PKG_NAME uthash)
set(PKG_VERSION v1.9.9)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

unset(PKG_DEBUG_FLAGS)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(PKG_DEBUG_FLAGS "CFLAGS=-g -O0")
endif(CMAKE_BUILD_TYPE STREQUAL "Debug")        

set(UTHASH_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/${PKG_NAME}/src/utarray.h
    ${CMAKE_CURRENT_SOURCE_DIR}/${PKG_NAME}/src/uthash.h
    ${CMAKE_CURRENT_SOURCE_DIR}/${PKG_NAME}/src/utlist.h
    ${CMAKE_CURRENT_SOURCE_DIR}/${PKG_NAME}/src/utstring.h
    )

install(FILES ${UTHASH_FILES} DESTINATION usr/include COMPONENT uthash)
