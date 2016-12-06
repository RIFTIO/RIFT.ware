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
# Creation Date: 11/22/2013
# 

cmake_minimum_required(VERSION 2.8)

## 
# This file serves as a basic template for building
# static and shared libraries
##

##
# Specify the target name for the library
##
set(FOO_LIB_TARGET foo)

##
# This specifies the list of directories to search for
# header files. This provides -I flags to the compiler.
##
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

##
# This specifies the list of directories to search for
# library files. This provides -L flags to the compiler.
# This current target doesn't need to search for any 
# libraries.
##
# link_directories(${CMAKE_CURRENT_BINARY_DIR})

##
# Sepcify the list of source files
##
set(C_SOURCES foo_lib.c)

##
# This creates a static library
##
# add_library(${FOO_LIB_TARGET} ${C_SOURCES})

##
# This creates a shared libarary
##
add_library(${FOO_LIB_TARGET} SHARED ${C_SOURCES})

##
# Specificy the targets that the library target depends on
# This target doesn't have any dependencies so the following
# line is commented out
##
# add_dependencies(${FOO_LIB_TARGET} ${deps})

##
# The following code installs the targets to 
# ${CMAKE_INSTALL_PREFIX}/usr/lib.
# Always use relative path for DESTINATION
#
# NOTE: In some examples the install command may also be called 
# with COMPONENT label. This is for CPACK component based
# installation.
##
install(TARGETS ${FOO_LIB_TARGET} DESTINATION usr/lib)

# use the archive keyword for static libraries
# install(TARGETS ${FOO_LIB_TARGET} ARCHIVE DESTINATION usr/lib)
