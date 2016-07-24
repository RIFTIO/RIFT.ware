include(rift_externalproject)
include(rift_pkg_config)

##
# Find the full path to the CLANG compiler. This path is used
# to pass CMAKE_C_COMPILER argument during the externalproject_add.
##
find_program(CLANG_COMPILER clang)
if(CLANG_COMPILER-NOTFOUND)
  message(FATAL_ERROR "Didn't find the CLANG compiler")
endif()

##
# Add the external project target for libdispatch
#
# NOTE: Pass the full path to the clang compiler. Passing a relative path
# causes the cmake to forget other command line arguments during the re-run.
# Specifically without the full path the following sequence will have an issue:
#   make; make clean; make
# cmake will forget the CMAKE_INSTALL_PREFIX passed through the command line
# during the make after the "make clean". This issue captured @
# http://public.kitware.com/Bug/view.php?id=14294
##
rift_externalproject_add(
  libdispatch
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libdispatch-1.0
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}/usr
    -DCMAKE_C_COMPILER=${CLANG_COMPILER}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DTOP_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
    -DCOREFOUNDATION_FOUND=FALSE
  INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/usr
  DEPENDS libkqueue libblocksruntime libpthread_workqueue
)

rift_make_pc_ext(libdispatch
  VERSION 1.0
  DESC "libdispatch"
  LIBS -ldispatch
  COMPONENT libdispatch)

set(subdirs test)
rift_add_subdirs(SUBDIR_LIST ${subdirs})
