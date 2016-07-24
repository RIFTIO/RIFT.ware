# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Tom Seidenberg
# Creation Date: 2014/04/08
# 

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
  COMPONENT
    ${PKG_LONG_NAME}
)

##
# Install the XML file
##
install(FILES ../cli/cli_rwuagent.xml DESTINATION usr/data/manifest COMPONENT ${PKG_LONG_NAME})

