
# STANDARD_RIFT_IO_COPYRIGHT

include(rift_pkg_config)
# Note that the reaper is not using any rift libraries on
# purpose.  The reaper gets started in environments that do not
# yet have any RIFT software running at all.  Do not add any such
# dependencies to the reaper.


add_definitions(-std=gnu99)

add_library(reaper SHARED
  libreaper.c)
target_link_libraries(reaper PRIVATE
  msgpack)

add_library(reaper_client SHARED
  reaper_client.c)
target_link_libraries(reaper_client PRIVATE
  msgpack)

add_executable(reaperd reaperd.c)
target_link_libraries(reaperd PRIVATE
  reaper)

_install(TARGETS reaper reaper_client
  DESTINATION usr/${RIFT_LIBDIR}
  COMPONENT ${INSTALL_COMPONENT})

_install(TARGETS reaperd
  DESTINATION usr/${RIFT_BINDIR}
  COMPONENT ${INSTALL_COMPONENT})

install(FILES
  reaper_client.h
  DESTINATION usr/include/riftware
  COMPONENT ${INSTALL_COMPONENT})

add_subdirectory(test)
