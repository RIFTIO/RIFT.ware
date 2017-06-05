# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 2014/04/01
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

set(subdirs proto rwdtsrouter-c yang rwdtstasklet dtstestpub)
rift_add_subdirs(SUBDIR_LIST ${subdirs})
install(DIRECTORY lua-scripts DESTINATION usr/bin/ COMPONENT ${PKG_NAME})
