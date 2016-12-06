
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

install(FILES
    include/riftware/rwtasklet.h
    include/riftware/rwtasklet_var_root.h
  DESTINATION usr/include/riftware
  COMPONENT ${INSTALL_COMPONENT})


set(GI_NAMESPACE RwTasklet)
set(GI_VERSION 1.0)

rift_uncamel(${GI_NAMESPACE} UNCAMEL_GI_NAMESPACE)

rift_add_introspection(rwtasklet_gi
  NAMESPACE ${GI_NAMESPACE}
  VERSION ${GI_VERSION}
  VAPI_PREFIX ${UNCAMEL_GI_NAMESPACE}
  INCLUDE_PATHS
    /usr/include/dispatch
    ${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/src/
    ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmsg/include/
#??    ${CMAKE_INSTALL_PREFIX}/usr/include/dispatch
  HFILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/include/riftware/rwtasklet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/riftware/rwtasklet_var_root.h
  GI_INCLUDE_HFILES rwtasklet.h rwtasklet_var_root.h
  LIBRARIES rwtypes rwvcs rwtasklet
  GIR_PATHS
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwsched/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwlog/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/plugins/yang
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwschema/yang
  PACKAGES
    RwTypes-1.0
    RwLog-1.0
    RwSched-1.0
    RwManifestYang-1.0
  LIBRARY_PATHS
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/src
    ${RIFT_SUBMODULE_BINARY_ROOT}/rwvcs/rwtasklet/src
  IDENTIFIER_FILTER_CMD "python ${CMAKE_CURRENT_SOURCE_DIR}/rwtasklet_gi_filter.py"
  SYMBOL_PREFIX "rwtasklet"
  DEPENDS
    rwvcs
    rwtasklet
    rwlog_gi
    rwsched_gi
    rwmanifest_yang
  GI_PY_OVERRIDE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/rwtasklet_gi_override.py"
  )

rift_install_vala_artifacts(
  COMPONENT ${INSTALL_COMPONENT}
  HEADER_FILES ${GI_INCLUDE_HFILES}
  GIR_FILES ${GI_NAMESPACE}-${GI_VERSION}.gir
  TYPELIB_FILES ${GI_NAMESPACE}-${GI_VERSION}.typelib
  VAPI_FILES ${UNCAMEL_GI_NAMESPACE}-${GI_VERSION}.vapi
  LUA_OVERRIDES ${GI_NAMESPACE}.lua
  PYTHON_OVERRIDES ${GI_NAMESPACE}.py
  DEST_PREFIX .
  )

add_subdirectory(src)
