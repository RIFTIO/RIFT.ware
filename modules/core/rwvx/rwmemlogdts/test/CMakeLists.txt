# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)

cmake_minimum_required(VERSION 2.8)

##
# Add the rwlog python test
##

rift_py3test(rwmemlogdts_gi_test
  TEST_ARGS ${CMAKE_CURRENT_SOURCE_DIR}/rwmemlogdts_gi_test.py
  TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS rwdts_api rwmemlog_gi rwdts_gi
)

add_custom_target(rwmemlogdts_unittest
  DEPENDS
  rwmemlogdts_gi_test 
  )

