# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Austin Cormier
# Creation Date: 12/23/2014
# 

##
# Parse the yang files
##
set(source_yang_files rw-netns-log.yang)

rift_add_yang_target(
  TARGET rwnetns_yang
  YANG_FILES ${source_yang_files}
  COMPONENT ${PKG_LONG_NAME}
  LIBRARIES
    rwschema_yang_gen
  )
