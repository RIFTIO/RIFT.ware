# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tom Seidenberg
# Creation Date: 2014/04/03
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

##
# Yang targets
##
rift_add_yang_target(
  TARGET uagent_ut_test_yang
  YANG_FILES ut-types.yang interfaces.yang routes.yang dns.yang
  notif.yang vehicle-b.yang vehicle-augment-b.yang rw-agent-testbed.yang ut-composite.yang
  NO_LIB_COMPONENT
  COMPONENT
    ${INSTALL_COMPONENT}
  DEPENDS
   rwuagent_yang
  LIBRARIES
   rwuagent_yang_gen
)
