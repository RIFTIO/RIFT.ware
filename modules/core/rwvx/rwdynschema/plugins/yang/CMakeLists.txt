# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Creation Date: 2014/04/08
# 

##
# Yang targets
##
rift_add_yang_target(
  TARGET rwdynschema_yang
  YANG_FILES rw-dynschema-log.yang
  COMPONENT ${PKG_LONG_NAME}
  LIBRARIES
    rwschema_yang_gen
    rwyang
    rwlog
    rwlog-mgmt_yang_gen
    rwschema_yang_gen
)
