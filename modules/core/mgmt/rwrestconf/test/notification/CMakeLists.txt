# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Varun Prasad
# Creation Date: 09/03/2016
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

install(
  PROGRAMS
    notification_test
  DESTINATION usr/rift/systemtest/notification_test
  COMPONENT ${INSTALL_COMPONENT})

install(
  FILES
    pytest/conftest.py
    pytest/notification_test.py
  DESTINATION usr/rift/systemtest/notification_test/pytest
  COMPONENT ${INSTALL_COMPONENT})

install(
  FILES
    racfg/notification_test.racfg
  DESTINATION
    usr/rift/systemtest/notification_test
    COMPONENT ${INSTALL_COMPONENT})

rift_python_install_tree(
  FILES
    rift/test/restconf_system.py
  PYTHON3_ONLY
  COMPONENT ${INSTALL_COMPONENT})
