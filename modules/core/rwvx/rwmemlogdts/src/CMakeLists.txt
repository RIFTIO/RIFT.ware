# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Creation Date: 9/11/15
# 

##
# Add a shared libarary target
##

set(
  C_SOURCES
    rwmemlogdts.c 
)

add_library(
  rwmemlogdts
  OBJECT
    ${C_SOURCES}
    ${auto_c_files}
)

include_directories(BEFORE
${RIFT_SUBMODULE_SOURCE_ROOT}/rwmsg/include
${RIFT_SUBMODULE_SOURCE_ROOT}/rwdts/include
${RIFT_SUBMODULE_SOURCE_ROOT}/rwdtsperf/include
${RIFT_SUBMODULE_SOURCE_ROOT}/rwsched/src
${RIFT_SUBMODULE_SOURCE_ROOT}/rwschema/src
${RIFT_SUBMODULE_BINARY_ROOT}/rwdts/plugins/proto
)

add_dependencies(
  rwmemlogdts
  rwmemlog
  rwdtsproto
  rwdts_yang.headers
  rwgeneric_yang.headers
  rwschema_yang.headers
  rwlog_event_auto
  process_rwlog_evt_files
)

##
# Install the header files and targets
##
set(INSTALL_HFILES
  ${RIFT_SUBMODULE_SOURCE_ROOT}/rwmemlogdts/include/riftware/rwmemlogdts.h
)

install(FILES ${INSTALL_HFILES} DESTINATION usr/include COMPONENT ${PKG_LONG_NAME})
install(FILES ${auto_h_files} DESTINATION usr/include/rwmemlogdts COMPONENT ${PKG_LONG_NAME})
# install(TARGETS rwmemlogdts LIBRARY DESTINATION usr/lib COMPONENT ${PKG_LONG_NAME})

##
# documentation target
##
rift_add_doxygen_target(${PKG_NAME}_doxygen
  DOXY_NAME "RW.MemlogDts QuickStart and API Reference"
  DOXY_FILES 
  ../include/riftware/memlogdts.h 
  DEST_DIR ${PKG_NAME})
