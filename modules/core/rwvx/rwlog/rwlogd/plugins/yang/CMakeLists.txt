# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf
# Creation Date: 2014/05/07
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

include(rift_yang)

##
# Build our yang files
##
set(yang_sources
  ${CMAKE_CURRENT_SOURCE_DIR}/rwlog-mgmt.yang)

rift_add_yang_target(
  TARGET rwlog-mgmt_yang
  YANG_FILES ${yang_sources}
  NO_LIB_COMPONENT
  COMPONENT ${INSTALL_COMPONENT}
  LIBRARIES
    rwschema_yang_gen
    rwshell-mgmt_yang_gen
)
