# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

set(subdirs
  basic_plugin
  basic_functor
  example_gi
  vala_gi
  feature_plugin
  python_plugin
  rwarray_types
  rwplugin
  rwpython_util
  standard_plugin
  test_nonglobal_loader
#  lua_coroutine
  )
rift_add_subdirs(SUBDIR_LIST ${subdirs})
