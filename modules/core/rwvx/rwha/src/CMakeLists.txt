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
# Creation Date: 6/13/2016
# 
include(rift_pkg_config)

set(PKG_NAME rwha)
set(PKG_VERSION 1.0)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

rift_pkg_check_modules(RWLIB REQUIRED rwlib>=1.0)
rift_pkg_check_modules(RW_VX REQUIRED rw_vx_plugin>=1.0)
rift_pkg_check_modules(RWTRACE REQUIRED rwtrace>=1.0)
rift_pkg_check_modules(RWYANG REQUIRED rwyang>=1.0)
rift_pkg_check_modules(RWPYTHON REQUIRED rwpython_util>=1.0)
rift_pkg_check_modules(GLIB REQUIRED glib-2.0>=2.42.0)
rift_pkg_check_modules(TALLOC REQUIRED talloc>=2.1.0)
rift_pkg_check_modules(NANOMSG REQUIRED libnanomsg>=0.1.0)

add_definitions(-std=gnu99 -D_GNU_SOURCE -D_CF_)
include_directories(BEFORE
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmsg/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwdts/include
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmemlogdts/include/riftware
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/src
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwschema/src
  /usr/include/libxml2
  /usr/include/libxml2/libxml
  ${CMAKE_INSTALL_PREFIX}/usr/include/yuma/ncx
  ${CMAKE_INSTALL_PREFIX}/usr/include/yuma/platform)

##
# Install the header files and targets
##
set(INSTALL_HFILES ../include/riftware/rwha_dts_api.h)

install(FILES ${INSTALL_HFILES}
  DESTINATION usr/include/riftware
  COMPONENT ${INSTALL_COMPONENT})

add_library(rwha_dts_api SHARED
  rwha_dts_api.c
  )
target_link_libraries(rwha_dts_api
  ${RW_VX_LIBRARIES}
  -lCoreFoundation
  ${GLIB_LIBRARIES}
  ${MSGPACK_LIBRARIES}
  ${PB_C_LIBRARIES}
  ${TALLOC_LIBRARIES}
  ${RWLIB_LIBRARIES}
  ${COREDUMPER_LIBRARIES}
  rwdts_api
  rwdts_yang_gen
  rwha_yang_gen
  rwdtsproto
  rwvcs
  rw_vcs_yang_gen)

install(TARGETS rwha_dts_api
  DESTINATION usr/${RIFT_LIBDIR}
  COMPONENT ${INSTALL_COMPONENT})
