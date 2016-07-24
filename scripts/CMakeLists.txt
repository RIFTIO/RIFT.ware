
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#



add_subdirectory(cloud)


configure_file(
  rift-scripts.sh.in
  ${CMAKE_CURRENT_BINARY_DIR}/rift-scripts.sh
  ESCAPE_QUOTES @ONLY
  )

set(BUILD 9999)

execute_process(COMMAND cat ../RELEASE OUTPUT_VARIABLE TEMP)

string(STRIP ${TEMP} RELEASE)

add_custom_target(rpmbuild DEPENDS rift_scripts-${RELEASE}-${BUILD}.fc20.noarch.rpm )

add_custom_command(
  OUTPUT rift_scripts-${RELEASE}-${BUILD}.fc20.noarch.rpm
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/rift-scripts.sh ${RELEASE} ${BUILD}
  COMMENT "Building a noarch scripts RPM")


