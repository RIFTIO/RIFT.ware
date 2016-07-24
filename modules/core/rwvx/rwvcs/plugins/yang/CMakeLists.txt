
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

include(rift_yang)

rift_add_yang_target(
  TARGET rwmanifest_yang
  YANG_FILES rw-manifest.yang
  COMPONENT ${PKG_LONG_NAME}
  LIBRARIES
    rwschema_yang_gen
)

rift_add_yang_target(
  TARGET rw_vcs_yang
  YANG_FILES rw-vcs.yang rw-vm-heartbeat-log.yang
  COMPONENT ${PKG_LONG_NAME}
  LIBRARIES 
  ietf_yang_gen
    rwlog
    rwlog-mgmt_yang_gen
    rwmanifest_yang_gen
)
