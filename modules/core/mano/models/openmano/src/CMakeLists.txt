# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 2014/12/11
# 

cmake_minimum_required(VERSION 2.8)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/generate_tidgen_packages.sh.in
  ${CMAKE_CURRENT_BINARY_DIR}/generate_tidgen_packages.sh
  ESCAPE_QUOTES @ONLY
  )

add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/2tidgenMWC_2sriov.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/tidgenMWC_2sriov.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/2tidgenMWC_4sriov.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/tidgenMWC_4sriov.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/2tidgenMWC_2sriov_no_ctrlnet.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/tidgenMWC_2sriov_no_ctrlnet.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/2tidgenMWC_4sriov_no_ctrlnet.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/tidgenMWC_4sriov_no_ctrlnet.tar.gz

    
  COMMAND
    ${CMAKE_CURRENT_BINARY_DIR}/generate_tidgen_packages.sh

  DEPENDS 
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/openmano/test/tidgen_ns_2sriov.yaml
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/openmano/test/tidgen_ns_4sriov.yaml
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/openmano/test/tidgen_vnf_2sriov.yaml
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/openmano/test/tidgen_vnf_4sriov.yaml
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/openmano/test/tidgen_ns_2sriov_no_ctrlnet.yaml
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/openmano/test/tidgen_ns_4sriov_no_ctrlnet.yaml
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/openmano/test/tidgen_vnf_2sriov_no_ctrlnet.yaml
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/openmano/test/tidgen_vnf_4sriov_no_ctrlnet.yaml
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/openmano/src/openmano2rift.py
  )

add_custom_target(tidgen ALL
  DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/2tidgenMWC_2sriov.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/tidgenMWC_2sriov.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/2tidgenMWC_4sriov.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/tidgenMWC_4sriov.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/2tidgenMWC_2sriov_no_ctrlnet.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/tidgenMWC_2sriov_no_ctrlnet.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/2tidgenMWC_4sriov_no_ctrlnet.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/tidgenMWC_4sriov_no_ctrlnet.tar.gz
    mano_yang
  )

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/2tidgenMWC_4sriov.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/tidgenMWC_4sriov.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/2tidgenMWC_2sriov.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/tidgenMWC_2sriov.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/2tidgenMWC_4sriov_no_ctrlnet.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/tidgenMWC_4sriov_no_ctrlnet.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/2tidgenMWC_2sriov_no_ctrlnet.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/tidgenMWC_2sriov_no_ctrlnet.tar.gz

    
  DESTINATION
    usr/rift/mano/examples/tidgen_ns
    COMPONENT ${PKG_LONG_NAME}
  )
