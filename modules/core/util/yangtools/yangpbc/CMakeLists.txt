# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Tom Seidenberg
# Creation Date: 2014/03/27
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

rift_pkg_check_modules(GLIB REQUIRED glib-2.0>=2.42.0)
include_directories(BEFORE
  ${GOBJECT_INCLUDE_DIRS})

add_executable(yangpbc yangpbc_exe.cpp)
target_link_libraries(yangpbc PRIVATE
  rwlib
  rwyang
  ${GLIB_LIBRARIES})

install(TARGETS yangpbc DESTINATION usr/bin COMPONENT ${INSTALL_COMPONENT})

