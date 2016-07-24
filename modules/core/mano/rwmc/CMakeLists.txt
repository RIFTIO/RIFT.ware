# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Joshua Downer
# Author(s): Austin Cormier
# Creation Date: 5/12/2015
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwmc)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

set(subdirs
  plugins
  ra
  test
  )

##
# Include the subdirs
##
rift_add_subdirs(SUBDIR_LIST ${subdirs})

install(
  PROGRAMS
    bin/cloudsim_http_proxy.sh
  DESTINATION usr/bin
  COMPONENT ${PKG_LONG_NAME}
)
