# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 03/26/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

##
# DEPENDENCY ALERT
# The submodule dependencies must be specified in the
# .gitmodules.dep file at the top level (supermodule) directory
# If this submodule depends other submodules remember to update
# the .gitmodules.dep
##

cmake_minimum_required(VERSION 2.8)

##
# Submodule specific includes will go here, 
# These are specified here, since these variables are accessed
# from multiple sub directories. If the variable is subdirectory
# specific it must be declared in the subdirectory.
#sic#

set(PKG_NAME confd-basic)
set(PKG_VERSION 6.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

##
# This macro adds targets for documentaion, unittests, code coverage and packaging
##

set(CONFD_DIR ${CMAKE_CURRENT_BINARY_DIR}/${PKG_NAME}-${PKG_VERSION})

add_custom_target(extract_confd_installer ALL
  COMMAND rm -rf ${CONFD_DIR}
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/${PKG_LONG_NAME}.linux.x86_64.installer.bin ${CONFD_DIR}
  # Make the unix domain socket driver
  COMMAND make -C ${CONFD_DIR}/src/confd/ipc_drv
  # Find and remove all symlinks
  COMMAND find ${CONFD_DIR} -type l -exec rm -f {} "\;"
  # Remove the built-in libconfd
  COMMAND rm ${CONFD_DIR}/lib/libconfd.so
  # Remove the build-in crypto library
  COMMAND rm ${CONFD_DIR}/lib/confd/lib/core/crypto/priv/lib/crypto.so
  COMMENT "Running confd installer"
  )

##
# Confd install
##
install(DIRECTORY ${CONFD_DIR}/bin ${CONFD_DIR}/lib ${CONFD_DIR}/src ${CONFD_DIR}/etc ${CONFD_DIR}/var
  FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  DESTINATION usr/local/confd
  COMPONENT ${PKG_LONG_NAME}
  )

install(DIRECTORY ${CONFD_DIR}/include
  FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  DESTINATION usr
  COMPONENT ${PKG_LONG_NAME}
  )

