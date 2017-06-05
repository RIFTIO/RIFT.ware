# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tom Seidenberg
# Creation Date: 2014/01/31
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

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

