# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Rajesh Ramankuyyu
# Creation Date: 2015/05/20
# 

include(rift_yang)

##
# Build our yang files
##
set(yang_sources
  ${CMAKE_CURRENT_SOURCE_DIR}/rwshell-mgmt.yang)

rift_add_yang_target(
  TARGET rwshell-mgmt_yang
  YANG_FILES ${yang_sources}
  COMPONENT ${PKG_LONG_NAME}
  LIBRARIES
    rwschema_yang_gen
)
