# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Balaji Rajappa 
# Creation Date: 08/25/2015
# 

cmake_minimum_required(VERSION 2.8)

##
# Add the subdirectories
##
set(subdirs
  rwtermio-c)

rift_add_subdirs(SUBDIR_LIST ${subdirs})
