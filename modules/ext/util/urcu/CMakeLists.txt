include(rift_externalproject)

rift_externalproject_add(
  urcu
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/urcu
  CONFIGURE_COMMAND
    ./configure --prefix=${CMAKE_INSTALL_PREFIX}/usr
)

externalproject_add_step(
  urcu
  autoreconf
  COMMAND autoreconf -f -i
  WORKING_DIRECTORY <BINARY_DIR>
  DEPENDERS configure
  DEPENDEES rsync_to_build
)
