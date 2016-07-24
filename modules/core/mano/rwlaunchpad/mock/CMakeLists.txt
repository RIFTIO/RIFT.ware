
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

cmake_minimum_required(VERSION 2.0)

include(rift_plugin)

set(subdirs
    plugins
    )

rift_add_subdirs(SUBDIR_LIST ${subdirs})

