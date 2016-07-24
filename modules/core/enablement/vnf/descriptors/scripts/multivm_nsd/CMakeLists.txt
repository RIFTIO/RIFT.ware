# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Sanil Puthiyandyil
# Creation Date: 03/26/2014
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME multivm_nsd)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/generate_nsd_packages.sh.in
  ${CMAKE_CURRENT_BINARY_DIR}/generate_nsd_packages.sh
  ESCAPE_QUOTES @ONLY
  )

add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_trafsink_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_slb_trafsink_2VM_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/tg_autoscaler_ts_1VM_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1a_tg_ts_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1a_9vm_tg_ts_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1b_tg_ts_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1b_9vm_tg_ts_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_3_tg_slb_ts_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_3_tg_destnat_ts_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/cag_pgw_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_2a_cag_pgw_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_2b_cag_pgw_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_trafsink_raw_multivm_nsd.tar.gz

  COMMAND
    ${CMAKE_CURRENT_BINARY_DIR}/generate_nsd_packages.sh

  DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/rw_multivm_nsd.py
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/trafgen_1VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/trafgen_2VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/trafgen_3VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/trafsink_2VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/slb_2VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/trafsink_1VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/autoscaler_1VM_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/dkpi_1a_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/dkpi_1a_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/dkpi_1a_9vm_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/dkpi_1a_9vm_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/dkpi_1b_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/dkpi_1b_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/dkpi_1b_9vm_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/dkpi_1b_9vm_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/dkpi_2a_cag_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/dkpi_2a_pgw_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/dkpi_2b_cag_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/dkpi_2b_pgw_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/dkpi_3_trafgen_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/dkpi_3_trafsink_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/dkpi_3_slb_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/dkpi_3_destnat_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/cag_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/pgw_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/trafgen_raw_vnfd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/../multivm_vnfd/trafsink_raw_vnfd.tar.gz
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/trafgen_trafsink
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/trafgen_trafsink_2VM
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/trafgen_trafsink_raw
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/trafgen_slb_trafsink_2VM
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/tg_autoscaler_ts_1VM
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_1a_tg_ts
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_1a_9vm_tg_ts
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_1b_tg_ts
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_1b_9vm_tg_ts
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_2a_cag_pgw
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_2b_cag_pgw
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/dkpi_3_tg_slb_ts
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/cag_pgw
    ${CMAKE_CURRENT_SOURCE_DIR}/config/tg_ts_trafgen
    ${CMAKE_CURRENT_SOURCE_DIR}/config/tg_ts_trafsink
    ${CMAKE_CURRENT_SOURCE_DIR}/config/slb_trafgen
    ${CMAKE_CURRENT_SOURCE_DIR}/config/slb_slb
    ${CMAKE_CURRENT_SOURCE_DIR}/config/slb_destnat
    ${CMAKE_CURRENT_SOURCE_DIR}/config/slb_trafsink
    ${CMAKE_CURRENT_SOURCE_DIR}/config/autoscaler_trafgen
    ${CMAKE_CURRENT_SOURCE_DIR}/config/autoscaler_slb
    ${CMAKE_CURRENT_SOURCE_DIR}/config/autoscaler_trafsink
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/tgtraffic_startstop.py
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/iketraffic_startstop.py
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/iketraffic_startstop_2a.py
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/iketraffic_startstop_2b.py
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/setreset_cat.py
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/stream_startstop.py
  )

add_custom_target(multivm_nsd ALL
  DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_trafsink_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_slb_trafsink_2VM_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/tg_autoscaler_ts_1VM_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1a_tg_ts_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1a_9vm_tg_ts_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1b_tg_ts_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1b_9vm_tg_ts_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_2a_cag_pgw_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_2b_cag_pgw_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_3_tg_slb_ts_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_3_tg_destnat_ts_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/cag_pgw_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_SOURCE_DIR}/generate_nsd_packages.sh.in
  )

add_dependencies(multivm_nsd multivm)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_trafsink_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_slb_trafsink_2VM_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/tg_autoscaler_ts_1VM_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1a_tg_ts_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1a_9vm_tg_ts_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1b_tg_ts_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_1b_9vm_tg_ts_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_2a_cag_pgw_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_2b_cag_pgw_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_3_tg_slb_ts_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/dkpi_3_tg_destnat_ts_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/cag_pgw_multivm_nsd.tar.gz
    ${CMAKE_CURRENT_BINARY_DIR}/trafgen_trafsink_raw_multivm_nsd.tar.gz
  DESTINATION
    usr/rift/mano/nsds/multivm_nsd
    COMPONENT ${PKG_LONG_NAME}
  )

rift_python_install_tree(
  COMPONENT ${PKG_LONG_NAME}
  FILES
    rift/mano/nsds/rw_multivm_nsd.py
  )
