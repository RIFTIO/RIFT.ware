# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

##
# DEPENDENCY ALERT
# The submodule dependencies must be specified in the
# .gitmodules.dep file at the top level (supermodule) directory
# If this submodule depends other submodules remember to update
# the .gitmodules.dep
##

cmake_minimum_required(VERSION 2.8)
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined ${CMAKE_SHARED_LINKER_FLAGS}")

set(INSTALL_COMPONENT "rwvx")

##
# DO NOT add any code before this and DO NOT
# include this file anywhere else
##
include(rift_submodule NO_POLICY_SCOPE)

##
# Submodule specific includes will go here,
# These are specified here, since these variables are accessed
# from multiple sub directories. If the variable is subdirectory
# specific it must be declared in the subdirectory.
##

##
# Include the subdirs
##
set(subdirs
  python
  rwdts
  rwha
  rwdtsperf
  rwdynschema
  rwlog
  rwmemlogdts
  rwmsg
  rwnetns
  rwsched
  rwschema
  rwvcs
  rwtermio
  rwshell
  rwdtsmockserver
  )
rift_add_subdirs(SUBDIR_LIST ${subdirs})

##
# Set up package details
##

rift_set_component_package_fields(
  "rwvx"
  DESCRIPTION "RIFT.ware rwvx"
  )

rift_set_component_package_fields(
  "rwdts"
  DESCRIPTION "RIFT.ware Core RwVx Package"
  )

##
# This macro adds targets for documentaion, unittests, code coverage and packaging
##
rift_add_submodule_targets(
  SUBMODULE_PACKAGE_NAME "rw.core.rwvx"
  )
