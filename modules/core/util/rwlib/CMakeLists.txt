# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwlib)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

set(subdirs src test plugins python)
rift_add_subdirs(SUBDIR_LIST ${subdirs})

##
# documentation target
##
rift_add_doxygen_target(${PKG_NAME}_doxygen
  DOXY_NAME "RW.Library API Reference"
  DEST_DIR ${PKG_NAME})

