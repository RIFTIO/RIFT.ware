# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tim Mortsolf
# Creation Date: 2014/05/07
# 

include(rift_yang)

##
# Build our yang files
##
set(yang_sources
  ${CMAKE_CURRENT_SOURCE_DIR}/rwlog-mgmt.yang)

rift_add_yang_target(
  TARGET rwlog-mgmt_yang
  YANG_FILES ${yang_sources}
  COMPONENT ${PKG_LONG_NAME}
  LIBRARIES
    rwschema_yang_gen
    rwshell-mgmt_yang_gen
)
