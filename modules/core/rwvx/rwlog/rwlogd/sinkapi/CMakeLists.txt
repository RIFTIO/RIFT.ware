# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# 

cmake_minimum_required(VERSION 2.8)

##
# Set the path to the top level cmake modules directory
##
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/modules")

##
# DO NOT add any code before this and DO NOT
# include this file anywhere else
##


#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")

##
# Include the subdirs
##
set(subdirs 
  plugin/rwlogd_syslog_sink
  plugin/rwlogd_cli_sink
  src
  )
rift_add_subdirs(SUBDIR_LIST ${subdirs})

##
# This macro adds targets for documentaion, unittests, code coverage and packaging
##
