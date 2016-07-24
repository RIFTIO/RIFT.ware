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
    ${CMAKE_CURRENT_SOURCE_DIR}/rwmsg-data.yang
    ${CMAKE_CURRENT_SOURCE_DIR}/rw-msg-log.yang
   )

rift_add_yang_target(
  TARGET rwmsg_yang
  YANG_FILES ${yang_sources}
  COMPONENT ${PKG_LONG_NAME}
  LIBRARIES
    rwschema_yang_gen
)
