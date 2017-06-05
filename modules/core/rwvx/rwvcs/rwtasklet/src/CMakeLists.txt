
# STANDARD_RIFT_IO_COPYRIGHT

include(rift_pkg_config)

rift_pkg_check_modules(RWLIB REQUIRED rwlib>=1.0)

rift_pkg_check_modules(GOBJECT REQUIRED gobject-2.0>=2.42.0)
#rift_pkg_check_modules(CFLITE REQUIRED CFLite>=0.1.2)
#rift_pkg_check_modules(MSGPACK REQUIRED msgpack>=0.5.9)
rift_pkg_check_modules(PB_C REQUIRED libprotobuf-c>=1.1.0)

add_definitions(-std=gnu99 -D_GNU_SOURCE)
include_directories(
  ${RWLIB_INCLUDE_DIRS}
  ${GOBJECT_INCLUDE_DIRS}
  ${PB_C_INCLUDE_DIRS})

add_library(rwtasklet SHARED
  rwtasklet.c
  rwtasklet_var_root.c)

target_link_libraries(rwtasklet
  rwlog
  rwmsg
  rwsched
  rwvcs
  rwyang
  rwmanifest_yang_gen
  rw_vcs_yang_gen

  ${RWLIB_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  CoreFoundation
  ${PB_C_LIBRARIES})

install(TARGETS rwtasklet
  LIBRARY
  DESTINATION usr/lib
  COMPONENT ${INSTALL_COMPONENT})

rift_make_pc(rwtasklet
  VERSION 1.0
  REQUIRES_PRIVATE rwvcs
  COMPONENT ${INSTALL_COMPONENT})
