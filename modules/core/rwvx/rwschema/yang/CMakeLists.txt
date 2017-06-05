# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf
# Creation Date: 2014/03/13
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

include(rift_yang)

##
# Set various yang files
##
set(rwschema_yang_files
  mano-base.yang
  rw-log.yang rw-base.yang rw-debug.yang rw-mgmt-schema.yang
  rw-vnf-base-config.yang rw-vnf-base-opdata.yang
  toy-vnf-config.yang toy-vnf-opdata.yang rwvcs-types.yang
  rw-vnf-base-types.yang
  )

##
# function to add targets for .yang->.xsd->.proto
##
rift_add_yang_target(
  TARGET rwschema_yang
  YANG_FILES ${rwschema_yang_files}
  NO_LIB_COMPONENT
  COMPONENT ${INSTALL_COMPONENT}
)

##
# Generate various yang trees for visualization
##
rift_gen_yang_tree(rw-base-pyang-trees
  OUTFILE_PREFIX rw-base.yang
  YANG_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/mano-base.yang
    ${CMAKE_CURRENT_SOURCE_DIR}/rw-base.yang
    ${CMAKE_CURRENT_SOURCE_DIR}/rw-debug.yang
    ${CMAKE_CURRENT_SOURCE_DIR}/rw-log.yang
    ${CMAKE_CURRENT_SOURCE_DIR}/rw-mgmt-schema.yang
)
