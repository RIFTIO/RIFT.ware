# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 10/15/2014
# 

# NOTE: These files will be used by the new MANO subsystem
set(source_yang_files
  vnfd.yang vnfr.yang rw-vnfd.yang rw-vnfr.yang
  vld.yang vlr.yang rw-vld.yang rw-vlr.yang
  nsd.yang nsr.yang rw-nsd.yang rw-nsr.yang
  pnfd.yang
  vnffgd.yang
  ietf-network.yang
  ietf-network-topology.yang
  ietf-l2-topology.yang
  rw-topology.yang
  )

rift_add_yang_target(
  TARGET mano_yang
  YANG_FILES ${source_yang_files}
  GIR_PATHS ${CMAKE_CURRENT_BINARY_DIR}
  COMPONENT ${PKG_LONG_NAME}
  LIBRARIES
    rwmanifest_yang_gen
    rwschema_yang_gen
    rwcloud_yang_gen
  )

rift_gen_yang_tree(mano-pyang-trees
  OUTFILE_PREFIX mano.yang
  YANG_FILES
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/plugins/yang/vnfd.yang
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/plugins/yang/vld.yang
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/plugins/yang/nsd.yang
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/plugins/yang/rw-vnfd.yang
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/plugins/yang/rw-vld.yang
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/plugins/yang/rw-nsd.yang
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/plugins/yang/pnfd.yang
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/plugins/yang/vnffgd.yang
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/plugins/yang/ietf-network.yang
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/plugins/yang/ietf-network-topology.yang
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/plugins/yang/ietf-l2-topology.yang
    ${RIFT_SUBMODULE_SOURCE_ROOT}/models/plugins/yang/rw-topology.yang
  )
