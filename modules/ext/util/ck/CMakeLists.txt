include(rift_externalproject)

rift_externalproject_add(
  ck
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ck
  CONFIGURE_COMMAND
    ./configure --prefix=${CMAKE_INSTALL_PREFIX}/usr
)

