# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
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
# Set the path to the top level cmake modules directory
##
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/modules")

##
# DO NOT add any code before this and DO NOT
# include this file anywhere else
##
include(rift_submodule)

##
# Submodule specific includes will go here, 
# These are specified here, since these variables are accessed
# from multiple sub directories. If the variable is subdirectory
# specific it must be declared in the subdirectory.
##
# set(GLIB_VERSION 2.40.0)
# set(GI_VERSION 1.40.0)
# set(PYGOBJECT_VERSION 3.12.3)
# set(LIBPEAS_VERSION 1.10.0)
#set(VALA_VERSION 0.26)

set(GLIB_VERSION 2.46.0)
set(GI_VERSION 1.46.0)
set(PYGOBJECT_VERSION 3.18.0)
set(LIBPEAS_VERSION 1.12.1)
set(VALA_VERSION 0.31.1)

##
# Include the subdirs
##
set(subdirs 
  glib
  vala
  gi
  lgi
  pygobject
  libpeas
  )
rift_add_subdirs(SUBDIR_LIST ${subdirs})

##
# This macro adds targets for documentaion, unittests, code coverage and packaging
##
rift_add_submodule_targets(SUBMODULE_PACKAGE_NAME "rw.ext.gnome")
