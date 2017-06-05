# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf
# Creation Date: 2014/03/18
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)
include(rift_python)

set(PKG_NAME rwcli)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

set(subdirs src proto plugins test vala scripts)
rift_add_subdirs(SUBDIR_LIST ${subdirs})

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/test/cli_test.xml
  ${CMAKE_CURRENT_BINARY_DIR}/cli_test.xml
  ESCAPE_QUOTES @ONLY
)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/test/yang/rwcli_test.yang
  ${CMAKE_CURRENT_BINARY_DIR}/rwcli_test.yang
  ESCAPE_QUOTES @ONLY
)

##
# documentation target
##
rift_add_doxygen_target(rwcli_doxygen DEST_DIR rwcli)

rift_python_install_tree(
  FILES
    rift/pprint.py
    rift/rwcli_writer.py
  COMPONENT ${INSTALL_COMPONENT})
