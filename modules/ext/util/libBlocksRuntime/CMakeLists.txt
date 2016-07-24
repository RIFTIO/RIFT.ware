include(rift_externalproject)

rift_externalproject_add(
  libblocksruntime
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libBlocksRuntime-0.3
  CONFIGURE_COMMAND
    ./configure --prefix=${CMAKE_INSTALL_PREFIX}/usr
  BUILD_COMMAND
    make -j1
)

set(libdir ${CMAKE_INSTALL_PREFIX}/usr/lib)
externalproject_add_step(
  libblocksruntime
  link_in_install
  COMMAND
    ln -snf libBlocksRuntime.so.0.0 ${libdir}/libBlocksRuntime.so
  DEPENDEES install)

set(libdir ${RIFT_SUBMODULE_INSTALL_PREFIX}/libblocksruntime/${CMAKE_INSTALL_PREFIX}/usr/lib)
externalproject_add_step(
  libblocksruntime
  link_in_bcache
  COMMAND
    ln -snf libBlocksRuntime.so.0.0 ${libdir}/libBlocksRuntime.so
  DEPENDEES bcache)

