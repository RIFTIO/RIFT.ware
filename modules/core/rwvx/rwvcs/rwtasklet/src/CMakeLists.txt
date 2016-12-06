
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

include(rift_pkg_config)

rift_pkg_check_modules(RWLIB REQUIRED rwlib>=1.0)

rift_pkg_check_modules(GOBJECT REQUIRED gobject-2.0>=2.42.0)
#rift_pkg_check_modules(CFLITE REQUIRED CFLite>=0.1.2)
#rift_pkg_check_modules(MSGPACK REQUIRED msgpack>=0.5.9)
rift_pkg_check_modules(PB_C REQUIRED libprotobuf-c>=1.1.0)

add_definitions(-std=gnu99 -D_GNU_SOURCE)
include_directories(
  ${RWLIB_INCLUDE_DIRS}
  ${GOBJECT_INCLUDE_DIRS}
  ${PB_C_INCLUDE_DIRS})

add_library(rwtasklet SHARED
  rwtasklet.c
  rwtasklet_var_root.c)

target_link_libraries(rwtasklet
  rwlog
  rwmsg
  rwsched
  rwvcs
  rwyang
  rwmanifest_yang_gen
  rw_vcs_yang_gen

  ${RWLIB_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  CoreFoundation
  ${PB_C_LIBRARIES})

install(TARGETS rwtasklet
  LIBRARY
  DESTINATION usr/lib
  COMPONENT ${INSTALL_COMPONENT})

rift_make_pc(rwtasklet
  VERSION 1.0
  REQUIRES_PRIVATE rwvcs
  COMPONENT ${INSTALL_COMPONENT})
