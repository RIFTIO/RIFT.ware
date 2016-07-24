# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Vijay Nag
# Creation Date: 09/20/2014
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME redis)
set(PG_RELEASE 1)

add_subdirectory(redis-cluster-3.0)

install(FILES redis.conf DESTINATION usr/bin/ COMPONENT ${PKG_NAME})
install(DIRECTORY redis-gems DESTINATION usr/bin/ COMPONENT ${PKG_NAME})
install(DIRECTORY lua-scripts DESTINATION usr/bin/ COMPONENT ${PKG_NAME})
install(PROGRAMS redis_cluster.py DESTINATION usr/bin COMPONENT ${PKG_NAME})

