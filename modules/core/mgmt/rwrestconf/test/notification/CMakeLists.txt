# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Varun Prasad
# Creation Date: 09/03/2016
# 

cmake_minimum_required(VERSION 2.8)

install(
  PROGRAMS
    notification_test
  DESTINATION usr/rift/systemtest/notification_test
  COMPONENT "rwmgmt-test")

install(
  FILES
    pytest/conftest.py
    pytest/notification_test.py
  DESTINATION usr/rift/systemtest/notification_test/pytest
  COMPONENT "rwmgmt-test")

install(
  FILES
    racfg/notification_test.racfg
  DESTINATION
    usr/rift/systemtest/notification_test
    COMPONENT "rwmgmt-test")

rift_python_install_tree(
  FILES
    rift/test/__init__.py
    rift/test/restconf_system.py
  PYTHON3_ONLY
  COMPONENT "rwmgmt-test")
