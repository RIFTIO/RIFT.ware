# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tom Seidenberg
# Creation Date: 2014/04/03
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwuagent)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

##
# Add flags to the compiler command line for sources in the 
# current directory and below
##
add_definitions(-DINSTALLDIR="${CMAKE_INSTALL_PREFIX}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")


##
# Add the given directories to those searched by the compiler for include files
# By default the directories are appended onto the current list of directories
##
include_directories(BEFORE
  /usr/include/libxml2
  ${CMAKE_INSTALL_PREFIX}/usr/local/confd/include/)

##
# Add the target for the rwuagent library
##
set(src_files
  rwuagent_all.cpp
)

if(NOT RIFT_AGENT_BUILD STREQUAL "XML_ONLY")
  set(src_files ${src_files} rwuagent_confd_all.cpp)
endif()

add_library(rwuagent SHARED ${src_files})
add_dependencies(rwuagent 
  rwuagent_yang.schema 
  rwuagent_yang.headers 
  rwschema_yang.headers)

# link with all the libraries that rwuagent depends on
target_link_libraries(rwuagent
  xslt
  xml2
  rwmemlog
  rwdynschema
  rwuagent_yang_gen
  rwschema_yang_gen
  rwuagentproto
  rw_tasklet_plugin-1.0
  CoreFoundation
)

install(
  TARGETS rwuagent LIBRARY
  DESTINATION usr/lib
  COMPONENT ${PKG_LONG_NAME}
)
