# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Kiran Kashalkar
# Creation Date: 08/18/2015
# 

##
# DEPENDENCY ALERT
# The submodule dependencies must be specified in the
# .gitmodules.dep file at the top level (supermodule) directory
# If this submodule depends other submodules remember to update
# the .gitmodules.dep
##

cmake_minimum_required(VERSION 2.8)

##
# Submodule specific includes will go here,
# These are specified here, since these variables are accessed
# from multiple sub directories. If the variable is subdirectory
# specific it must be declared in the subdirectory.
##

rift_externalproject_add(
  skyquake
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
  CONFIGURE_COMMAND echo
  BUILD_COMMAND
    npm install
  INSTALL_COMMAND
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/install_skyquake.sh
    ${CMAKE_CURRENT_BINARY_DIR}/skyquake/skyquake-build
    ${CMAKE_INSTALL_PREFIX}/usr/share/rw.ui/skyquake
    ${RIFT_SUBMODULE_INSTALL_PREFIX}/skyquake/${CMAKE_INSTALL_PREFIX}/usr/share/rw.ui/skyquake

  BCACHE_COMMAND echo
)


##
# Include the subdirs
##
set(
  subdirs
    plugins
  )
rift_add_subdirs(
  SUBDIR_LIST 
    ${subdirs}
  )
