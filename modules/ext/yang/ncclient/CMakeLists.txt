include(rift_externalproject)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(PKG_DEBUG_FLAGS "CFLAGS=-g -O0")
endif()

rift_externalproject_add(
  ncclient
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ncclient
  CONFIGURE_COMMAND echo
  BUILD_COMMAND echo
  CONFIGURE_COMMAND echo
  INSTALL_COMMAND echo
  BCACHE_COMMAND echo
)

externalproject_add_step(
  ncclient
  build_python
  COMMAND
    python3 ./setup.py build
  WORKING_DIRECTORY <SOURCE_DIR>
  DEPENDEES build
  DEPENDERS install)

externalproject_add_step(
  ncclient
  install_python
  COMMAND
    python3 ./setup.py install --root=${CMAKE_INSTALL_PREFIX}
  WORKING_DIRECTORY <SOURCE_DIR>
  DEPENDEES install)

externalproject_add_step(
  ncclient
  bcache_python
  COMMAND
    python3 ./setup.py install
      --root=${RIFT_SUBMODULE_INSTALL_PREFIX}/ncclient/${CMAKE_INSTALL_PREFIX}
  WORKING_DIRECTORY <SOURCE_DIR>
  DEPENDEES bcache)
