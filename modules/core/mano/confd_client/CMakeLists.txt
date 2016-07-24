# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 2014/04/30
# 

cmake_minimum_required(VERSION 2.8)

# confd_client executable
add_executable(confd_client confd_client.c)

target_link_libraries(confd_client
  ${CMAKE_INSTALL_PREFIX}/usr/local/confd/lib/libconfd.so
  pthread
  )
