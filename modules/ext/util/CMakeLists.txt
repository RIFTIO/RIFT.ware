
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

cmake_minimum_required(VERSION 2.8)

include(rift_submodule)

# These should not be detected at runtime.  Doing so means we
# can't build VM images from jenkins or developer workstations
# where the kernel has been updated.
set(FPATH_KERNEL_VERSION 3.12.9-301.fc20.x86_64)
set(FPATH_KERNEL_SRC /lib/modules/3.12.9-301.fc20.x86_64/build)


set(subdirs
  gperftools
  stream
  )

if ($ENV{RIFT_PLATFORM} STREQUAL "fc20")
   list(APPEND subdirs dpdk vtune)
endif()

rift_add_subdirs(SUBDIR_LIST ${subdirs})

rift_add_submodule_targets(
  SUBMODULE_PACKAGE_NAME "rw.ext.util"
  DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/package-description.txt"
  DESCRIPTION_SUMMARY "RIFTware Ext Util Package"
  )
