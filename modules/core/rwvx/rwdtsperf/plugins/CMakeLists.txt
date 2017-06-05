# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf
# Creation Date: 2014/03/20
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

set(subdirs yang ${PKG_NAME}-c rwdtsperfmgrtasklet rwdtsperftasklet)
rift_add_subdirs(SUBDIR_LIST ${subdirs})
