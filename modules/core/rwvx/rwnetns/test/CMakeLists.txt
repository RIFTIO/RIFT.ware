
# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#

include(rift_pkg_config)

add_definitions(-std=gnu99)

rift_pkg_check_modules(PROF REQUIRED libprofiler>=2.4)

# Simple wrapper around the library for profiling.
# strace -c speed_test
# valgrind --tool=callgrind speed_test
# CPUPROFILE_FREQUENCY=1000 CPUPROFILE=/tmp/netns.prof speed_test
add_executable(speed_test speed_test.c)
target_link_libraries(speed_test
  rwnetns
  ${PROF_LIBRARIES}
  pthread
)
