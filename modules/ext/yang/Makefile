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
# Author(s): Tim Mortsolf
# Creation Date: 11/25/2013
# 

##
# Define a Makefile function: find_upwards(filename)
#
# Searches for a file of the given name in the directory ., .., ../.., ../../.., etc.,
# until the file is found or the root directory is reached
##
find_upward = $(word 1, $(shell while [ `pwd` != / ] ; do find `pwd` -maxdepth 1 -name $1 ; cd .. ; done))

##
# Call find_upward("Makefile.top") to find the nearest upwards adjacent Makefile.top
##
makefile.top := $(call find_upward, "Makefile.top")

##
# If Makefile.top was found, then include it
##
include $(makefile.top)
