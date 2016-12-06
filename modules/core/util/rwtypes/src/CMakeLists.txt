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
# Author(s): Anil Gunturu
# Creation Date: 02/10/2015
# 

add_definitions(-Werror)

install(FILES
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwtypes/include/riftware/rwtypes_gi.h
  DESTINATION usr/include COMPONENT ${INSTALL_COMPONENT})

rift_pkg_check_modules(GOBJECT REQUIRED gobject-2.0>=2.42.0)
include_directories(BEFORE
  ${GOBJECT_INCLUDE_DIRS})

add_library(rwtypes SHARED rwtypes_gi.c)
target_link_libraries(rwtypes PRIVATE
  ${GOBJECT_LIBRARIES})
install(TARGETS rwtypes LIBRARY
  DESTINATION usr/lib/rift/plugins COMPONENT ${INSTALL_COMPONENT})
rift_make_pc(rwtypes
  LIBS -lrwtypes
  INCLUDEDIRS ${GOBJECT_LIBRARIES}
  COMPONENT ${INSTALL_COMPONENT})

set(GI_NAMESPACE RwTypes)
set(GI_VERSION 1.0)

rift_uncamel(${GI_NAMESPACE} UNCAMEL_GI_NAMESPACE)

rift_add_introspection(rwtypes_gi
  NAMESPACE ${GI_NAMESPACE}
  VERSION ${GI_VERSION}
  VAPI_PREFIX ${UNCAMEL_GI_NAMESPACE}
  HFILES ${RIFT_SUBMODULE_SOURCE_ROOT}/rwtypes/include/riftware/rwtypes_gi.h
  GI_INCLUDE_HFILES rwtypes_gi.h
  LIBRARIES rwtypes
  LIBRARY_PATHS ${RIFT_SUBMODULE_BINARY_ROOT}/rwtypes/src
  IDENTIFIER_FILTER_CMD "python ${CMAKE_CURRENT_SOURCE_DIR}/rwtypes_gi_filter.py"
  DEPENDS rwtypes
  )

rift_install_vala_artifacts( 
  COMPONENT ${INSTALL_COMPONENT} 
  HEADER_FILES ${GI_INCLUDE_HFILES}
  GIR_FILES ${GI_NAMESPACE}-${GI_VERSION}.gir
  TYPELIB_FILES ${GI_NAMESPACE}-${GI_VERSION}.typelib
  VAPI_FILES ${UNCAMEL_GI_NAMESPACE}-${GI_VERSION}.vapi
  LUA_OVERRIDES ${GI_NAMESPACE}.lua
  DEST_PREFIX .
  )
