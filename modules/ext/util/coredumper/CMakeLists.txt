include(rift_externalproject)

rift_externalproject_add(
  coredumper
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/coredumper-1.2.1
  CONFIGURE_COMMAND
    ./configure --prefix=${CMAKE_INSTALL_PREFIX}/usr CPPFLAGS=-I${CMAKE_INSTALL_PREFIX}/usr/include
)

rift_make_pc_ext(coredumper
  VERSION 1.2.1
  DESC "Google Coredumper"
  LIBS -lcoredumper
  COMPONENT coredumper)

add_dependencies(coredumper libunwind)
