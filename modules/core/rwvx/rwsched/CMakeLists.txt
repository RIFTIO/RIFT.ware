# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tim Mortsolf
# Creation Date: 12/2/2013
# 

cmake_minimum_required(VERSION 2.8)

##
# Add the subdirectories
##
set(subdirs src test plugins)
rift_add_subdirs(SUBDIR_LIST ${subdirs})
