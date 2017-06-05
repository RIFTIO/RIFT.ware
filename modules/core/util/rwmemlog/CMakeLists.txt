# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Creation Date: 2015/09/10
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwmemlog)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")


set(subdirs plugins src test)
rift_add_subdirs(SUBDIR_LIST
  plugins
  src
  test
)

rift_add_doxygen_target(
  ${PKG_NAME}_doxygen 
  DEST_DIR ${PKG_NAME}
)

