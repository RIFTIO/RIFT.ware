# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 2014/04/01
# 

cmake_minimum_required(VERSION 2.8)

set(subdirs proto rwdtsrouter-c yang rwdtstasklet dtstestpub)
rift_add_subdirs(SUBDIR_LIST ${subdirs})
