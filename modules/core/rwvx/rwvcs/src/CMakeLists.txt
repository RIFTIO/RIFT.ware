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
# Author(s): Tim Mortsolf
# Creation Date: 3/13/2013
# 
include(rift_pkg_config)

set(PKG_NAME rwvcs)
set(PKG_VERSION 1.0)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

rift_pkg_check_modules(RWLIB REQUIRED rwlib>=1.0)
rift_pkg_check_modules(RW_VX REQUIRED rw_vx_plugin>=1.0)
rift_pkg_check_modules(RWTRACE REQUIRED rwtrace>=1.0)
rift_pkg_check_modules(RWYANG REQUIRED rwyang>=1.0)
rift_pkg_check_modules(RWPYTHON REQUIRED rwpython_util>=1.0)

rift_pkg_check_modules(GLIB REQUIRED glib-2.0>=2.42.0)
#rift_pkg_check_modules(CFLITE REQUIRED CFLite>=0.1.2)
rift_pkg_check_modules(TALLOC REQUIRED talloc>=2.1.0)
rift_pkg_check_modules(NANOMSG REQUIRED libnanomsg>=0.1.0)

add_definitions(-std=gnu99 -D_GNU_SOURCE -D_CF_)
include_directories(
  ${RWLIB_INCLUDE_DIRS}
  ${RW_VX_INCLUDE_DIRS}
  ${RWTRACE_INCLUDE_DIRS}
  ${RWYANG_INCLUDE_DIRS}
  ${RWPYTHON_INCLUDE_DIRS}
  ${GLIB_INCLUDE_DIRS}
  ${TALLOC_INCLUDE_DIRS}
  ${NANOMSG_INCLUDE_DIRS})

add_library(rwvcs SHARED
  rwvcs_component.c
  rwvcs_instance.c
  rwvcs_manifest.c
  rwvcs_variable.c
  rwvcs_zk.c
  rwvcs_rwzk.c)

target_link_libraries(rwvcs PRIVATE
  # submodule deps
  rwvcs_zk-1.0
  rwlog
  rw_vcs_yang_gen
  rwmanifest_yang_gen
  rwmsg
  rwsched
  rwnetns
  dispatch
  peas-1.0

  # external deps
  ${RWLIB_LIBRARIES}
  ${RW_VX_LIBRARIES}
  ${RWTRACE_LIBRARIES}
  ${RWYANG_LIBRARIES}
  ${RWPYTHON_LIBRARIES}
  ${GLIB_LIBRARIES}
  CoreFoundation
  ${TALLOC_LIBRARIES}
  ${NANOMSG_LIBRARIES}
  rt)

add_executable(rwssh_wrapper rwssh_wrapper.c)

##
# Install the header files and targets
##
set(INSTALL_HFILES
  ../include/riftware/rwcal_vars_api.h
  ../include/riftware/rwvx.h
  ../include/riftware/rwvcs.h
  ../include/riftware/rwvcs_defs.h
  ../include/riftware/rwvcs_component.h
  ../include/riftware/rwvcs_manifest.h
  ../include/riftware/rwvcs_rwzk.h
  ../include/riftware/rwvcs_zk_api.h)

install(FILES ${INSTALL_HFILES}
  DESTINATION usr/include/riftware
  COMPONENT ${INSTALL_COMPONENT})

install(TARGETS rwvcs
  LIBRARY
  DESTINATION usr/lib
  COMPONENT ${INSTALL_COMPONENT})

install(TARGETS rwssh_wrapper
  DESTINATION usr/bin
  COMPONENT ${INSTALL_COMPONENT})

