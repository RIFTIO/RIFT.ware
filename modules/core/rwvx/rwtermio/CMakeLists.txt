# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Balaji Rajappa 
# Creation Date: 8/25/2015
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwtermio)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

##
# Add the subdirectories. TODO add test
##
set(subdirs 
  plugins
  src
)

rift_add_subdirs(SUBDIR_LIST ${subdirs})
