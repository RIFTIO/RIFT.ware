# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 11/22/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

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
