# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Sanil Puthiyandyil 
# Creation Date: 02/29/2016
# 

cmake_minimum_required(VERSION 2.8)

set(VNFCONTENT_INSTALL "demos/vnfcontents")

install(
  FILES
  multivmvnf.service
  DESTINATION ${VNFCONTENT_INSTALL}
  COMPONENT ${PKG_LONG_NAME})

install(
  FILES
    start_multi_vm_vnf.sh
    start_multivmvnf
    riftcli
  DESTINATION ${VNFCONTENT_INSTALL}
  COMPONENT ${PKG_LONG_NAME})
