# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Creation Date: 2015/09/11
# 

cmake_minimum_required(VERSION 2.8)

rift_pkg_check_modules(GOBJECT REQUIRED gobject-2.0>=2.42.0)
include_directories(BEFORE
  ${GOBJECT_INCLUDE_DIRS}
)

set(rwmemlog_base_src
    rwmemlog_base.cpp
)

add_library(rwmemlogobjlib OBJECT ${rwmemlog_base_src})

set(rwmemlog_sources
  rwmemlog_mgmt.cpp
  rwmemlog_output.cpp
)

add_library(rwmemlog SHARED ${rwmemlog_sources})
target_link_libraries(rwmemlog PRIVATE
  rwlib
  rwyang
  rwmemlog_yang_gen
  ${CMAKE_INSTALL_PREFIX}/usr/lib/libprotobuf-c.so
  ${GOBJECT_LIBRARIES}
)

install(
  TARGETS rwmemlog
  LIBRARY
  DESTINATION usr/lib
  COMPONENT ${PKG_LONG_NAME}
)

rift_make_pc(rwmemlog
  LIBS -lrwmemlog
  COMPONENT ${PKG_LONG_NAME}
)

install(FILES
  ../include/riftware/rwmemlog.h
  ../include/riftware/rwmemlog_mgmt.h
  DESTINATION usr/include
  COMPONENT ${PKG_LONG_NAME}
)

