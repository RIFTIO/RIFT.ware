# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Austin Cormier
# Creation Date: 01/04/2015
# 

cmake_minimum_required(VERSION 2.8)

##
# Add the subdirectories
##
set(subdirs
  yang)

rift_add_subdirs(SUBDIR_LIST ${subdirs})
