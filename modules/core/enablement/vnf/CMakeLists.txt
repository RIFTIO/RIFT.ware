# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Sanil Puthiyandyil
# Creation Date: 02/29/2016
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwvnf)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

set(subdirs
  image
  descriptors
  )

##
# Include the subdirs
##
rift_add_subdirs(SUBDIR_LIST ${subdirs})
