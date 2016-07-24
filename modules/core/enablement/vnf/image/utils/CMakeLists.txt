# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Sanil Puthiyandyil 
# Creation Date: 02/29/2016
# 

cmake_minimum_required(VERSION 2.8)

set(VNFTOOLS_INSTALL "demos/vnftools")

install(
  FILES
  prepare_multivm_qcow.sh
  DESTINATION ${VNFTOOLS_INSTALL}
  COMPONENT ${PKG_LONG_NAME})

