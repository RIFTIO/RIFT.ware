# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Balaji Rajappa 
# Creation Date: 20/07/2016
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

##
# Install the system test launch script
##
install(
  PROGRAMS
    restconf_systest.sh
  DESTINATION usr/rift/systemtest/restconf
  COMPONENT ${INSTALL_COMPONENT}
)

##
# Install the test script that tests the system
##
install(
  PROGRAMS
    pytest/restconf_systest.py
  DESTINATION usr/rift/systemtest/restconf/pytest
  COMPONENT ${INSTALL_COMPONENT}
)

##
# Install the automation config
##
install(
  FILES
    racfg/restconf_systest.racfg
    racfg/restconf_systest_xml.racfg
  DESTINATION
    usr/rift/systemtest/restconf
    COMPONENT ${INSTALL_COMPONENT}
)

