# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Joshua Downer
# Creation Date: 2015/10/27
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwmon)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

set(subdirs plugins test)
rift_add_subdirs(SUBDIR_LIST ${subdirs})
