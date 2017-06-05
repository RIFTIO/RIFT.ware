# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Creation Date: 2015/09/11
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

include(rift_yang)

set(source_yang_files
  rw-memlog.yang
)

rift_add_yang_target(
  TARGET rwmemlog_yang
  YANG_FILES ${source_yang_files}
  NO_LIB_COMPONENT 
  COMPONENT ${INSTALL_COMPONENT}
  LIBRARIES rwext_yang_gen  
)

rift_gen_yang_tree(rwmemlog-pyang-trees
  OUTFILE_PREFIX rw-memlog.yang
  YANG_FILES ${CMAKE_CURRENT_SOURCE_DIR}/rw-memlog.yang
)

