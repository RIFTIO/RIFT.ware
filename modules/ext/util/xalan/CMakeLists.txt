include(rift_externalproject)

set(dashd)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(PKG_DEBUG_FLAGS "CFLAGS=-g -O0")
  set(dashd "-d")
endif()

# xalan doesn't build in parallel 
rift_externalproject_add(
  xalan
  SOURCE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/xalan-c-1.11/c
  CONFIGURE_COMMAND
    ${PROJECT_TOP_DIR}/scripts/env/envset.sh
      XALANCROOT=${CMAKE_CURRENT_BINARY_DIR}/xalan/xalan-build
      XERCESCROOT=${RIFT_INSTALL}
      -e ./runConfigure -p linux -c gcc -x g++ -b 64 ${dashd} -P ${RIFT_INSTALL}
  BUILD_COMMAND
    ${PROJECT_TOP_DIR}/scripts/env/envset.sh
      XALANCROOT=${CMAKE_CURRENT_BINARY_DIR}/xalan/xalan-build
      XERCESCROOT=${RIFT_INSTALL}
      -e make -j1
  INSTALL_COMMAND
    ${PROJECT_TOP_DIR}/scripts/env/envset.sh
      XALANCROOT=${CMAKE_CURRENT_BINARY_DIR}/xalan/xalan-build
      XERCESCROOT=${RIFT_INSTALL}
      -e $(MAKE) install
  BCACHE_COMMAND
    ${PROJECT_TOP_DIR}/scripts/env/envset.sh
      XALANCROOT=${CMAKE_CURRENT_BINARY_DIR}/xalan/xalan-build
      XERCESCROOT=${RIFT_INSTALL}
      -e $(MAKE) DESTDIR=${RIFT_SUBMODULE_INSTALL_PREFIX}/xalan install
  DEPENDS
    xerces-c
)

