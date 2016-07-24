# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Manish Patel
# Creation Date: 10/28/2015
# 

cmake_minimum_required(VERSION 2.8)

set(CONMAN_INSTALL "demos/conman")

install(
  FILES
    start_cm_system.py
    README.start_cm
  DESTINATION ${CONMAN_INSTALL}
  COMPONENT ${PKG_LONG_NAME})
  
# set(NS_NAME ping_pong_nsd)
# install(
#   FILES
#     ${NS_NAME}/configuration_input_params.yml
#     ${NS_NAME}/ping_vnfd_1_scriptconf_template.cfg
#     ${NS_NAME}/pong_vnfd_11_scriptconf_template.cfg
#   DESTINATION ${CONMAN_INSTALL}/${NS_NAME}
#   COMPONENT ${PKG_LONG_NAME})

