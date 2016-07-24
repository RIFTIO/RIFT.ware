# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Sanil Puthiyandyil
# Creation Date: 02/29/2016
# 

cmake_minimum_required(VERSION 2.8)

set(subdirs
  multivm_vnfd
  multivm_nsd
  )

##
# Include the subdirs
##
rift_add_subdirs(SUBDIR_LIST ${subdirs})
