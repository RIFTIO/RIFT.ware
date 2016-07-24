# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# 

##
# Parse the yang files
##

include(rift_yang)

set(source_yang_files rwsdn.yang)

rift_add_yang_target(
  TARGET rwsdn_yang
  YANG_FILES ${source_yang_files}
  COMPONENT ${PKG_LONG_NAME}
  LIBRARIES
    rwschema_yang_gen
    rwyang
    rwlog
    rwlog-mgmt_yang_gen
)

