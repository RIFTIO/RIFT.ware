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
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# 

# this should be included by the CMakeLists.txt in the root directory
# of every git submodule

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git AND NOT ${RIFT_NON_SUBMODULE_PROJECT})
  message(FATAL_ERROR "Included rift_topdir but not in a submodule root directory")
endif()

include(rift_globals)

#cmake_policy(SET CMP0017 NEW)
