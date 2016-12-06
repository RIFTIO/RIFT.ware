
# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
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
