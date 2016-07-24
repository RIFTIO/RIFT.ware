
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

include(rift_pkg_config)
# Note that the reaper is not using any rift libraries on
# purpose.  The reaper gets started in environments that do not
# yet have any RIFT software running at all.  Do not add any such
# dependencies to the reaper.


add_definitions(-std=gnu99)

rift_pkg_check_modules(MSGPACK REQUIRED msgpack>=0.5.9)

include_directories(
  ${MSGPACK_INCLUDE_DIRS})

add_library(reaper SHARED
  libreaper.c)
target_link_libraries(reaper PRIVATE
  ${MSGPACK_LIBRARIES})

add_library(reaper_client SHARED
  reaper_client.c)
target_link_libraries(reaper_client PRIVATE
  ${MSGPACK_LIBRARIES})

add_executable(reaperd reaperd.c)
target_link_libraries(reaperd PRIVATE
  reaper)

_install(TARGETS reaper reaper_client
  DESTINATION usr/${RIFT_LIBDIR}
  COMPONENT rwvcs-1.0)

_install(TARGETS reaperd
  DESTINATION usr/${RIFT_BINDIR}
  COMPONENT rwvcs-1.0)

install(FILES
  reaper_client.h
  DESTINATION usr/include/riftware
  COMPONENT rwvcs-1.0)

add_subdirectory(test)
