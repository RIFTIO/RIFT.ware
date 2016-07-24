# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Creation Date: 2015/09/11
# 

include(rift_yang)

set(source_yang_files
  rw-memlog.yang
)

rift_add_yang_target(
  TARGET rwmemlog_yang
  YANG_FILES ${source_yang_files}
  COMPONENT ${PKG_LONG_NAME}
  LIBRARIES rwext_yang_gen  
)

rift_gen_yang_tree(rwmemlog-pyang-trees
  OUTFILE_PREFIX rw-memlog.yang
  YANG_FILES ${CMAKE_CURRENT_SOURCE_DIR}/rw-memlog.yang
)

