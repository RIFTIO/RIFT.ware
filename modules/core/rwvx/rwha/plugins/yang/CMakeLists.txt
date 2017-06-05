# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Creation Date: 2016/06/22
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

include(rift_yang)

##
# Parse the yang files
##
set(ha_yang_files rw-ha.yang)

rift_add_yang_target(
  TARGET
    rwha_yang
  YANG_FILES
    ${ha_yang_files}
  LIBRARIES
    rwschema_yang_gen
    rw_vcs_yang_gen
  NO_LIB_COMPONENT
  COMPONENT
    ${INSTALL_COMPONENT}
  )
