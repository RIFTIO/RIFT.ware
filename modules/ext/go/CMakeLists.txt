
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/modules")

include(rift_submodule)
include(rift_externalproject)

rift_externalproject_add(
  go-pkgs
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/go-pkgs
  CONFIGURE_COMMAND echo
  BUILD_COMMAND
    GOPATH=<BINARY_DIR> make -C src/github.com/hashicorp/serf
  INSTALL_COMMAND echo
  BCACHE_COMMAND echo
)

set(dest ${CMAKE_INSTALL_PREFIX}/usr)
externalproject_add_step(
  go-pkgs
  real_install
  COMMAND mkdir -p ${dest}/bin
  COMMAND cp -r <BINARY_DIR>/bin/serf ${dest}/bin/
  DEPENDEES build
  DEPENDERS install
)

set(dest ${RIFT_SUBMODULE_INSTALL_PREFIX}/go-pkgs/${CMAKE_INSTALL_PREFIX}/usr)
externalproject_add_step(
  go-pkgs
  real_bcache
  COMMAND mkdir -p ${dest}/bin
  COMMAND cp -r <BINARY_DIR>/bin/serf ${dest}/bin
  DEPENDEES build
  DEPENDERS bcache
)


rift_add_submodule_targets(SUBMODULE_PACKAGE_NAME "rw.ext.go")
