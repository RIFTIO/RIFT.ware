# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tim Mortsolf
# Creation Date: 1/23/2014
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)
include(rift_pkg_config)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format -Wno-main -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-label -Wno-unused-variable -Wno-implicit-function-declaration -Wno-return-type -Wno-missing-field-initializers")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format -Wno-main -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-label -Wno-unused-variable -Wno-return-type -Wno-missing-field-initializers")

##
# Add the target for plconvert
##

add_executable(plconvert plconvert.c)
target_link_libraries(plconvert CoreFoundation)
add_dependencies(plconvert cflite)

##
# Add the target for CFRunLoopTimerExample
##

add_executable(CFRunLoopTimerExample CFRunLoopTimerExample.c)
target_link_libraries(CFRunLoopTimerExample CoreFoundation m)
add_dependencies(CFRunLoopTimerExample cflite)

##
# Add the target for CFRunLoopSignalExample
##

add_executable(CFRunLoopSignalExample CFRunLoopSignalExample.c)
target_link_libraries(CFRunLoopSignalExample CoreFoundation m)
add_dependencies(CFRunLoopSignalExample cflite)

##
# Add the target for CFArray
##

add_executable(CFArray CFArray.c)
target_link_libraries(CFArray CoreFoundation m)
add_dependencies(CFArray cflite)

##
# Add the target for CFMutableArray
##

add_executable(CFMutableArray CFMutableArray.c)
target_link_libraries(CFMutableArray CoreFoundation m)
add_dependencies(CFMutableArray cflite)

##
# Add the target for CFTree
##

add_executable(CFTree CFTree.c)
target_link_libraries(CFTree CoreFoundation m)
add_dependencies(CFTree cflite)

##
# Add the target for CFSet
##

add_executable(CFSet CFSet.c)
target_link_libraries(CFSet CoreFoundation m)
add_dependencies(CFSet cflite)

##
# Add the target for CFPropertyList
##

add_executable(CFPropertyList CFPropertyList.c)
target_link_libraries(CFPropertyList CoreFoundation m)
add_dependencies(CFPropertyList cflite)

##
# Add the target for CFAllocator
##

add_executable(CFAllocator CFAllocator.c)
target_link_libraries(CFAllocator CoreFoundation m)
add_dependencies(CFAllocator cflite)

##
# Converted unit test target (runs with GTest and Celero)
##
rift_allow_compiler_warning(unused-value)

rift_pkg_check_modules(GLIB REQUIRED glib-2.0>=2.42.0)
include_directories(BEFORE
  ${GLIB_INCLUDE_DIRS}
  ${TALLOC_INCLUDE_DIRS})


rift_gtest(rw_cf_type_validate_test
  ADD_SETUP             # uncomment to add a setup procedure into main() of the func testing
  TEST_SRCS rw_cf_type_validate_test.cpp rw_cf_type_list.c
  TEST_LIBS
    rwlib
    ${GLIB_LIBRARIES}
    talloc

    # RIFT-5092 - this shoul dbe using rift_pkg_check_modules
    ${CMAKE_INSTALL_PREFIX}/usr/lib/libCoreFoundation.so
  DEPENDS cflite rwlib
)
set_target_properties(rw_cf_type_validate_test PROPERTIES COMPILE_DEFINITIONS "_CF_")

##
# Add the target for myCFType_test
##

set(C_SOURCES myCFType_test.c myCFType01.c myCFType02.c)
add_executable(myCFType_test ${C_SOURCES})
target_link_libraries(myCFType_test CoreFoundation m)
add_dependencies(myCFType_test cflite)
