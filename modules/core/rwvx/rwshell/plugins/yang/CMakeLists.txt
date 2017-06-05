# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Rajesh Ramankuyyu
# Creation Date: 2015/05/20
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

include(rift_yang)

##
# Build our yang files
##
set(yang_sources
  ${CMAKE_CURRENT_SOURCE_DIR}/rwshell-mgmt.yang)

rift_add_yang_target(
  TARGET rwshell-mgmt_yang
  YANG_FILES ${yang_sources}
  NO_LIB_COMPONENT
  COMPONENT ${INSTALL_COMPONENT}
  LIBRARIES
    rwschema_yang_gen
)
