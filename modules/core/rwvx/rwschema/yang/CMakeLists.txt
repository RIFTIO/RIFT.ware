# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tim Mortsolf
# Creation Date: 2014/03/13
# 

include(rift_yang)

##
# Set various yang files
##
set(rwschema_yang_files rw-log.yang rw-base.yang rw-debug.yang rw-mgmt-schema.yang
  mano-base.yang rw-vnf-base-config.yang rw-vnf-base-opdata.yang
  toy-vnf-config.yang toy-vnf-opdata.yang mano-types.yang rwvcs-types.yang
  rw-vnf-base-types.yang
  )

##
# function to add targets for .yang->.xsd->.proto
##
rift_add_yang_target(
  TARGET rwschema_yang
  YANG_FILES ${rwschema_yang_files}
  COMPONENT ${PKG_LONG_NAME}
)

##
# Generate various yang trees for visualization
##
rift_gen_yang_tree(rw-base-pyang-trees
  OUTFILE_PREFIX rw-base.yang
  YANG_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/mano-types.yang
    ${CMAKE_CURRENT_SOURCE_DIR}/rw-base.yang
    ${CMAKE_CURRENT_SOURCE_DIR}/rw-debug.yang
    ${CMAKE_CURRENT_SOURCE_DIR}/rw-log.yang
    ${CMAKE_CURRENT_SOURCE_DIR}/rw-mgmt-schema.yang
)
