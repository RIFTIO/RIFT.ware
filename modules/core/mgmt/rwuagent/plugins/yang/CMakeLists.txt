# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tom Seidenberg
# Creation Date: 2014/04/08
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

##
# Yang targets
##
set(yang_files
    rw-mgmtagt.yang
    rw-mgmtagt-dts.yang
    rw-mgmtagt-log.yang
    rw-mgmtagt-composite.yang
   )

if(NOT RIFT_AGENT_BUILD STREQUAL "XML_ONLY")
  list(APPEND yang_files rw-mgmtagt-confd.yang)
endif()

rift_add_yang_target(
  TARGET
    rwuagent_yang
  YANG_FILES
    ${yang_files}
  LIBRARIES
    rwschema_yang_gen
    rwmanifest_yang_gen
    rwlog-mgmt_yang_gen
  NO_LIB_COMPONENT
  COMPONENT
    ${INSTALL_COMPONENT}
)

