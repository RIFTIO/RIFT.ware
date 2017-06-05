# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Baiju Parameswaran
# Creation Date: 5/8/2015
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

include(rift_yang)

##
# Parse the yang files
##
set(source_yang_files rw-dtsperf.yang rw-dtsperfmgr.yang)

rift_add_yang_target(
  TARGET rwdtsperf_yang
  YANG_FILES ${source_yang_files}
  NO_LIB_COMPONENT
  COMPONENT ${INSTALL_COMPONENT}
  LIBRARIES
    rwschema_yang_gen
    rwlog
    rwmanifest_yang_gen
    rw_vcs_yang_gen
  )

