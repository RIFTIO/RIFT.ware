
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
