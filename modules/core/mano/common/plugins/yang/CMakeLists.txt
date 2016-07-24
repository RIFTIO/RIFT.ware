# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Austin Cormier
# Creation Date: 2015/11/20
# 

##
# Yang targets
##
rift_add_yang_target(
  TARGET rwcloud_yang 
  YANG_FILES rw-cloud.yang rw-sdn.yang
  COMPONENT ${PKG_LONG_NAME}
  LIBRARIES
    rwsdn_yang_gen 
)

rift_add_yang_target(
  TARGET rwconfig_agent_yang
  YANG_FILES rw-config-agent.yang
  COMPONENT ${PKG_LONG_NAME}
  LIBRARIES
    mano_yang_gen
  DEPENDS
    mano_yang
)
