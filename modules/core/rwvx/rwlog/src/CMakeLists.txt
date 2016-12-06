
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

include(rift_python)

##
# Add a shared libarary target
##
set(C_SOURCES rwlog.c 
  rwlog_filters.c rwlog_event_buf.c)
add_library(rwlog SHARED ${C_SOURCES} ${auto_c_files})
target_link_libraries(rwlog PRIVATE
  rwlib
  rwschema_yang_gen
  rwgeneric_yang_gen
  glib-2.0
  gobject-2.0
  protobuf-c
  uuid
  rt
  ssl
  pthread
  crypto)
add_dependencies(rwlog
  rwgeneric_yang.headers
  rwschema_yang.headers
  rwlog_event_auto
  process_rwlog_evt_files)

##
# Install the header files and targets
##
set(INSTALL_HFILES
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwlog/include/riftware/rwlog.h
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwlog/include/riftware/rwlog_gi.h
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwlog/include/riftware/rwlog_filters.h
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwlog/include/riftware/rwlog_common_macros.h
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwlog/include/riftware/rwlog_category_list.h
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwlog/include/riftware/rwlog_event_buf.h)

rift_python_install_tree(
  FILES rwlogger.py
  COMPONENT ${INSTALL_COMPONENT})

install(FILES ${INSTALL_HFILES} DESTINATION usr/include COMPONENT ${INSTALL_COMPONENT})
install(FILES ${auto_h_files} DESTINATION usr/include/rwlog COMPONENT ${INSTALL_COMPONENT})
install(TARGETS rwlog LIBRARY DESTINATION usr/lib COMPONENT ${INSTALL_COMPONENT})

##
# documentation target
##
rift_add_doxygen_target(${PKG_NAME}_doxygen
  DOXY_NAME "RW.Log QuickStart and API Reference"
  DOXY_FILES 
  ../include/riftware/rwlog.h 
  ../rwlogd/include/riftware/rwlogd_plugin_mgr.h
  ../include/riftware/rwlog_gi.h
  ../include/riftware/rwlog_filters.h ../include/riftware/rwlog_event_buf.h
  ../include/riftware/rwlog_category_list.h rwlog.c
  DEST_DIR ${PKG_NAME})

##
# Add the cmake magic to generate the GI bindings
# Note that the g-ir-scanner scans the annotations
# in HFILES and generates the gir and typelib files
##
set(GI_NAME rwlog)
set(GI_VERSION 1.0)
set(GI_NAMESPACE RwLog)
rift_uncamel(${GI_NAMESPACE} UNCAMEL_GI_NAMESPACE)

rift_add_introspection(${GI_NAME}_gi
  NAMESPACE ${GI_NAMESPACE}
  VERSION ${GI_VERSION}
  HFILES ${RIFT_SUBMODULE_SOURCE_ROOT}/rwlog/include/riftware/rwlog_gi.h
  LIBRARIES rwlog rwtypes protobuf-c
  PACKAGES ProtobufC-1.0
  SYMBOL_PREFIX "rwlog"
  IDENTIFIER_FILTER_CMD "python ${CMAKE_CURRENT_SOURCE_DIR}/rwlog_gi_filter.py"
  GI_INCLUDE_HFILES rwlog_gi.h
  DEPENDS rwlog
  VAPI_PREFIX ${UNCAMEL_GI_NAMESPACE}
  )

##
# Install the generated gir and typelib files
##
rift_install_vala_artifacts(
  COMPONENT ${INSTALL_COMPONENT}
  GIR_FILES ${GI_NAMESPACE}-${GI_VERSION}.gir
  VAPI_FILES ${UNCAMEL_GI_NAMESPACE}-${GI_VERSION}.vapi
  TYPELIB_FILES ${GI_NAMESPACE}-${GI_VERSION}.typelib
  LUA_OVERRIDES ${GI_NAMESPACE}.lua
  DEST_PREFIX .
  )
