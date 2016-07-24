# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 07/23/2014
# 

set(GI_NAME example_gi)
set(GI_VERSION 1.0)
set(GI_NAMESPACE ExampleGi)

rift_pkg_check_modules(PEAS REQUIRED libpeas-1.0>=1.12.2)
include_directories(BEFORE
  ${PEAS_INCLUDE_DIRS})

add_library(${GI_NAME}
  SHARED example_gi_boxed.c example_gi_enum.c example_gi_gobject.c)
target_link_libraries(${GI_NAME} PRIVATE
  ${PEAS_LIBRARIES})

install(TARGETS ${GI_NAME} LIBRARY DESTINATION usr/lib
  COMPONENT ${PKG_LONG_NAME})

rift_add_introspection(${GI_NAME}_
  NAMESPACE ${GI_NAMESPACE}
  VERSION ${GI_VERSION}
  VAPI_PREFIX ${GI_NAME}
  GI_INCLUDE_HFILES ${GI_NAME}.h
  CFILES 
  ${CMAKE_CURRENT_SOURCE_DIR}/example_gi_boxed.c 
  ${CMAKE_CURRENT_SOURCE_DIR}/example_gi_enum.c 
  ${CMAKE_CURRENT_SOURCE_DIR}/example_gi_gobject.c
  HFILES 
  ${CMAKE_CURRENT_SOURCE_DIR}/example_gi_boxed.h 
  ${CMAKE_CURRENT_SOURCE_DIR}/example_gi_enum.h 
  ${CMAKE_CURRENT_SOURCE_DIR}/example_gi_gobject.h
  LIBRARIES ${GI_NAME}
  DEPENDS ${GI_NAME}
  GENERATE_GI_OVERRIDE
  )

rift_install_vala_artifacts(
  COMPONENT ${PKG_LONG_NAME}
  GIR_FILES ${GI_NAMESPACE}-${GI_VERSION}.gir
  TYPELIB_FILES ${GI_NAMESPACE}-${GI_VERSION}.typelib
  VAPI_FILES ${GI_NAME}-${GI_VERSION}.vapi
  LUA_OVERRIDES ${GI_NAMESPACE}.lua
  PYTHON_OVERRIDES ${GI_NAMESPACE}.py
  DEST_PREFIX .
  )

##
# Add the example gi python test
##
rift_unittest(example_gi_pytest
  TEST_ARGS python3 example_gi_pytest.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )

##
# Add the example gi lua test
##
rift_luatest(example_gi_lua
  TEST_ARGS example_gi_luatest.lua
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )

