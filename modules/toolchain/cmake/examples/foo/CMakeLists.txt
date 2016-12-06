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
# Creation Date: 11/22/2013
# 

cmake_minimum_required(VERSION 2.8)

##
# To see this example at work follow these steps:
#
# mkdir build
# cd build
# cmake ../
# make
##

##
# The following code adds the subdirectories 
# for the compilation.
##
set(subdirs app lib)
foreach(subdir ${subdirs})
  add_subdirectory(${subdir})
endforeach(subdir)

