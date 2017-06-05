# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf
# Creation Date: 2014/03/20
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

set(subdirs ${PKG_NAME}-c rwcli_plugin-python rwcli-zsh)
rift_add_subdirs(SUBDIR_LIST ${subdirs})

#
# Install the script files.
#
install(
FILES
  rw_ssd.zsh
  rw_cli_login_shell.sh
DESTINATION
  usr/rift/cli/scripts
COMPONENT
  ${INSTALL_COMPONENT}
)

#
# Install the base riftware CLI Manifest
#
install(
FILES
  rw.cli.xml
DESTINATION
  usr/data/yang
COMPONENT
  ${INSTALL_COMPONENT}
)
