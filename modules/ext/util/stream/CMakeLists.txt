include(rift_externalproject)

rift_externalproject_add(
  stream
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/stream-5.10
  CONFIGURE_COMMAND echo
  BUILD_COMMAND make rw_stream
  INSTALL_COMMAND echo
  BCACHE_COMMAND echo
)


externalproject_add_step(
  stream
  my_install
  COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/usr/local/bin/stream-5.10
  COMMAND cp -rf <BINARY_DIR>/rw_stream ${CMAKE_INSTALL_PREFIX}/usr/local/bin/stream-5.10
  COMMAND cp -rf ${CMAKE_CURRENT_SOURCE_DIR}/riftware/stream_wrapper.sh ${CMAKE_INSTALL_PREFIX}/usr/local/bin/stream-5.10
  DEPENDERS install
  DEPENDEES build
)

externalproject_add_step(
  stream
  my_bcache
  COMMAND mkdir -p ${RIFT_SUBMODULE_INSTALL_PREFIX}/stream/${CMAKE_INSTALL_PREFIX}/usr/local/bin/stream-5.10
  COMMAND cp -rf <BINARY_DIR>/rw_stream ${RIFT_SUBMODULE_INSTALL_PREFIX}/stream/${CMAKE_INSTALL_PREFIX}/usr/local/bin/stream-5.10
  COMMAND cp -rf ${CMAKE_CURRENT_SOURCE_DIR}/riftware/stream_wrapper.sh ${RIFT_SUBMODULE_INSTALL_PREFIX}/stream/${CMAKE_INSTALL_PREFIX}/usr/local/bin/stream-5.10
  DEPENDERS bcache
  DEPENDEES build
)
