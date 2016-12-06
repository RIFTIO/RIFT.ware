# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
# Author(s): Anil Gunturu
# Creation Date: 03/26/2014
# 

cmake_minimum_required(VERSION 2.8)

##
# DO NOT add any code before this and DO NOT
# include this file anywhere else
##
include(rift_submodule)

install(
  DIRECTORY container_tools/usr/rift/container_tools/
  DESTINATION container_tools
  USE_SOURCE_PERMISSIONS
  COMPONENT "container-tools"
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
  DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/package-description.txt"
  DESCRIPTION_SUMMARY "RIFTware Tools Package"
  )


# vim: ts=2 et:
