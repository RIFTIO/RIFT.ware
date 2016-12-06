# 
#   Copyright 2016 RIFT.IO Inc
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
# Author(s): Anil Gunturu
# Creation Date: 8/29/2013
# 

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

