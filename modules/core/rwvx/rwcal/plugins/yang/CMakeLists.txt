# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# 

##
# Parse the yang files
##

include(rift_yang)

set(source_yang_files rwcal.yang)

rift_generate_python_log_yang(
    LOG_CATEGORY_NAME rw-cal-log
    START_EVENT_ID 63000
    OUT_YANG_FILE_VAR rw_cal_log_file
    )

rift_add_yang_target(
  TARGET rwcal_yang
  YANG_FILES
    ${source_yang_files}
    ${rw_cal_log_file}
  COMPONENT ${PKG_LONG_NAME}
  LIBRARIES
    rwschema_yang_gen
    rwyang
    rwlog
    rwlog-mgmt_yang_gen
)
