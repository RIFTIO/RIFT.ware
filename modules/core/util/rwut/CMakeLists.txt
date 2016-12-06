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
# Author(s): Andrew Golovanov
# Creation Date: 4/3/2014
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME rwut)

# Install header files of the unit test framework
file(GLOB RWUT_HFILES include/riftware/*)
install(
  FILES ${RWUT_HFILES} 
  DESTINATION usr/include/rwut 
  COMPONENT ${INSTALL_COMPONENT}
)

##
# documentation target
##
set(doxy_files
  include/riftware/rwut.h
)

rift_add_doxygen_target(${PKG_NAME}_doxygen
  DOXY_NAME "RW Unit Test Framework"
  DOXY_FILES ${doxy_files}
  DEST_DIR ${PKG_NAME})


add_subdirectory(test)
