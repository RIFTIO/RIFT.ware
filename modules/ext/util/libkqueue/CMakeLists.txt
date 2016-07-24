include(rift_externalproject)

rift_externalproject_add(
  libkqueue
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libkqueue-2.0.1
  CONFIGURE_COMMAND
    ./configure --prefix=${CMAKE_INSTALL_PREFIX}/usr
)
