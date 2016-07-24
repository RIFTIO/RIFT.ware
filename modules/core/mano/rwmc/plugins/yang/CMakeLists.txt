# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Austin Cormier
# Creation Date: 2015/08/01
# 

##
# Yang targets
##
rift_generate_python_log_yang(
    LOG_CATEGORY_NAME rw-mc-log
    START_EVENT_ID 120000
    OUT_YANG_FILE_VAR rw_mc_log_file
    )


rift_add_yang_target(
  TARGET rwmc_yang
  YANG_FILES 
    rw-mc.yang 
    ${rw_mc_log_file}
  LIBRARIES
    rwcloud_yang_gen
  DEPENDS
    rwcloud_yang
  COMPONENT ${PKG_LONG_NAME}
  LIBRARIES
     rwsdn_yang_gen
)

##
# Install the XML file
##
install(
  FILES
  ../cli/cli_rwmc.xml
  ../cli/cli_rwmc_schema_listing.txt
  DESTINATION usr/data/manifest
  COMPONENT ${PKG_LONG_NAME}
)

