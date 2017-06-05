# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 03/26/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

##
# DO NOT add any code before this and DO NOT
# include this file anywhere else
##
include(rift_submodule NO_POLICY_SCOPE)

rift_set_component_package_fields(
  "container-tools"
  DESCRIPTION "RIFT.ware tools to configure and manage containers"
  )

install(
  DIRECTORY container_tools/usr/rift/container_tools/
  DESTINATION container_tools
  USE_SOURCE_PERMISSIONS
  COMPONENT "container-tools"
  )

rift_set_component_package_fields(
  "scripts"
  DESCRIPTION "RIFT.ware scripts"
  )

install(
  DIRECTORY 
    scripts/usr/rift/bin/
  DESTINATION bin
  USE_SOURCE_PERMISSIONS
  COMPONENT "scripts"
  )

install(
  DIRECTORY 
    scripts/usr/rift/etc/
  DESTINATION etc
  USE_SOURCE_PERMISSIONS
  COMPONENT "scripts"
  )

rift_set_component_package_fields(
  "scripts-internal"
  DESCRIPTION "RIFT.ware internal scripts, not released externally"
  )

install(
  DIRECTORY 
    internal/usr/rift/bin/
  DESTINATION bin
  USE_SOURCE_PERMISSIONS
  COMPONENT "scripts-internal"
  )

install(
  DIRECTORY 
    internal/usr/rift/cloud/
  DESTINATION cloud
  USE_SOURCE_PERMISSIONS
  COMPONENT "scripts-internal"
  )

install(
  DIRECTORY 
    internal/usr/rift/env/
  DESTINATION env
  USE_SOURCE_PERMISSIONS
  COMPONENT "scripts-internal"
  )

install(
  DIRECTORY 
    internal/usr/rift/nagios/
  DESTINATION nagios
  USE_SOURCE_PERMISSIONS
  COMPONENT "scripts-internal"
  )

install(
  DIRECTORY 
    internal/usr/rift/setup/
  DESTINATION setup
  USE_SOURCE_PERMISSIONS
  COMPONENT "scripts-internal"
  )

##
# This macro adds targets for documentaion, unittests, code coverage and packaging
##
rift_add_submodule_targets(
  SUBMODULE_PACKAGE_NAME "rw.tools"
  )


# vim: ts=2 et:
