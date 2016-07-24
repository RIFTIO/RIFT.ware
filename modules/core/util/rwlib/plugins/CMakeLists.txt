# 
# (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
#
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# 

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
  lua_coroutine
  )
rift_add_subdirs(SUBDIR_LIST ${subdirs})
