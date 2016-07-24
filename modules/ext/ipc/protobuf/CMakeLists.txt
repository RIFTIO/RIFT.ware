include(rift_externalproject)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(PKG_DEBUG_FLAGS "CFLAGS=-g -O0")
endif()

rift_externalproject_add(
  protobuf
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/protobuf-${PROTOBUF_VERSION}
  CONFIGURE_COMMAND
    ./configure
      --prefix=${CMAKE_INSTALL_PREFIX}/usr
      ${PKG_DEBUG_FLAGS}
      PKG_CONFIG_PATH=${RIFT_PKG_CONFIG_PATH}
)

externalproject_add_step(
  protobuf
  autoreconf
  COMMAND
    autoreconf --install --force --verbose
  WORKING_DIRECTORY <SOURCE_DIR>
  DEPENDEES rsync_to_build
  DEPENDERS configure)

externalproject_add_step(
  protobuf
  build_python
  COMMAND
    python setup.py build
  WORKING_DIRECTORY <SOURCE_DIR>/python
  DEPENDEES build
  DEPENDERS install)

externalproject_add_step(
  protobuf
  install_python
  COMMAND
    python ./setup.py install --root=${CMAKE_INSTALL_PREFIX}
  WORKING_DIRECTORY <SOURCE_DIR>/python
  DEPENDEES install)

externalproject_add_step(
  protobuf
  bcache_python
  COMMAND
    python ./setup.py install
      --root=${RIFT_SUBMODULE_INSTALL_PREFIX}/protobuf/${CMAKE_INSTALL_PREFIX}
  WORKING_DIRECTORY <SOURCE_DIR>/python
  DEPENDEES bcache)

externalproject_add_step(
  protobuf
  copy_python
  COMMAND
    rsync -av --delete-after <SOURCE_DIR>/python/ <SOURCE_DIR>/python3/
  DEPENDEES install)

externalproject_add_step(
  protobuf
  2to3
  COMMAND
    2to3 --write --no-diffs <SOURCE_DIR>/python3
  DEPENDEES copy_python)

externalproject_add_step(
  protobuf
  install_python3
  COMMAND
    python3 ./setup.py install --root=${CMAKE_INSTALL_PREFIX}
  WORKING_DIRECTORY <SOURCE_DIR>/python3
  DEPENDEES 2to3)

externalproject_add_step(
  protobuf
  bcache_python3
  COMMAND
    python3 ./setup.py install
      --root=${RIFT_SUBMODULE_INSTALL_PREFIX}/protobuf/${CMAKE_INSTALL_PREFIX}
  WORKING_DIRECTORY <SOURCE_DIR>/python3
  DEPENDEES install_python3)

