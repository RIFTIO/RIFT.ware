
# STANDARD_RIFT_IO_COPYRIGHT

include(rift_yang)

rift_add_yang_target(
  TARGET rwmanifest_yang
  YANG_FILES rw-manifest.yang
  NO_LIB_COMPONENT
  COMPONENT ${INSTALL_COMPONENT}
  LIBRARIES
    rwschema_yang_gen
)

rift_add_yang_target(
  TARGET rw_vcs_yang
  YANG_FILES rw-vcs.yang rw-vm-heartbeat-log.yang
  NO_LIB_COMPONENT
  COMPONENT ${INSTALL_COMPONENT}
  LIBRARIES 
  ietf_yang_gen
    rwlog
    rwlog-mgmt_yang_gen
    rwmanifest_yang_gen
)
