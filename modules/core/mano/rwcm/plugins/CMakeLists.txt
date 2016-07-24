# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Manish Patel
# Creation Date: 10/29/2015
# 

cmake_minimum_required(VERSION 2.8)

set(subdirs
  yang
  rwconman
  )

##
# Include the subdirs
##
rift_add_subdirs(SUBDIR_LIST ${subdirs})
