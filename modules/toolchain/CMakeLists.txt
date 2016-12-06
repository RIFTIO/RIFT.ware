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

set(BASE_INSTALL_NAME rwbase)
set(TOOLCHAIN_INSTALL_NAME rwtoolchain)
set(VM_IMAGE_INSTALL_NAME rwcontainertools)

install(
  DIRECTORY cmake/
  DESTINATION cmake
  USE_SOURCE_PERMISSIONS
  COMPONENT ${TOOLCHAIN_INSTALL_NAME}
  )

##
# TODO: Remove this after fixing all the paths in core/mano ext/vnfs to point to RIFT_INSTALL/cmake
##
install(
  DIRECTORY cmake
  DESTINATION usr/rift/toolchain
  USE_SOURCE_PERMISSIONS
  COMPONENT ${TOOLCHAIN_INSTALL_NAME}
  )

install(
  DIRECTORY home/etc/
  DESTINATION etc
  USE_SOURCE_PERMISSIONS
  COMPONENT ${BASE_INSTALL_NAME}
  )

install(
  DIRECTORY home/.build/
  DESTINATION .build
  USE_SOURCE_PERMISSIONS
  COMPONENT ${BASE_INSTALL_NAME}
  )

install(
  DIRECTORY home/.artifacts/
  DESTINATION .artifacts
  USE_SOURCE_PERMISSIONS
  COMPONENT ${BASE_INSTALL_NAME}
  )

install(
  PROGRAMS
    home/get_platform.sh
    home/rift-bashrc
    home/rift-prompt
    home/rift-shell
    home/rift_env.py
  DESTINATION .
  COMPONENT ${BASE_INSTALL_NAME}
  )


##
# Include the subdirs
##
set(subdirs
    rwbase
)
rift_add_subdirs(SUBDIR_LIST ${subdirs})

##
# This macro adds targets for documentaion, unittests, code coverage and packaging
##
rift_add_submodule_targets(
  SUBMODULE_PACKAGE_NAME "rw.toolchain"
  DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/package-description.txt"
  DESCRIPTION_SUMMARY "RIFTware Toolchain Package"
  )


# vim: ts=2 et:
