# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Austin Cormier
# Creation Date: 01/04/2015
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)


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
  NO_LIB_COMPONENT
  COMPONENT ${INSTALL_COMPONENT}
  LIBRARIES
    rwschema_yang_gen
  )
