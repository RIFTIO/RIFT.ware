# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Manish Patel
# Creation Date: 10/28/2015
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwcm)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

set(subdirs
  plugins
  test
  )

##
# Include the subdirs
##
rift_add_subdirs(SUBDIR_LIST ${subdirs})
