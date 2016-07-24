# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Austin Cormier
# Creation Date: 01/04/2015
# 

##
# Parse the yang files
##
rift_generate_python_log_yang(
  LOG_CATEGORY_NAME rw-generic-log
  START_EVENT_ID 6000
  OUT_YANG_FILE_VAR rw_generic_log_file
  )

rift_add_yang_target(
  TARGET rwgeneric_yang
  YANG_FILES ${rw_generic_log_file}
  COMPONENT ${PKG_LONG_NAME}
  LIBRARIES
    rwschema_yang_gen
  )
