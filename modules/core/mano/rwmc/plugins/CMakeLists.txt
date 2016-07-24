# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Joshua Downer
# Author(s): Austin Cormier
# Creation Date: 5/12/2015
# 

cmake_minimum_required(VERSION 2.8)

set(subdirs
  rwmctasklet
  yang
  )

##
# Include the subdirs
##
rift_add_subdirs(SUBDIR_LIST ${subdirs})
