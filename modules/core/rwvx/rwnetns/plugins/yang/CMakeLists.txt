# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Austin Cormier
# Creation Date: 12/23/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

##
# Parse the yang files
##
set(source_yang_files rw-netns-log.yang)

rift_add_yang_target(
  TARGET rwnetns_yang
  YANG_FILES ${source_yang_files}
  NO_LIB_COMPONENT
  COMPONENT ${INSTALL_COMPONENT}
  LIBRARIES
    rwschema_yang_gen
  )
