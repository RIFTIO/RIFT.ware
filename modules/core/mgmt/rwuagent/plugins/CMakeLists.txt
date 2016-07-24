# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tom Seidenberg
# Creation Date: 2014/04/03
# 

cmake_minimum_required(VERSION 2.8)

set(subdirs proto yang rwuagent-c)
rift_add_subdirs(SUBDIR_LIST ${subdirs})
