include(rift_externalproject)

# add an external project target gtest
rift_externalproject_add(
  gmock
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/gmock-1.7.0
  INSTALL_COMMAND echo
  BCACHE_COMMAND echo
)

set(bcache_dest
  ${RIFT_SUBMODULE_INSTALL_PREFIX}/gmock/${CMAKE_INSTALL_PREFIX}/usr)

externalproject_add_step(
  gmock
  extra_bcache

  COMMAND
    mkdir -p ${bcache_dest}/include/rwut

  # copy gmock header files
  COMMAND
    cp -r
      <BINARY_DIR>/include/gmock
      ${bcache_dest}/include/rwut

  # copy gtest header files
  COMMAND
    cp -r
      <BINARY_DIR>/gtest/include/gtest
      ${bcache_dest}/include/rwut

  COMMAND
    mkdir -p ${bcache_dest}/lib/rwut

  # copy gtest libraries
  COMMAND
    cp
      <BINARY_DIR>/gtest/libgtest.a
      <BINARY_DIR>/gtest/libgtest_main.a
      ${bcache_dest}/lib/rwut

  # copy gmock libraries
  COMMAND
    cp
      <BINARY_DIR>/libgmock.a
      <BINARY_DIR>/libgmock_main.a
      ${bcache_dest}/lib/rwut

  DEPENDEES bcache)

externalproject_add_step(
  gmock
  extra_install

  COMMAND
    mkdir -p ${CMAKE_INSTALL_PREFIX}/usr/include/rwut

  # copy gmock header files
  COMMAND
    cp -r
      <BINARY_DIR>/include/gmock
      ${CMAKE_INSTALL_PREFIX}/usr/include/rwut

  # copy gtest header files
  COMMAND
    cp -r
      <BINARY_DIR>/gtest/include/gtest
      ${CMAKE_INSTALL_PREFIX}/usr/include/rwut

  COMMAND
    mkdir -p ${CMAKE_INSTALL_PREFIX}/usr/lib/rwut

  # copy gtest libraries
  COMMAND
    cp
      <BINARY_DIR>/gtest/libgtest.a
      <BINARY_DIR>/gtest/libgtest_main.a
      ${CMAKE_INSTALL_PREFIX}/usr/lib/rwut

  # copy gmock libraries
  COMMAND
    cp
      <BINARY_DIR>/libgmock.a
      <BINARY_DIR>/libgmock_main.a
      ${CMAKE_INSTALL_PREFIX}/usr/lib/rwut

  DEPENDEES install)

