# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 11/22/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

## 
# This file serves as a basic template for building
# an executable target
##

##
# Specify the target name for the executable
##
set(FOO_TARGET foo_app)

##
# This appends to the list of directories to search for
# header files. INCLUDE_DIRECTORIES is a DIRECTORY property and
# is inherited from the parent directory.
#
# NOTE: The order of include directory search can be controlled
# by the keyword AFTER or BEFORE. In this case we specified
# BEFORE indicating that these directories need to be searched first.
# If the BEFORE/AFTER keyword is NOT specified it deafults to AFTER.
##
include_directories(
  BEFORE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib
  )

##
# Sepcify the list of source files
##
set(C_SOURCES foo_app.c)

##
# This creates an executable target
##
add_executable(${FOO_TARGET} ${C_SOURCES})

##
# Specify the libraries that foo app depends on
##
target_link_libraries(${FOO_TARGET} 
  ${CMAKE_CURRENT_BINARY_DIR}/../lib/libfoo.so)

##
# Specificy the targets that the executable target depends on
##
add_dependencies(${FOO_TARGET} foo)

##
# The following code installs the targets to 
# ${CMAKE_INSTALL_PREFIX}/usr/bin.
# Always use relative path for destination.
#
# NOTE: In some examples the install command may also be called 
# with COMPONENT label. This is for CPACK component based
# installation.
##
install(TARGETS ${FOO_TARGET} DESTINATION usr/bin)
