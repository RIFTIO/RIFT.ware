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

set(BASE_INSTALL_NAME rwbase)
set(TOOLCHAIN_INSTALL_NAME rwtoolchain)
set(VM_IMAGE_INSTALL_NAME rwcontainertools)

rift_set_component_package_fields(
  "${TOOLCHAIN_INSTALL_NAME}"
  DESCRIPTION "RIFT.ware toolchain"
  )

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

rift_set_component_package_fields(
  "${BASE_INSTALL_NAME}"
  DESCRIPTION "RIFT.ware toolchain"
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
  )


# vim: ts=2 et:
