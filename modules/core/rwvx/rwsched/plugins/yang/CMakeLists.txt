# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Prashanth Bhaskar
# Creation Date: 2015/10/15
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

include(rift_yang)

set(PKG_NAME rwsched)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

##
# Build our yang files
##
set(yang_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/rw-sched-log.yang
   )

rift_add_yang_target(
  TARGET rwsched_log_yang
  YANG_FILES ${yang_sources}
  NO_LIB_COMPONENT
  COMPONENT ${INSTALL_COMPONENT}
  LIBRARIES
    rwschema_yang_gen
)
