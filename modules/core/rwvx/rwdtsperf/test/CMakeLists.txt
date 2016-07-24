# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tim Mortsolf
# Creation Date: 2014/03/20
# 

cmake_minimum_required(VERSION 2.8)

add_custom_target(run_rwdtsperf
  COMMAND
    cd ${CMAKE_INSTALL_PREFIX} &&
    ${PROJECT_TOP_DIR}/scripts/env/envset.sh
      "GDB=1"
      "VERBOSE=1"
      "RWMAIN_CMDARGS=--collapse-all"
      ${CMAKE_INSTALL_PREFIX}/usr/bin/rwmain
        load manifest file ${CMAKE_CURRENT_SOURCE_DIR}/rw-manifest.xml
)
