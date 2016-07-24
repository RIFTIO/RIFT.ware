# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Joshua Downer
# Author(s): Austin Cormier
# Creation Date: 5/12/2015
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME common)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

set(subdirs
  plugins
  python
  )

##
# Include the subdirs
##
rift_add_subdirs(SUBDIR_LIST ${subdirs})

install(
  FILES
    rw_gen_package.py
  DESTINATION usr/rift/mano/common
  COMPONENT ${PKG_LONG_NAME})
