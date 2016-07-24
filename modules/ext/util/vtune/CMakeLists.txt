include(rift_externalproject)

set(make_cmd
  make
    -C src/
    KERNEL_VERSION=${FPATH_KERNEL_VERSION}
    KERNEL_SRC_DIR=${FPATH_KERNEL_SRC})

rift_externalproject_add(
  vtune
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vtune-3_15
  CONFIGURE_COMMAND echo
  BUILD_COMMAND ${make_cmd} all
  INSTALL_COMMAND
    ${make_cmd}
      INSTALL=${CMAKE_INSTALL_PREFIX}/usr/lib/modules/${FPATH_KERNEL_VERSION}/extra/vtunes
      install
  BCACHE_COMMAND
    ${make_cmd} 
      INSTALL=${RIFT_SUBMODULE_INSTALL_PREFIX}/vtune/${CMAKE_INSTALL_PREFIX}/usr/lib/modules/${FPATH_KERNEL_VERSION}/extra/vtunes
      install
)

