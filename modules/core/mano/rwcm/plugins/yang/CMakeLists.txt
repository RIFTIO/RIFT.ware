# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Manish Patel
# Creation Date: 10/28/2015
# 

##
# Yang targets
##

rift_generate_python_log_yang(
    LOG_CATEGORY_NAME rw-conman-log
    START_EVENT_ID 66000
    OUT_YANG_FILE_VAR rw_conman_log_file
    )

rift_add_yang_target(
  TARGET rw_conman_yang
  YANG_FILES rw-conman.yang ${rw_conman_log_file}
  COMPONENT ${PKG_LONG_NAME}
  LIBRARIES
    mano_yang_gen
    rwconfig_agent_yang_gen
  DEPENDS
    mano_yang
    rwconfig_agent_yang
)

##
# Install the XML file
##
install(
  FILES ../cli/cli_rwcm.xml
  DESTINATION usr/data/manifest
  COMPONENT ${PKG_LONG_NAME}
)

