# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf
# Creation Date: 3/20/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

rift_pkg_check_modules(GLIB REQUIRED glib-2.0>=2.42.0)
#rift_pkg_check_modules(CFLITE REQUIRED CFLite>=0.1.2)
rift_pkg_check_modules(TALLOC REQUIRED talloc>=2.1.0)
rift_pkg_check_modules(RWYANG REQUIRED rwyang>=1.0)

include_directories(${GLIB_INCLUDE_DIRS} 
  ${TALLOC_INCLUDE_DIRS}
  ${RWYANG_INCLUDE_DIRS})


##
# Add flags to the compiler command line for sources in the 
# current directory and below
##
add_definitions(-DINSTALLDIR="${CMAKE_INSTALL_PREFIX}")

##
# Add the target for the rwhelloworld library
##
set(C_SOURCES 
  rwdtsperf.c
  rwdtsperf_mgmt.c
  rwdtsperf_dts.c
  rwdtsperf_dts_mgmt.c)

add_library(rwdtsperf STATIC ${C_SOURCES})

target_link_libraries(rwdtsperf
  rw_tasklet_plugin-1.0
  rwdts_api
  rwdtsperf_yang_gen
  rwtasklet
  ${GLIB_LIBRARIES}
  ${TALLOC_LIBRARIES}
  CoreFoundation
  ${RWYANG_LIBRARIES})

add_dependencies(rwdtsperf rwdtsperf_yang)
