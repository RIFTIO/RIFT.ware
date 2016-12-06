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
# Creation Date: 2014/05/12
# 

cmake_minimum_required(VERSION 2.8)

##
# Generate the protobuf file -- This will GO AWAY once we can do this via yang
# TODO: CLEAN THIS UP
##

set(proto_files rwtoytasklet.proto)
rift_add_proto_target(
  TARGET rwtoytasklet_proto_parse
  WITH_DSO
  PROTO_FILES ${proto_files}
  OUT_C_FILES_VAR proto_c_files
  COMPONENT ${INSTALL_COMPONENT}
)

##
# Add the target for the library
##
add_library(rwtoytaskletproto SHARED ${proto_c_files})
target_link_libraries(rwtoytaskletproto PRIVATE
  rwmsg
  protobuf-c)
install(TARGETS rwtoytaskletproto LIBRARY DESTINATION usr/lib
  COMPONENT ${INSTALL_COMPONENT})

