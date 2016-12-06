# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
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
  protobuf-c
  ${GOBJECT_LIBRARIES}
)

install(
  TARGETS rwmemlog
  LIBRARY
  DESTINATION usr/lib
  COMPONENT ${INSTALL_COMPONENT}
)

rift_make_pc(rwmemlog
  LIBS -lrwmemlog
  COMPONENT ${INSTALL_COMPONENT}
)

install(FILES
  ../include/riftware/rwmemlog.h
  ../include/riftware/rwmemlog_mgmt.h
  DESTINATION usr/include
  COMPONENT ${INSTALL_COMPONENT}
)

