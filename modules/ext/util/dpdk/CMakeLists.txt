include(rift_externalproject)

set(PKG_LONG_NAME dpdk-2.2.0)

set(DPDK_DEBUG_FLAGS "EXTRA_CFLAGS=-g")

# add an external project target for DPDK
rift_externalproject_add(
  dpdk
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dpdk-2.2.0
  CONFIGURE_COMMAND echo
  BUILD_COMMAND
    $(MAKE) -j1 install T=x86_64-riftio-linuxapp-gcc V=1 ${DPDK_DEBUG_FLAGS}
      RTE_OUTPUT=<BINARY_DIR>/src/${PKG_LONG_NAME}-build/
      RTE_KERNELDIR=/lib/modules/${FPATH_KERNEL_VERSION}/build
  INSTALL_COMMAND echo
  BCACHE_COMMAND echo
)

set(install_path ${CMAKE_INSTALL_PREFIX}/usr)
externalproject_add_step(
  dpdk
  real_install
  COMMAND ${CMAKE_COMMAND} -E make_directory ${install_path}/lib/modules/${FPATH_KERNEL_VERSION}/extra/dpdk
  COMMAND cp -rf <BINARY_DIR>/src/${PKG_LONG_NAME}-build/lib ${install_path}
  COMMAND cp -L -r <BINARY_DIR>/src/${PKG_LONG_NAME}-build/include ${install_path}
  COMMAND mkdir -p ${install_path}/lib/modules/${FPATH_KERNEL_VERSION}/extra/dpdk
  COMMAND mkdir -p ${install_path}/lib/modules/${FPATH_KERNEL_VERSION}/extra/dpdk
  COMMAND cp -rf <BINARY_DIR>/src/${PKG_LONG_NAME}-build/kmod ${install_path}/lib/modules/${FPATH_KERNEL_VERSION}/extra/dpdk
  COMMAND mkdir -p ${install_path}/etc/dpdk/
  COMMAND cp -rf <BINARY_DIR>/tools/ ${install_path}/etc/dpdk/tools
  DEPENDERS install
  DEPENDEES build
)

set(bcache_path "${RIFT_SUBMODULE_INSTALL_PREFIX}/dpdk/${CMAKE_INSTALL_PREFIX}/usr")
externalproject_add_step(
  dpdk
  real_bcache
  COMMAND ${CMAKE_COMMAND} -E make_directory ${bcache_path}/lib/modules/${FPATH_KERNEL_VERSION}/extra/dpdk
  COMMAND cp -rf <BINARY_DIR>/src/${PKG_LONG_NAME}-build/lib ${bcache_path}
  COMMAND cp -L -r <BINARY_DIR>/src/${PKG_LONG_NAME}-build/include ${bcache_path}
  COMMAND mkdir -p ${bcache_path}/lib/modules/${FPATH_KERNEL_VERSION}/extra/dpdk
  COMMAND mkdir -p ${bcache_path}/lib/modules/${FPATH_KERNEL_VERSION}/extra/dpdk
  COMMAND cp -rf <BINARY_DIR>/src/${PKG_LONG_NAME}-build/kmod ${bcache_path}/lib/modules/${FPATH_KERNEL_VERSION}/extra/dpdk
  COMMAND mkdir -p ${bcache_path}/etc/dpdk/
  COMMAND cp -rf <BINARY_DIR>/tools/ ${bcache_path}/etc/dpdk/tools
  DEPENDERS bcache
  DEPENDEES build
)
