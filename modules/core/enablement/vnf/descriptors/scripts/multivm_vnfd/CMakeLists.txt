# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Sanil Puthiyandyil
# Creation Date: 03/26/2014
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME multivm)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/generate_packages.sh.in
  ${CMAKE_CURRENT_BINARY_DIR}/generate_packages.sh
  ESCAPE_QUOTES @ONLY
  )


add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_1VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_2VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_3VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_3VM_2FAB_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafsink_2VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/slb_2VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafsink_1VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/autoscaler_1VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1a_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1a_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1a_9vm_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1a_9vm_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1b_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1b_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1b_9vm_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1b_9vm_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_3_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_3_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_3_slb_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_3_destnat_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/cag_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/pgw_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_2a_cag_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_2a_pgw_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_2b_cag_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_2b_pgw_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_raw_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafsink_raw_vnfd.tar.gz

  COMMAND
    ${CMAKE_CURRENT_BINARY_DIR}/generate_packages.sh

  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/rw_multivm_vnfd.py
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/trafgen_1VM
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/trafgen_2VM
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/trafgen_3VM
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/trafgen_3VM_2FAB
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/trafsink_2VM
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/slb_2VM
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/trafsink_1VM
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/autoscaler_1VM
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_1a_trafgen
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_1a_trafsink
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_1a_9vm_trafgen
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_1a_9vm_trafsink
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_1b_trafgen
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_1b_trafsink
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_1b_9vm_trafgen
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_1b_9vm_trafsink
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/cag
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/pgw
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_3_trafgen
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_3_trafsink
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_3_slb
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_3_destnat
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_2a_cag
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_2a_pgw
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_2b_cag
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_2b_pgw
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/trafgen_raw
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/trafsink_raw
    ${CMAKE_CURRENT_SOURCE_DIR}/config/1a1b_trafgen
    ${CMAKE_CURRENT_SOURCE_DIR}/config/1a1b_trafsink
    ${CMAKE_CURRENT_SOURCE_DIR}/config/1b_trafgen
    ${CMAKE_CURRENT_SOURCE_DIR}/config/1b_9vm_trafgen
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_3_trafgen_cfg
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_3_trafsink_cfg
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_3_slb_cfg
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_3_destnat_cfg
    ${CMAKE_CURRENT_SOURCE_DIR}/config/cag
    ${CMAKE_CURRENT_SOURCE_DIR}/config/pgw
    ${CMAKE_CURRENT_SOURCE_DIR}/config/autoscaler_slb
    ${CMAKE_CURRENT_SOURCE_DIR}/config/autoscaler_trafgen
    ${CMAKE_CURRENT_SOURCE_DIR}/config/autoscaler_trafsink
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_3_trafsink_cfg
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_3_slb_cfg
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_3_destnat_cfg
    ${CMAKE_CURRENT_SOURCE_DIR}/config/slb_slb
    ${CMAKE_CURRENT_SOURCE_DIR}/config/slb_trafgen
    ${CMAKE_CURRENT_SOURCE_DIR}/config/slb_trafsink
    ${CMAKE_CURRENT_SOURCE_DIR}/config/tg_ts_trafgen
    ${CMAKE_CURRENT_SOURCE_DIR}/config/tg_ts_trafsink
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_2a_cag_cfg
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_2a_cag_cfg2
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_2a_cag_cfg3
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_2a_cag_cfg4
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_2a_pgw_cfg
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_2a_pgw_cfg2
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_2a_pgw_cfg3
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_2a_pgw_cfg4
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_2b_cag_cfg
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_2b_cag_cfg2
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_2b_cag_cfg3
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_2b_cag_cfg4
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_2b_pgw_cfg
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_2b_pgw_cfg2
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_2b_pgw_cfg3
    ${CMAKE_CURRENT_SOURCE_DIR}/config/dkpi_2b_pgw_cfg4
    ${CMAKE_CURRENT_SOURCE_DIR}/config/tg_ts_trafgen_raw
    ${CMAKE_CURRENT_SOURCE_DIR}/config/tg_ts_trafsink_raw
  )


add_custom_target(multivm ALL
  DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_1VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_2VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_3VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_3VM_2FAB_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafsink_2VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/slb_2VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafsink_1VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/autoscaler_1VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1a_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1a_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1a_9vm_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1a_9vm_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1b_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1b_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1b_9vm_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1b_9vm_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_3_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_3_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_3_slb_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_3_destnat_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/cag_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/pgw_vnfd.tar.gz
    ${CMAKE_CURRENT_SOURCE_DIR}/generate_packages.sh.in
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_2a_cag_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_2a_pgw_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_2b_cag_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_2b_pgw_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_raw_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafsink_raw_vnfd.tar.gz
  )

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_1VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_2VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_3VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_3VM_2FAB_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafsink_2VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/slb_2VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafsink_1VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/autoscaler_1VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1a_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1a_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1a_9vm_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1a_9vm_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1b_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1b_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1b_9vm_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1b_9vm_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_3_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_3_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_3_slb_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_3_destnat_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/cag_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/pgw_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_2a_cag_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_2a_pgw_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_2b_cag_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_2b_pgw_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_raw_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafsink_raw_vnfd.tar.gz
  DESTINATION
    usr/rift/mano/vnfds/multivm
    COMPONENT ${PKG_LONG_NAME}
  )

rift_python_install_tree(
  COMPONENT ${PKG_LONG_NAME}
  FILES
    rift/mano/vnfds/rw_multivm_vnfd.py
  )
