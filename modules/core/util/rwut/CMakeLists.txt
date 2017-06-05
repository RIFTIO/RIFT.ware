# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Andrew Golovanov
# Creation Date: 4/3/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwut)

# Install header files of the unit test framework
file(GLOB RWUT_HFILES include/riftware/*)
install(
  FILES ${RWUT_HFILES} 
  DESTINATION usr/include/rwut 
  COMPONENT ${INSTALL_COMPONENT}
)

##
# documentation target
##
set(doxy_files
  include/riftware/rwut.h
)

rift_add_doxygen_target(${PKG_NAME}_doxygen
  DOXY_NAME "RW Unit Test Framework"
  DOXY_FILES ${doxy_files}
  DEST_DIR ${PKG_NAME})


add_subdirectory(test)
