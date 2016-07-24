# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Joshua Downer
# Author(s): Austin Cormier
# Creation Date: 5/12/2015
# 

cmake_minimum_required(VERSION 2.8)

install(
  PROGRAMS mission_control.py
  DESTINATION usr/rift/systemtest/mission_control
  COMPONENT ${PKG_LONG_NAME}
  )
