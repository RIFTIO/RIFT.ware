# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Baiju Parameswaran
# Creation Date: 5/8/2015
# 

include(rift_yang)

##
# Parse the yang files
##
set(source_yang_files rw-dtsperf.yang rw-dtsperfmgr.yang)

rift_add_yang_target(
  TARGET rwdtsperf_yang
  YANG_FILES ${source_yang_files}
  COMPONENT ${PKG_LONG_NAME}
  LIBRARIES
    rwschema_yang_gen
    rwlog
    rwmanifest_yang_gen
    rw_vcs_yang_gen
  )

