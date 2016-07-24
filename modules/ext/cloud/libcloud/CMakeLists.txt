# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 2014/05/01
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

rift_externalproject_add(
  libcloud
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libcloud
  CONFIGURE_COMMAND echo
  BUILD_COMMAND python ./setup.py build

  INSTALL_COMMAND
    python ./setup.py install
      --prefix=${CMAKE_INSTALL_PREFIX}/usr
  BCACHE_COMMAND
    python ./setup.py install
      --root=${RIFT_SUBMODULE_INSTALL_PREFIX}/libcloud/${CMAKE_INSTALL_PREFIX}
)


