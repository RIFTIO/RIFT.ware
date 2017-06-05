# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf
# Creation Date: 2014/05/07
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

include(rift_yang)

##
# Build our yang files
##
set(yang_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/rw-dts-api-log.yang
    ${CMAKE_CURRENT_SOURCE_DIR}/rw-dts-router-log.yang
    ${CMAKE_CURRENT_SOURCE_DIR}/rw-dts.yang
    ${CMAKE_CURRENT_SOURCE_DIR}/rw-dts-toy-tasklet.yang
   )

rift_add_yang_target(
  TARGET rwdts_yang
  YANG_FILES ${yang_sources}
  NO_LIB_COMPONENT
  COMPONENT ${INSTALL_COMPONENT}
  LIBRARIES
    rwschema_yang_gen
    rwmanifest_yang_gen
    rw_vcs_yang_gen
)
