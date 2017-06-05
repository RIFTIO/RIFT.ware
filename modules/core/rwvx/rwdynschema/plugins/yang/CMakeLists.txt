# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Creation Date: 2014/04/08
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

##
# Yang targets
##
rift_add_yang_target(
  TARGET rwdynschema_yang
  YANG_FILES rw-dynschema-log.yang
  NO_LIB_COMPONENT
  COMPONENT ${INSTALL_COMPONENT}
  LIBRARIES
    rwschema_yang_gen
    rwyang
    rwlog
    rwlog-mgmt_yang_gen
    rwschema_yang_gen
)
