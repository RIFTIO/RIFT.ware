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
# Author(s): Tom Seidenberg
# Creation Date: 2014/03/27
# 

rift_pkg_check_modules(GLIB REQUIRED glib-2.0>=2.42.0)
include_directories(BEFORE
  ${GOBJECT_INCLUDE_DIRS})

add_executable(yangpbc yangpbc_exe.cpp)
target_link_libraries(yangpbc PRIVATE
  rwlib
  rwyang
  ${GLIB_LIBRARIES})

install(TARGETS yangpbc DESTINATION usr/bin COMPONENT ${INSTALL_COMPONENT})

