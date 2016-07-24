# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tim Mortsolf
# Creation Date: 3/20/2014
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwcli)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

##
# Add flags to the compiler command line for sources in the 
# current directory and below
##
add_definitions(-DINSTALLDIR="${CMAKE_INSTALL_PREFIX}")

##
# Add the given directories to those searched by the compiler for include files
# By default the directories are appended onto the current list of directories
##
include_directories(BEFORE
  ${CMAKE_CURRENT_BINARY_DIR}/../vala
  ${CMAKE_CURRENT_SOURCE_DIR}/../vala
  /usr/include/libxml2)


##
# Add the target for rwcli library
# This library doesn't need to link with tasklet framework
# libraries, so can be used in google tests directly
##
set(C_SOURCES rwcli_schema.cpp rwcli.cpp)
add_library(rwcli SHARED ${C_SOURCES})
add_dependencies(rwcli rwcli_plugin-1.0)
install(TARGETS rwcli LIBRARY DESTINATION usr/lib 
  COMPONENT ${PKG_LONG_NAME})
target_link_libraries(rwcli
  glib-2.0 
  rwvcs 
  rwuagentproto 
  rwuagent_msg_client
  CoreFoundation 
  rwcli_plugin-1.0
  boost_filesystem
  boost_system)


##
# Add the target for the rwclitasklet library
##
set(C_SOURCES rwcli.c)
add_library(rwclitasklet STATIC ${C_SOURCES})
target_link_libraries(rwclitasklet 
  glib-2.0 
  rwvcs 
  rwuagentproto 
  rwuagent_msg_client
  CoreFoundation 
  rwcli
  rwcli_plugin-1.0)
add_dependencies(rwclitasklet rwcli rwlog)
