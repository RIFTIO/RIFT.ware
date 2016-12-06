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
# Creation Date: 2014/01/31
# 

cmake_minimum_required(VERSION 2.8)

set(PKG_NAME yangtools)
set(PKG_VERSION 1.0)
set(PKG_RELEASE 1)
set(PKG_LONG_NAME ${PKG_NAME}-${PKG_VERSION})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")

rift_allow_compiler_warning(missing-field-initializers)
rift_allow_compiler_warning(unused-parameter)
rift_allow_compiler_warning(unused-but-set-variable)
#rift_allow_compiler_warning(sign-compare)

##
# documentation target
##
rift_add_doxygen_target(yangtools_doxygen DEST_DIR yangtools)

##
# Add the subdirectories.
##
set(subdirs proto lib yangpbc pb2c yang test plugins rwperson_database rwyangutil)
rift_add_subdirs(SUBDIR_LIST ${subdirs})

